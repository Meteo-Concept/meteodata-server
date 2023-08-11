/**
 * @file barani_rain_gauge_message.cpp
 * @brief Implementation of the BaraniRainGaugeMessage class
 * @author Laurent Georget
 * @date 2022-03-24
 */
/*
 * Copyright (C) 2022  SAS JD Environnement <contact@meteo-concept.fr>
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

#include "barani_rain_gauge_message.h"
#include "cassandra_utils.h"
#include "hex_parser.h"

namespace meteodata
{

namespace chrono = std::chrono;
namespace json = boost::json;

BaraniRainGaugeMessage::BaraniRainGaugeMessage(DbConnectionObservations& db):
	_db{db}
{}

void BaraniRainGaugeMessage::ingest(const CassUuid& station, const std::string& payload, const date::sys_seconds& datetime)
{
	using namespace std::chrono;
	using namespace hex_parser;

	if (!validateInput(payload, 12)) {
		_obs.valid = false;
		return;
	}

	_obs.time = datetime;

	// parse and fill in the obs
	_obs.valid = true;

	std::istringstream is{payload};

	// store the numbers on 16-bit integers to ensure the bit manipulations below never cause overflow
	std::vector<uint16_t> raw(10);
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

	int previousCorrectionClicks;
	result = _db.getCachedInt(station, BARANI_RAINFALL_CORRECTION_CACHE_KEY, lastUpdate, previousCorrectionClicks);
	std::optional<int> prevCorr = std::nullopt;
	if (result && chrono::_V2::system_clock::from_time_t(lastUpdate) > chrono::_V2::system_clock::now() - 24h) {
		// the last rainfall datapoint is not too old, we can use
		// it as a reference for the current number of clicks recorded
		// by the pluviometer
		prevCorr = previousCorrectionClicks;
	}

	// bytes 0-7: index
	_obs.index = raw[0];
	// bytes 8-12: battery, resolution 0.05V, offset 3V
	uint16_t battery = (raw[1] & 0b1111'1000) >> 3;
	_obs.batteryVoltage = battery == 0b1'1111 ? NAN : (3 + battery * 0.05f);
	// bytes 13-24: rainfall, in number of clicks
	uint16_t rainClicks = ((raw[1] & 0b0000'0111) << 11) + (raw[2] << 1) + ((raw[3] & 0b1000'0000) >> 7);
	_obs.rainfallClicks = rainClicks;
	if (prev) {
		if (rainClicks > *prev) {
			_obs.rainfall = (rainClicks - *prev) * BARANI_RAIN_GAUGE_RESOLUTION;
		} else {
			_obs.rainfall = (4096 - *prev + rainClicks ) * BARANI_RAIN_GAUGE_RESOLUTION;
		}
	}
	// bytes 25-32: min time between clicks
	uint16_t minTimeBetweenClicks = ((raw[3] & 0b0111'1111) << 1) + ((raw[4] & 0b1000'0000) >> 7);
	_obs.minTimeBetweenClicks = minTimeBetweenClicks;
	_obs.maxRainrate = BARANI_RAIN_GAUGE_RESOLUTION / (182.f / minTimeBetweenClicks);
	// byte 33: internal temperature over 2°C, 0=FALSE, 1=TRUE
	_obs.tempOver2C = raw[4] & 0b0100'0000;
	// byte 34: heater status, 0=OFF, 1=ON
	_obs.heaterSwitchedOn = raw[4] & 0b0010'0000;
	// byte 35-46: rain correction, in number of clicks
	uint16_t rainCorrectionClicks = ((raw[4] & 0b0001'1111) << 5) + ((raw[5] & 0b1111'1110) >> 1);
	_obs.correction = rainCorrectionClicks;
	if (prevCorr) {
		if (rainCorrectionClicks > *prevCorr) {
			_obs.rainfall += (rainCorrectionClicks - *prevCorr) * 0.01f * BARANI_RAIN_GAUGE_RESOLUTION;
		} else {
			_obs.rainfall += (4096 - *prevCorr + rainCorrectionClicks) * 0.01f * BARANI_RAIN_GAUGE_RESOLUTION;
		}
	}
}

void BaraniRainGaugeMessage::cacheValues(const CassUuid& station)
{
	if (_obs.valid) {
		int ret = _db.cacheInt(station, BARANI_RAINFALL_CACHE_KEY, chrono::system_clock::to_time_t(_obs.time), _obs.rainfallClicks);
		if (!ret)
			std::cerr << SD_ERR << "[MQTT " << station << "] management: "
				  << "Couldn't update the rainfall number of clicks, accumulation error possible"
				  << std::endl;
		ret = _db.cacheInt(station, BARANI_RAINFALL_CORRECTION_CACHE_KEY, chrono::system_clock::to_time_t(_obs.time), _obs.correction);
		if (!ret)
			std::cerr << SD_ERR << "[MQTT " << station << "] management: "
				  << "Couldn't update the rainfall number of clicks, accumulation error possible"
				  << std::endl;
	}
}

Observation BaraniRainGaugeMessage::getObservation(const CassUuid& station) const
{
	Observation result;

	if (_obs.valid) {
		result.station = station;
		result.day = date::floor<date::days>(_obs.time);
		result.time = _obs.time;
		result.rainfall = { _obs.rainfall != NAN, _obs.rainfall };
		result.rainrate = { _obs.maxRainrate != NAN, _obs.maxRainrate };
	}

	return result;
}

json::object BaraniRainGaugeMessage::getDecodedMessage() const
{
	return json::object{
		{ "model", "barani_pluviometer_20230411" },
		{ "value", {
			{ "index", _obs.index },
			{ "battery_voltage", _obs.batteryVoltage },
			{ "rainfall_clicks",_obs.rainfallClicks },
			{ "min_time_between_clicks", _obs.minTimeBetweenClicks },
			{ "max_rainrate", _obs.maxRainrate },
			{ "temp_over_2C", _obs.tempOver2C },
			{ "heater_switched_on", _obs.heaterSwitchedOn },
			{ "correction", _obs.correction }
		} }
	};
}

}
