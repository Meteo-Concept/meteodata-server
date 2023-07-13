/**
 * @file thpllora_message.cpp
 * @brief Implementation of the ThplloraMessage class
 * @author Laurent Georget
 * @date 2023-05-01
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
#include <cmath>

#include <systemd/sd-daemon.h>
#include <cassandra.h>
#include <observation.h>
#include <boost/json.hpp>

#include "thpllora_message.h"
#include "hex_parser.h"
#include "cassandra_utils.h"

namespace meteodata
{

namespace chrono = std::chrono;
namespace json = boost::json;

ThplloraMessage::ThplloraMessage(DbConnectionObservations& db):
	_db{db}
{}

void ThplloraMessage::ingest(const CassUuid& station, const std::string& payload, const date::sys_seconds& datetime)
{
	using namespace hex_parser;

	if (!validateInput(payload, 24)) {
		_obs.valid = false;
		return;
	}

	_obs.time = datetime;
	uint16_t battery;
	uint16_t rainrate;
	uint16_t temp;
	uint16_t hum;

	std::istringstream is{payload};
	is >> parse(battery, 4, 16)
	   >> parse(rainrate, 4, 16)
	   >> parse(_obs.totalPulses, 8, 16)
	   >> parse(temp, 4, 16)
	   >> parse(hum, 4, 16);

	_obs.battery = float(battery) / 1000;

	if (rainrate == 0x7FFF) {
		_obs.rainrate = NAN;
	} else {
		_obs.rainrate = float(rainrate) / 10;
	}

	_obs.humidity = float(hum) / 10;
	if (temp == 0xFFFF) {
		_obs.temperature = NAN;
		_obs.humidity = NAN;
	} else if ((temp & 0xFC00) == 0) {
		_obs.temperature = float(temp) / 10;
	} else {
		_obs.temperature = (float(temp) - 65535) / 10;
	}

	time_t lastUpdate;
	int previousClicks;
	bool result = _db.getCachedInt(station, THPLLORA_RAINFALL_CACHE_KEY, lastUpdate, previousClicks);
	if (result && chrono::_V2::system_clock::from_time_t(lastUpdate) > chrono::_V2::system_clock::now() - chrono::hours{24}) {
		// the last rainfall datapoint is not too old, we can use
		// it as a reference for the current number of clicks recorded
		// by the pluviometer
		if (_obs.totalPulses >= previousClicks) {
			_obs.rainfall = (_obs.totalPulses - previousClicks) * THPLLORA_RAIN_GAUGE_RESOLUTION;
		} else {
			_obs.rainfall = ((0xFFFFFFFFu - previousClicks) + _obs.totalPulses) * THPLLORA_RAIN_GAUGE_RESOLUTION;
		}
	}

	_obs.valid = true;
}

void ThplloraMessage::cacheValues(const CassUuid& station)
{
	if (_obs.valid) {
		int ret = _db.cacheInt(station, THPLLORA_RAINFALL_CACHE_KEY, chrono::system_clock::to_time_t(_obs.time), _obs.totalPulses);
		if (!ret)
			std::cerr << SD_ERR << "[MQTT " << station << "] management: "
				  << "Couldn't update the rainfall number of clicks, accumulation error possible"
				  << std::endl;
	}
}

Observation ThplloraMessage::getObservation(const CassUuid& station) const
{
	Observation obs;
	obs.station = station;
	obs.day = date::floor<date::days>(_obs.time);
	obs.time = _obs.time;
	obs.rainfall = {!std::isnan(_obs.rainfall), _obs.rainfall};
	obs.rainrate = {!std::isnan(_obs.rainrate), _obs.rainrate};
	obs.outsidetemp = {!std::isnan(_obs.temperature), _obs.temperature};
	obs.outsidehum = {!std::isnan(_obs.humidity), int(std::round(_obs.humidity))};
	return obs;
}

json::object ThplloraMessage::getDecodedMessage() const
{
	return json::object{
		{ "model", "Thpllora_20230713" },
		{ "value", {
			{ "battery", _obs.battery },
			{ "temperature", _obs.temperature },
			{ "humidity", _obs.humidity },
			{ "total_pulses", _obs.totalPulses },
			{ "rainfall", _obs.rainfall },
			{ "rainrate", _obs.rainrate },
		} }
	};
}

}
