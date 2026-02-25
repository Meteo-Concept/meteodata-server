/**
 * @file barani_anemometer_2026_message.cpp
 * @brief Implementation of the BaraniAnemometer2026Message class
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
#include <array>
#include <algorithm>
#include <cmath>

#include <boost/json.hpp>
#include <systemd/sd-daemon.h>
#include <cassandra.h>
#include <cassobs/observation.h>

#include "barani_anemometer_2026_message.h"
#include "hex_parser.h"
#include "cassandra_utils.h"

namespace
{
	float pulsesToKmh(uint16_t pulses)
	{
		float result = -0.00065f * pulses * pulses + 0.675f * pulses + 0.2f;
		if (result <= 0)
			result = 0.f;
		return result * 3.6f;
	}
}

namespace meteodata
{

namespace chrono = std::chrono;
namespace json = boost::json;

const std::string BaraniAnemometer2026Message::BARANI_LAST_BATTERY = "meteowind_battery";

BaraniAnemometer2026Message::BaraniAnemometer2026Message(DbConnectionObservations& db):
	LiveobjectsMessage{},
	_db{db}
{}

void BaraniAnemometer2026Message::ingest(const CassUuid& station, const std::string& payload, const date::sys_seconds& datetime)
{
	using namespace hex_parser;

	if (!validateInput(payload, 28)) {
		_obs.valid = false;
		return;
	}

	_obs.time = datetime;

	// parse and fill in the obs
	_obs.valid = true;

	std::istringstream is{payload};

	// store the numbers on 16-bit integers to ensure the bit manipulations below never cause overflow
	std::array<uint16_t, 14> raw;
	for (int byte=0 ; byte<14 ; byte++) {
		is >> parse(raw[byte], 2, 16);
	}

	time_t lastUpdateTimestamp;
	int knownBattery = 33;
	_db.getCachedInt(station, BARANI_LAST_BATTERY, lastUpdateTimestamp, knownBattery);

	// bits 0-7: index
	_obs.index = raw[0];
	// byte 8: battery index from which battery voltage is computed, resolution 0.2V, offset 3V
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
	// bits 9-20: wind 10-min avg speed, resolution 0.02Hz
	uint16_t windAvg10minSpeed = ((raw[1] & 0b0111'1111) << 5) + ((raw[2] & 0b1111'1000) >> 3);
	_obs.windAvg10minSpeed = windAvg10minSpeed == 0b1111'1111'1111 ? NAN : windAvg10minSpeed == 0b0000'0000'0000 ? 0 : ::pulsesToKmh(windAvg10minSpeed * 0.02f);
	// bits 21-29: wind 3-s gust, resolution 0.1Hz
	uint16_t wind3sGustSpeed = ((raw[2] & 0b0000'0111) << 6) + ((raw[3] & 0b1111'1100) >> 2);
	_obs.wind3sGustSpeed = wind3sGustSpeed == 0b1'1111'1111 ? NAN : ::pulsesToKmh(windAvg10minSpeed * 0.02f + wind3sGustSpeed * 0.1f);
	// bits 30-37: wind 1-s gust, resolution 0.1Hz
	uint16_t wind1sGustSpeed = ((raw[3] & 0b0000'0011) << 6) + ((raw[4] & 0b1111'1100) >> 2);
	_obs.wind1sGustSpeed = wind1sGustSpeed == 0b1111'1111 ? NAN : ::pulsesToKmh(windAvg10minSpeed * 0.02f + wind3sGustSpeed * 0.1f + wind1sGustSpeed * 0.1f);
	// bits 38-46: wind 3-s gust min, resolution 0.1Hz
	uint16_t wind3sMinSpeed = ((raw[4] & 0b0000'0011) << 7) + ((raw[5] & 0b1111'1110) >> 1);
	_obs.wind3sMinSpeed = wind3sMinSpeed == 0b1'1111'1111 ? NAN : wind3sMinSpeed == 0b0'0000'0000 ? 0 : ::pulsesToKmh(wind3sMinSpeed * 0.1f);
	// bits 47-54: 1-s wind speed std deviation, resolution 0.1Hz
	uint16_t windSpeedStdev = ((raw[5] & 0b0000'0001) << 7) + ((raw[6] & 0b1111'1110) >> 1);
	_obs.windSpeedStdev = windSpeedStdev == 0b1111'1111 ? NAN : windSpeedStdev == 0b0000'0000 ? 0 : ::pulsesToKmh(windSpeedStdev);
	// bits 55-63: wind 10-min direction, resolution 1°
	uint16_t windAvg10minDirection = ((raw[6] & 0b0000'0001) << 8) + raw[7];
	_obs.windAvg10minDirection = windAvg10minDirection == 0b1'1111'1111 ? -1 : windAvg10minDirection;
	// bits 64-72: wind 1-s direction, resolution 1°
	uint16_t wind1sGustDirection = (raw[8] << 1) + ((raw[9] & 0b1000'0000) >> 7);
	_obs.wind1sGustDirection = wind1sGustDirection == 0b1'1111'1111 ? -1 : wind1sGustDirection;
	// bits 73-80: direction std deviation, resolution 1°
	uint16_t windDirectionStdev = ((raw[9] & 0b0111'1111) << 1) + ((raw[10] & 0b1000'0000) >> 7);
	_obs.windDirectionStdev = windDirectionStdev == 0b1111'1111 ? -1 : windDirectionStdev;
	// bits 81-89: min angle reached counter-clockwise, resolution 1°
	uint16_t minCcw = ((raw[10] & 0b0111'1111) << 2) + ((raw[11] & 0b1100'0000) >> 6);
	_obs.dirCCWMin = minCcw == 0b1'1111'1111 ? -1 : minCcw;
	// bits 90-98: max angle reached clockwise, resolution 1°
	uint16_t maxCw = ((raw[11] & 0b0011'1111) << 3) + ((raw[12] & 0b1110'0000) >> 5);
	_obs.dirCWMax = minCcw == 0b1'1111'1111 ? -1 : maxCw;
	// bits 99-105: time of max wind, resolution 5s, offset from start of logging interval (10min)
	int t = ((raw[12] & 0b0001'1111) << 2) + ((raw[13] & 0b1100'0000) >> 6);
	_obs.maxWindDatetime = date::floor<chrono::seconds>(datetime) - chrono::minutes{10} + chrono::seconds{t * 5};
	// bit 106: alarm flag
	_obs.alarmSent = (raw[13] & 0b0010'0000) >> 5;
	// bits 107-111: debug flags
	_obs.debugFlags = raw[11] & 0b0011'1111;
}

Observation BaraniAnemometer2026Message::getObservation(const CassUuid& station) const
{
	Observation result;

	if (_obs.valid) {
		result.station = station;
		result.day = date::floor<date::days>(_obs.time);
		result.time = _obs.time;
		result.windspeed = { !std::isnan(_obs.windAvg10minSpeed), _obs.windAvg10minSpeed };
		result.min_windspeed = { !std::isnan(_obs.wind3sMinSpeed), _obs.wind3sMinSpeed };
		result.windgust = { !std::isnan(_obs.wind3sGustSpeed), _obs.wind3sGustSpeed };
		result.max_windgust = { !std::isnan(_obs.wind1sGustSpeed), _obs.wind1sGustSpeed };
		result.winddir = { _obs.windAvg10minDirection >= 0, _obs.windAvg10minDirection };
		result.voltage_battery = { !std::isnan(_obs.batteryVoltage), _obs.batteryVoltage };
	}

	return result;
}

json::object BaraniAnemometer2026Message::getDecodedMessage() const
{
	std::ostringstream os;
	using namespace date;
	os << date::format("%FT%TZ", _obs.maxWindDatetime);

	return json::object{
		{ "model", "barani_anemometer_v2026_20260225225" },
		{ "value", {
			{ "index", _obs.index },
			{ "battery_voltage", _obs.batteryVoltage },
			{ "wind_avg_10min_speed",_obs.windAvg10minSpeed },
			{ "wind_3s_gust_speed", _obs.wind3sGustSpeed },
			{ "wind_1s_gust_speed", _obs.wind1sGustSpeed },
			{ "wind_3s_min_speed", _obs.wind3sMinSpeed },
			{ "wind_speed_stdev", _obs.windSpeedStdev },
			{ "wind_avg_10min_direction", _obs.windAvg10minDirection },
			{ "wind_1s_gust_direction", _obs.wind1sGustDirection },
			{ "direction_ccw_min", _obs.dirCCWMin },
			{ "direction_cw_max", _obs.dirCWMax },
			{ "max_wind_datetime", os.str() },
			{ "alarm_sent", _obs.alarmSent },
			{ "debug_flags", _obs.debugFlags }
		} }
	};
}

}
