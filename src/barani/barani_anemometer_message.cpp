/**
 * @file barani_anemometer_message.cpp
 * @brief Implementation of the BaraniAnemometerMessage class
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

#include <boost/json.hpp>
#include <cassandra.h>
#include <cassobs/observation.h>

#include "barani_anemometer_message.h"
#include "hex_parser.h"

namespace meteodata
{

namespace chrono = std::chrono;
namespace json = boost::json;


void BaraniAnemometerMessage::ingest(const CassUuid&, const std::string& payload, const date::sys_seconds& datetime)
{
	using namespace hex_parser;

	if (!validateInput(payload, 20)) {
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

	// bytes 0-7: index
	_obs.index = raw[0];
	// bytes 8-10: battery, resolution 0.2V, offset 3V
	uint16_t battery = (raw[1] & 0b1110'0000) >> 5;
	_obs.batteryVoltage = battery == 0b111 ? NAN : (3 + battery * 0.2f);
	// bytes 12-20: wind 10-min avg speed, resolution 0.1m/s
	uint16_t windAvg10minSpeed = ((raw[1] & 0b0001'1111) << 4) + ((raw[2] & 0b1111'0000) >> 4);
	_obs.windAvg10minSpeed = windAvg10minSpeed == 0b1'1111'1111 ? NAN : windAvg10minSpeed * 0.36f;
	// bytes 21-29: wind 3-s gust, resolution 0.1m/s
	uint16_t wind3sGustSpeed = ((raw[2] & 0b0000'1111) << 5) + ((raw[3] & 0b1111'1000) >> 3);
	_obs.wind3sGustSpeed = wind3sGustSpeed == 0b1'1111'1111 ? NAN : (windAvg10minSpeed + wind3sGustSpeed) * 0.36f;
	// bytes 30-38: wind 3-s gust min speed, resolution 0.1m/s
	uint16_t wind3sMinSpeed = ((raw[3] & 0b0000'0111) << 6) + ((raw[4] & 0b1111'1100) >> 2);
	_obs.wind3sMinSpeed = wind3sMinSpeed == 0b1'1111'1111 ? NAN : (windAvg10minSpeed - wind3sMinSpeed) * 0.36f;
	// bytes 39-46: wind speed std deviation, resolution 0.1m/s
	uint16_t windSpeedStdev = ((raw[4] & 0b0000'0011) << 6) + ((raw[5] & 0b1111'1100) >> 2);
	_obs.windSpeedStdev = windSpeedStdev == 0b1111'1111 ? NAN : windSpeedStdev * 0.36f;
	// bytes 47-55: wind 10-min direction, resolution 1°
	uint16_t windAvg10minDirection = ((raw[5] & 0b0000'0011) << 7) + ((raw[6] & 0b1111'1110) >> 1);
	_obs.windAvg10minDirection = windAvg10minDirection == 0b111'1111 ? -1 : windAvg10minDirection;
	// bytes 56-64: wind 3-s direction, resolution 1°
	uint16_t wind3sGustDirection = ((raw[6] & 0b0000'0001) << 8) + raw[7];
	_obs.wind3sGustDirection = wind3sGustDirection == 0b1'1111'1111 ? -1 : wind3sGustDirection;
	// bytes 65-71: direction std deviation, resolution 1°
	uint16_t windDirectionStdev = (raw[8] & 0b1111'1110) >> 1;
	_obs.windDirectionStdev = windDirectionStdev == 0b111'1111 ? -1 : windDirectionStdev;
	// bytes 72-78: time of max wind, resolution 5s, offset from start of logging interval (10min)
	int t = ((raw[8] & 0b0000'0001) << 6) + ((raw[9] & 0b1111'1100) >> 2);
	_obs.maxWindDatetime = date::floor<chrono::minutes>(datetime) - chrono::minutes{10} + chrono::seconds{t * 5};
	// byte 79: vector/scalar flag, scalar: 0, vector: 1, only scalar is supported or now
	_obs.vectorOrScalar = raw[9] & 0b0000'0010;
	// byte 80: alarm flag
	_obs.alarmSent = raw[9] & 0b0000'0001;
}

Observation BaraniAnemometerMessage::getObservation(const CassUuid& station) const
{
	Observation result;

	if (_obs.valid) {
		result.station = station;
		result.day = date::floor<date::days>(_obs.time);
		result.time = _obs.time;
		result.windspeed = { !std::isnan(_obs.windAvg10minSpeed), _obs.windAvg10minSpeed };
		result.windgust = { !std::isnan(_obs.wind3sGustSpeed), _obs.wind3sGustSpeed };
		result.winddir = { _obs.windAvg10minDirection >= 0, _obs.windAvg10minDirection };
	}

	return result;
}

json::object BaraniAnemometerMessage::getDecodedMessage() const
{
	std::ostringstream os;
	using namespace date;
	os << date::format("%FT%TZ", _obs.maxWindDatetime);

	return json::object{
		{ "model", "barani_anemometer_20230411" },
		{ "value", {
			{ "index", _obs.index },
			{ "battery_voltage", _obs.batteryVoltage },
			{ "wind_avg_10min_speed",_obs.windAvg10minSpeed },
			{ "wind_3s_gust_speed", _obs.wind3sGustSpeed },
			{ "wind_speed_stdev", _obs.windSpeedStdev },
			{ "wind_avg_10min_direction", _obs.windAvg10minDirection },
			{ "wind_3s_gust_direction", _obs.wind3sGustDirection },
			{ "max_wind_datetime", os.str() },
			{ "vector_or_scalar", _obs.vectorOrScalar },
			{ "alarm_sent", _obs.alarmSent }
		} }
	};
}

}
