/**
 * @file llms01_leaf_sensor_message.cpp
 * @brief Implementation of the Llms01LeafSensorMessage class
 * @author Laurent Georget
 * @date 2023-01-27
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
#include <cmath>

#include <boost/json.hpp>
#include <systemd/sd-daemon.h>
#include <cassandra.h>
#include <observation.h>

#include "llms01_leaf_sensor_message.h"
#include "hex_parser.h"
#include "cassandra_utils.h"

namespace meteodata
{

namespace chrono = std::chrono;
namespace json = boost::json;

void Llms01LeafSensorMessage::ingest(const CassUuid&, const std::string& payload, const date::sys_seconds& datetime)
{
	using namespace hex_parser;

	if (!validateInput(payload, 22)) {
		_obs.valid = false;
		return;
	}

	_obs.time = datetime;

	std::istringstream is{payload};
	uint16_t temp;
	uint16_t wet;
	is >> parse(_obs.battery, 4, 16)
	   >> ignore(4)
	   >> parse(wet, 4, 16)
	   >> parse(temp, 4, 16)
	   >> ignore(6);

	if (temp == 0xFFFF) {
		_obs.leafTemperature = NAN;
	} else if ((temp & 0x8000) == 0) {
		_obs.leafTemperature = float(temp) / 10;
	} else {
		_obs.leafTemperature = (float(temp) - 65536) / 10;
	}

	if (wet == 0xFFFF) {
		_obs.leafWetness = NAN;
	} else {
		_obs.leafWetness = float(wet) / 10;
	}

	_obs.valid = true;
}

Observation Llms01LeafSensorMessage::getObservation(const CassUuid& station) const
{
	Observation obs;
	obs.station = station;
	obs.day = date::floor<date::days>(_obs.time);
	obs.time = _obs.time;
	obs.leaftemp[0] = {!std::isnan(_obs.leafTemperature), _obs.leafTemperature};
	obs.leafwetness_percent1 = {!std::isnan(_obs.leafWetness), _obs.leafWetness};
	return obs;
}

json::object Llms01LeafSensorMessage::getDecodedMessage() const
{
	return json::object{
		{ "model", "dragino_llms01_20231204" },
		{ "value", {
			{ "battery", _obs.battery },
			{ "leaf_temperature", _obs.leafTemperature },
			{ "leaf_wetness", _obs.leafWetness }
		} }
	};
}

}
