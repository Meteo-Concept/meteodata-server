/**
 * @file barani_thermohygro_message.cpp
 * @brief Implementation of the BaraniThermohygroMessage class
 * @author Laurent Georget
 * @date 2023-08-10
 */
/*
 * Copyright (C) 2023  SAS JD Environnement <contact@meteo-concept.fr>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <string>
#include <sstream>
#include <vector>
#include <cmath>

#include <cassandra.h>
#include <dbconnection_observations.h>
#include <observation.h>
#include <systemd/sd-daemon.h>

#include "barani/barani_thermohygro_message.h"
#include "davis/vantagepro2_message.h"
#include "cassandra_utils.h"
#include "hex_parser.h"

namespace meteodata
{

namespace chrono = std::chrono;
namespace json = boost::json;

BaraniThermohygroMessage::BaraniThermohygroMessage(DbConnectionObservations& db):
	_db{db}
{}

void BaraniThermohygroMessage::ingest(const CassUuid& station, const std::string& payload, const date::sys_seconds& datetime)
{
	using namespace std::chrono;
	using namespace hex_parser;

	if (!validateInput(payload, 22)) {
		_obs.valid = false;
		return;
	}

	_obs.time = datetime;

	// parse and fill in the obs
	_obs.valid = true;

	std::istringstream is{payload};

	// store the numbers on 16-bit integers to ensure the bit manipulations below never cause overflow
	std::vector<uint16_t> raw(11);
	for (int byte=0 ; byte<10 ; byte++) {
		is >> parse(raw[byte], 2, 16);
	}


	time_t lastUpdate;
	int previousClicks;
	bool result = _db.getCachedInt(station, BARANI_RAINFALL_CACHE_KEY, lastUpdate, previousClicks);
	std::optional<int> prev = std::nullopt;
	if (result && chrono::_V2::system_clock::from_time_t(lastUpdate) > chrono::_V2::system_clock::now() - 24h) {
		// the last rainfall datapoint is not too old, we can use
		// it as a reference for the current number of clicks recorded
		// by the pluviometer
		prev = previousClicks;
	}

	// bytes 0-1: message type, must be 1 for now
	_obs.messageType = (raw[0] & 0b1100'0000) >> 6;
	if (_obs.messageType != 1) {
		_obs.valid = false;
		return;
	}
	// bytes 2-6: battery, resolution 0.05V, offset 3V
	uint16_t battery = (raw[0] & 0b0011'1110) >> 1;
	_obs.batteryVoltage = battery == 0b1'1111 ? NAN : (3 + battery * 0.05f);
	// bytes 7-17: temperature, resolution 0.1°C, offset -100°C
	uint16_t temperature = ((raw[0] & 0b0000'0001) << 10) + (raw[1] << 2) + ((raw[2] & 0b1100'0000) >> 6);
	_obs.temperature = temperature == 0b111'1111'1111 ? NAN : (-100 + temperature * 0.1f);
	// bytes 18-23: min temperature, resolution 0.1°C, subtracted from temperature
	uint16_t minTemp = (raw[2] & 0b0011'1111);
	_obs.minTemperature = minTemp == 0b11'1111 ? NAN : (-100 + temperature - minTemp) * 0.1f;
	// bytes 24-29: max temperature, resolution 0.1°C, added to temperature
	uint16_t maxTemp = (raw[3] & 0b1111'1100) >> 2;
	_obs.maxTemperature = maxTemp == 0b11'1111 ? NAN : (-100 + temperature + maxTemp) * 0.1f;
	// bytes 30-38: humidity, resolution 0.2%, offset 0
	uint16_t humidity = ((raw[3] & 0b0000'0011) << 7) + ((raw[4] & 0b1111'1110) >> 1);
	_obs.humidity = humidity == 0b111'1111 ? NAN : humidity * 0.2f;
	// bytes 39-52: atmospheric absolute pressure, resolution 5Pa, offset 50000Pa
	uint16_t pressure = ((raw[4] & 0b0000'0001) << 13) + (raw[5] << 5) + ((raw[6] & 0b1111'1000) >> 3);
	_obs.pressure = pressure == 0b11'1111'1111'1111 ? NAN : seaLevelPressure((pressure * 5 + 50000) * 0.01f, _obs.temperature, _obs.humidity);
	// bytes 53-62: global radiation, resolution 2W/m², offset 0W/m²
	uint16_t radiation = ((raw[6] & 0b0000'0111) << 7) + ((raw[7] & 0b1111'1110) >> 1);
	_obs.radiation = radiation == 0b11'1111'1111 ? NAN : radiation * 0.2f;
	// bytes 63-71: max global radiation, resolution 2W/m², added to radiation
	uint16_t maxRadiation = ((raw[7] & 0b0000'0001) << 8) + raw[8];
	_obs.maxRadiation = maxRadiation == 0b1'1111'1111 ? NAN : (radiation + maxRadiation) * 0.2f;
	// bytes 72-79: rainfall clicks, resolution dependent on rain gauge, set to 0.2mm by default
	uint16_t rainClicks = raw[9];
	_obs.rainfallClicks = rainClicks;
	if (prev) {
		if (rainClicks > *prev) {
			_obs.rainfall = (rainClicks - *prev) * DEFAULT_RAIN_GAUGE_RESOLUTION;
		} else {
			_obs.rainfall = (4096 - *prev + rainClicks ) * DEFAULT_RAIN_GAUGE_RESOLUTION;
		}
	}
	// bytes 81-89: min time between clicks
	uint16_t minTimeBetweenClicks = raw[10];
	_obs.minTimeBetweenClicks = minTimeBetweenClicks;
	_obs.maxRainrate = _obs.minTimeBetweenClicks ? (DEFAULT_RAIN_GAUGE_RESOLUTION / (minTimeBetweenClicks / 3600.f)) : 0;
}

void BaraniThermohygroMessage::cacheValues(const CassUuid& station)
{
	if (_obs.valid) {
		int ret = _db.cacheInt(station, BARANI_RAINFALL_CACHE_KEY, chrono::system_clock::to_time_t(_obs.time), _obs.rainfallClicks);
		if (!ret)
			std::cerr << SD_ERR << "[MQTT " << station << "] management: "
				  << "Couldn't update the rainfall number of clicks, accumulation error possible"
				  << std::endl;
	}
}

Observation BaraniThermohygroMessage::getObservation(const CassUuid& station) const
{
	Observation result;

	if (_obs.valid) {
		result.station = station;
		result.day = date::floor<date::days>(_obs.time);
		result.time = _obs.time;
		result.outsidetemp = { _obs.temperature != NAN, _obs.temperature };
		result.max_outside_temperature = { _obs.maxTemperature != NAN, _obs.maxTemperature };
		result.min_outside_temperature = { _obs.minTemperature != NAN, _obs.minTemperature };
		result.outsidehum = { _obs.humidity != NAN, _obs.humidity };
		result.barometer = { _obs.pressure != NAN, _obs.pressure };
		result.solarrad = { _obs.radiation != NAN, _obs.radiation };
		result.rainfall = { _obs.rainfall != NAN, _obs.rainfall };
		result.rainrate = { _obs.maxRainrate != NAN, _obs.maxRainrate };
	}

	return result;
}

json::object BaraniThermohygroMessage::getDecodedMessage() const
{
	return json::object{
		{ "model", "barani_meteohelix_20230810" },
		{ "value", {
			{ "message_type", _obs.messageType },
			{ "battery_voltage", _obs.batteryVoltage },
			{ "temperature", _obs.temperature },
			{ "min_temperature", _obs.minTemperature },
			{ "max_temperature", _obs.maxTemperature },
			{ "humidity", _obs.humidity },
			{ "atmospheric_absolute_pressure", _obs.pressure },
			{ "global_radiation", _obs.radiation },
			{ "max_global_radiation", _obs.maxRadiation },
			{ "rainfall_clicks",_obs.rainfallClicks },
			{ "min_time_between_clicks", _obs.minTimeBetweenClicks },
			{ "max_rainrate", _obs.maxRainrate }
		} }
	};
}

}
