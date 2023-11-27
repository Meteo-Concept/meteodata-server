/**
 * @file lsn50v2_thermohygrometer_message.cpp
 * @brief Implementation of the Lsn50v2ThermohygrometerMessage class
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

#include "lsn50v2_thermohygrometer_message.h"
#include "hex_parser.h"
#include "cassandra_utils.h"
#include "davis/vantagepro2_message.h"

namespace meteodata
{

namespace chrono = std::chrono;
namespace json = boost::json;

void Lsn50v2ThermohygrometerMessage::ingest(const CassUuid&, const std::string& payload, const date::sys_seconds& datetime)
{
	using namespace hex_parser;

	if (!validateInput(payload, 22)) {
		_obs.valid = false;
		return;
	}

	_obs.time = datetime;

	std::istringstream is{payload};
	uint16_t temp;
	uint16_t hum;
	is >> ignore(14)
	   >> parse(temp, 4, 16)
	   >> parse(hum, 4, 16);

	_obs.humidity = float(hum) / 10;
	if (temp == 0xFFFF && hum == 0xFFFF) {
		_obs.temperature = NAN;
		_obs.humidity = NAN;
	} else if ((temp & 0x8000) == 0) {
		_obs.temperature = float(temp) / 10;
	} else {
		_obs.temperature = (float(temp) - 65536) / 10;
	}

	_obs.valid = true;
}

Observation Lsn50v2ThermohygrometerMessage::getObservation(const CassUuid& station) const
{
	Observation obs;
	obs.station = station;
	obs.day = date::floor<date::days>(_obs.time);
	obs.time = _obs.time;
	obs.outsidetemp = {!std::isnan(_obs.temperature), _obs.temperature};
	obs.outsidehum = {!std::isnan(_obs.humidity), int(std::round(_obs.humidity))};
	if (!std::isnan(_obs.temperature) && !std::isnan(_obs.humidity)) {
		obs.dewpoint = {true, dew_point(_obs.temperature, _obs.humidity)};
		obs.heatindex = {true, heat_index(from_Celsius_to_Farenheit(_obs.temperature), _obs.humidity)};
	}
	return obs;
}

json::object Lsn50v2ThermohygrometerMessage::getDecodedMessage() const
{
	return json::object{
		{ "model", "dragino_lsn50v2_20230411" },
		{ "value", {
			{ "temperature", _obs.temperature },
			{ "humidity", _obs.humidity }
		} }
	};
}

}
