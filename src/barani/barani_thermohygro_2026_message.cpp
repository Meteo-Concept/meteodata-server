/**
 * @file barani_thermohygro_message.cpp
 * @brief Implementation of the BaraniThermohygro2026Message class
 * @author Laurent Georget
 * @date 2026-02-25
 */
/*
 * Copyright (C) 2026  SAS JD Environnement <contact@meteo-concept.fr>
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
#include <cassobs/dbconnection_observations.h>
#include <cassobs/observation.h>
#include <systemd/sd-daemon.h>

#include "barani/barani_thermohygro_2026_message.h"
#include "davis/vantagepro2_message.h"
#include "cassandra_utils.h"
#include "hex_parser.h"

namespace meteodata
{

namespace chrono = std::chrono;
namespace json = boost::json;

const std::string BaraniThermohygro2026Message::BARANI_LAST_BATTERY = "meteohelix_battery";

BaraniThermohygro2026Message::BaraniThermohygro2026Message(DbConnectionObservations& db):
	_db{db}
{}

void BaraniThermohygro2026Message::ingest(const CassUuid& station, const std::string& payload, const date::sys_seconds& datetime)
{
	using namespace std::chrono;
	using namespace hex_parser;

	if (!validateInput(payload, 32)) {
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

	float latitude, longitude;
	int altitude;
	std::string name;
	int pollPeriod;
	_db.getStationCoordinates(station, latitude, longitude, altitude, name, pollPeriod);

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
	int previousCorrectionClicks;
	result = _db.getCachedInt(station, BARANI_RAINCORR_CACHE_KEY, lastUpdate, previousCorrectionClicks);
	std::optional<int> prevCorr = std::nullopt;
	if (result && chrono::_V2::system_clock::from_time_t(lastUpdate) > chrono::_V2::system_clock::now() - 24h) {
		prevCorr = previousCorrectionClicks;
	}

	int knownBattery = 33;
	_db.getCachedInt(station, BARANI_LAST_BATTERY, lastUpdate, knownBattery);

	// bits 0-7: index
	_obs.index = raw[0];
	// bits 8: battery, resolution 0.05V, offset 3V
	uint16_t battery = (raw[1] & 0b1000'0000) >> 7;
	int newBattery = 33 + (_obs.index % 10) * 2 - (_obs.index % 10 > 4) * 10;
	if (_obs.batteryVoltage && newBattery > knownBattery) {
		knownBattery = newBattery + 1;
		_obs.batteryVoltage = knownBattery / 10.f;
	} else if (!_obs.batteryVoltage && newBattery < knownBattery) {
		knownBattery = newBattery - 1;
		_obs.batteryVoltage = knownBattery / 10.f;
	}
	_obs.batteryVoltage = std::clamp(knownBattery, 32, 42) / 10.f;
	if (!_db.cacheInt(station, BARANI_LAST_BATTERY, chrono::system_clock::to_time_t(datetime), knownBattery)) {
		std::cerr << SD_ERR << "[Liveobjects " << station << "] protocol: "
			  << "Failed to cache the battery known state for station " << station << std::endl;
	}
	_obs.batteryVoltage = battery == 0b1'1111 ? NAN : (3 + battery * 0.05f);
	// bits 9-22: temperature, resolution 0.01°C, offset -50°C
	uint16_t temperature = ((raw[1] & 0b0111'1111) << 7) + ((raw[2] & 0b1111'1110) >> 1);
	_obs.temperature = temperature == 0b11'1111'1111'1111 ? NAN : -50 + temperature * 0.01f;
	// bits 23-30: min temperature, resolution 0.05°C, subtracted from temperature
	uint16_t minTemp = ((raw[2] & 0b0000'0001) << 7) + ((raw[3] & 0b1111'1110) >> 1);
	_obs.minTemperature = minTemp == 0b1111'1111 ? NAN : _obs.temperature - (minTemp * 0.05f);
	// bits 31-38: max temperature, resolution 0.05°C, added to temperature
	uint16_t maxTemp = ((raw[3] & 0b0000'0001) << 7) + ((raw[4] & 0b1111'1110) >> 1);
	_obs.maxTemperature = maxTemp == 0b11'1111 ? NAN : _obs.temperature + (maxTemp * 0.05f);
	// bits 39-47: humidity, resolution 0.2%, offset 0%
	uint16_t humidity = ((raw[4] & 0b0000'0001) << 8) + raw[5];
	_obs.humidity = humidity == 0b1'1111'1111 ? NAN : humidity * 0.2f;
	// bits 48-62: atmospheric absolute pressure, resolution 2.5Pa, offset 30000Pa
	uint16_t pressure = (raw[6] << 7) + ((raw[7] & 0b1111'1110) >> 1);
	_obs.pressure = pressure == 0b111'1111'1111'1111 ? NAN : seaLevelPressureFromAltitude((pressure * 2.5 + 30000) * 0.01f, altitude, _obs.temperature);
	// bits 63-73: global radiation, resolution 1W/m², offset 0W/m²
	uint16_t radiation = ((raw[7] & 0b0000'0001) << 10) + (raw[8] << 2) + ((raw[9] & 0b1100'0000) >> 6);
	_obs.radiation = radiation == 0b111'1111'1111 ? NAN : radiation * 1.f;
	// bits 74-83: min global radiation, resolution 2W/m², offset 0W/m²
	uint16_t minRadiation = ((raw[9] & 0b0011'1111) << 4) + ((raw[10] & 0b1111'0000) >> 4);
	_obs.minRadiation = minRadiation == 0b11'1111'1111 ? NAN : minRadiation * 2.f;
	// bits 84-93: min global radiation, resolution 2W/m², offset 0W/m²
	uint16_t maxRadiation = ((raw[10] & 0b0000'1111) << 6) + ((raw[11] & 0b1111'1100) >> 2);
	_obs.maxRadiation = maxRadiation == 0b11'1111'1111 ? NAN : maxRadiation * 2.f;
	// bits 94-105: rainfall clicks, resolution dependent on rain gauge, set to 0.2mm by default
	uint16_t rainClicks = ((raw[11] & 0b0000'0011) << 10) + (raw[12] << 2) + ((raw[13] & 0b1100'0000) >> 6);
	_obs.rainfallClicks = rainClicks;
	if (prev) {
		if (rainClicks >= *prev) {
			_obs.rainfall = (rainClicks - *prev) * DEFAULT_RAIN_GAUGE_RESOLUTION;
		} else {
			_obs.rainfall = (4096 - *prev + rainClicks) * DEFAULT_RAIN_GAUGE_RESOLUTION;
		}
	}
	// bits 106-115: min time between clicks
	uint16_t minTimeBetweenClicks = ((raw[13] & 0b0011'1111) << 4) + ((raw[14] & 0b1111'0000) >> 4);
	_obs.minTimeBetweenClicks = std::pow(728.f / minTimeBetweenClicks, 2);
	_obs.maxRainrate = minTimeBetweenClicks ? (DEFAULT_RAIN_GAUGE_RESOLUTION / (_obs.minTimeBetweenClicks / 3600.f)) : 0;
	// bits 116-125: rain intensity correction
	uint16_t rainIntensityCorrection = ((raw[14] & 0b0000'1111) << 6) + ((raw[15] & 0b1111'1100) >> 2);
	_obs.intensityCorrection = rainIntensityCorrection;
	if (prevCorr) {
		if (rainIntensityCorrection >= *prevCorr) {
			_obs.intensityCorrection = (rainIntensityCorrection - *prevCorr) * DEFAULT_RAIN_GAUGE_RESOLUTION;
		} else {
			_obs.intensityCorrection = (4096 - *prevCorr + rainIntensityCorrection) * DEFAULT_RAIN_GAUGE_RESOLUTION;
		}
	}
	// bit 126: heater activation
	uint16_t heaterActivated = (raw[15] & 0b0000'0010) >> 1;
	_obs.heaterActivated = heaterActivated;
	// bit 127: alarm
	uint16_t alarm = raw[15] & 0b0000'0001;
	_obs.alarmSent = alarm;
}

void BaraniThermohygro2026Message::cacheValues(const CassUuid& station)
{
	if (_obs.valid) {
		int ret = _db.cacheInt(station, BARANI_RAINFALL_CACHE_KEY, chrono::system_clock::to_time_t(_obs.time), _obs.rainfallClicks);
		if (!ret)
			std::cerr << SD_ERR << "[MQTT " << station << "] management: "
				  << "Couldn't update the rainfall number of clicks, accumulation error possible"
				  << std::endl;
		ret = _db.cacheInt(station, BARANI_RAINCORR_CACHE_KEY, chrono::system_clock::to_time_t(_obs.time), _obs.intensityCorrection);
		if (!ret)
			std::cerr << SD_ERR << "[MQTT " << station << "] management: "
				  << "Couldn't update the rain correction number of clicks, accumulation error possible"
				  << std::endl;
	}
}

Observation BaraniThermohygro2026Message::getObservation(const CassUuid& station) const
{
	Observation result;

	if (_obs.valid) {
		result.station = station;
		result.day = date::floor<date::days>(_obs.time);
		result.time = _obs.time;
		result.outsidetemp = { !std::isnan(_obs.temperature), _obs.temperature };
		result.max_outside_temperature = { !std::isnan(_obs.maxTemperature), _obs.maxTemperature };
		result.min_outside_temperature = { !std::isnan(_obs.minTemperature), _obs.minTemperature };
		result.outsidehum = { !std::isnan(_obs.humidity), _obs.humidity };
		if (!std::isnan(_obs.temperature) && !std::isnan(_obs.humidity)) {
			result.dewpoint = {true, dew_point(_obs.temperature, _obs.humidity)};
			result.heatindex = {true, heat_index(from_Celsius_to_Farenheit(_obs.temperature), _obs.humidity)};
		}
		result.barometer = { !std::isnan(_obs.pressure), _obs.pressure };
		result.solarrad = { !std::isnan(_obs.radiation), _obs.radiation };
		result.rainfall = { !std::isnan(_obs.rainfall), _obs.rainfall };
		result.rainrate = { !std::isnan(_obs.maxRainrate), _obs.maxRainrate };
		result.voltage_battery = { !std::isnan(_obs.batteryVoltage), _obs.batteryVoltage };
	}

	return result;
}

json::object BaraniThermohygro2026Message::getDecodedMessage() const
{
	return json::object{
		{ "model", "barani_meteohelix_v2026_20260225" },
		{ "value", {
			{ "index", _obs.index },
			{ "battery_voltage", _obs.batteryVoltage },
			{ "temperature", _obs.temperature },
			{ "min_temperature", _obs.minTemperature },
			{ "max_temperature", _obs.maxTemperature },
			{ "humidity", _obs.humidity },
			{ "atmospheric_absolute_pressure", _obs.pressure },
			{ "global_radiation", _obs.radiation },
			{ "max_global_radiation", _obs.maxRadiation },
			{ "min_global_radiation", _obs.minRadiation },
			{ "rainfall_clicks",_obs.rainfallClicks },
			{ "min_time_between_clicks", _obs.minTimeBetweenClicks },
			{ "max_rainrate", _obs.maxRainrate },
			{ "rain_intensity_correction", _obs.intensityCorrection }
		} }
	};
}

}
