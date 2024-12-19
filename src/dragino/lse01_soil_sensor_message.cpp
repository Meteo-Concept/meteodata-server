/**
 * @file lse01_soil_sensor_message.cpp
 * @brief Implementation of the Lse01SoilSensorMessage class
 * @author Laurent Georget
 * @date 2024-12-17
 */
/*
 * Copyright (C) 2024  SAS JD Environnement <contact@meteo-concept.fr>
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
#include <cassobs/observation.h>

#include "davis/vantagepro2_message.h"
#include "lse01_soil_sensor_message.h"
#include "hex_parser.h"
#include "cassandra_utils.h"

namespace meteodata
{

namespace chrono = std::chrono;
namespace json = boost::json;

void Lse01SoilSensorMessage::ingest(const CassUuid&, const std::string& payload, const date::sys_seconds& datetime)
{
	using namespace hex_parser;

	if (!validateInput(payload, 22)) {
		_obs.valid = false;
		return;
	}

	_obs.time = datetime;

	std::istringstream is{payload};
	uint16_t temp;
	uint16_t moisture;
	uint16_t conductivity;
	is >> parse(_obs.battery, 4, 16)
	   >> ignore(4)
	   >> parse(moisture, 4, 16)
	   >> parse(temp, 4, 16)
	   >> parse(conductivity, 4, 16)
	   >> ignore(2);

	if (temp == 0xFFFF) {
		_obs.soilTemperature = NAN;
	} else if ((temp & 0x8000) == 0) {
		_obs.soilTemperature = float(temp) / 100;
	} else {
		_obs.soilTemperature = (float(temp) - 65536) / 100;
	}

	if (moisture == 0xFFFF) {
		_obs.soilMoisture = NAN;
	} else {
		_obs.soilMoisture = float(moisture) / 100;
	}

	if (conductivity == 0xFFFF) {
		_obs.soilConductivity = NAN;
	} else {
		_obs.soilConductivity = float(conductivity);
	}

	_obs.valid = true;
}

Observation Lse01SoilSensorMessage::getObservation(const CassUuid& station) const
{
	Observation obs;
	obs.station = station;
	obs.day = date::floor<date::days>(_obs.time);
	obs.time = _obs.time;
	obs.soiltemp[0] = {!std::isnan(_obs.soilTemperature), _obs.soilTemperature};
	obs.soilmoistures[0] = {!std::isnan(_obs.soilMoisture), _obs.soilMoisture};
	obs.soil_conductivity1 = {!std::isnan(_obs.soilConductivity), _obs.soilConductivity};
	return obs;
}

json::object Lse01SoilSensorMessage::getDecodedMessage() const
{
	return json::object{
		{ "model", "dragino_lse01_20241217" },
		{ "value", {
			{ "battery", _obs.battery },
			{ "soil_temperature", _obs.soilTemperature },
			{ "soil_moisture", _obs.soilMoisture },
			{ "soil_conductivity", _obs.soilConductivity }
		} }
	};
}

}
