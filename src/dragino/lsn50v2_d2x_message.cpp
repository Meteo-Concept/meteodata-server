/**
 * @file lsn50v2_d2x_message.cpp
 * @brief Implementation of the Lsn50v2D2xMessage class
 * @author Laurent Georget
 * @date 2025-08-26
 */
/*
 * Copyright (C) 2025  SAS JD Environnement <contact@meteo-concept.fr>
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

#include "lsn50v2_d2x_message.h"
#include "hex_parser.h"

namespace meteodata
{

namespace json = boost::json;

void Lsn50v2D2xMessage::ingest(const CassUuid&, const std::string& payload, const date::sys_seconds& datetime)
{
	using namespace hex_parser;

	if (!validateInput(payload, 22)) {
		_obs.valid = false;
		return;
	}

	_obs.time = datetime;

	std::istringstream is{payload};
	uint16_t bat;
	uint16_t temperature[3];
	uint16_t alarm;
	is >> parse(bat, 4, 16)
	   >> parse(temperature[0], 4, 16)
	   >> ignore(4)
	   >> parse(alarm, 2, 16)
	   >> parse(temperature[1], 4, 16)
	   >> parse(temperature[2], 4, 16);

	for (int i=0 ; i<3 ; i++) {
		if (temperature[i] == 0xFFFF) {
			_obs.temperature[i] = NAN;
		} else if ((temperature[i] & 0x8000) == 0) {
			_obs.temperature[i] = float(temperature[i]) / 10;
		} else {
			_obs.temperature[i] = (float(temperature[i]) - 65536) / 10;
		}
	}
	_obs.battery = bat;
	_obs.alarm = alarm;

	_obs.valid = true;
}

Observation Lsn50v2D2xMessage::getObservation(const CassUuid& station) const
{
	Observation obs;
	obs.station = station;
	obs.day = date::floor<date::days>(_obs.time);
	obs.time = _obs.time;
	obs.outsidetemp = {!std::isnan(_obs.temperature[0]), _obs.temperature[0]};
	obs.extratemp[0] = {!std::isnan(_obs.temperature[1]), _obs.temperature[1]};
	obs.extratemp[1] = {!std::isnan(_obs.temperature[2]), _obs.temperature[2]};
	obs.voltage_battery = {!std::isnan(_obs.battery), _obs.battery};
	return obs;
}

json::object Lsn50v2D2xMessage::getDecodedMessage() const
{
	return json::object{
		{ "model", "dragino_d2x_20250826" },
		{ "value", {
			{ "battery", _obs.battery },
			{ "temperature1", _obs.temperature[0] },
			{ "temperature2", _obs.temperature[1] },
			{ "temperature3", _obs.temperature[3] },
			{ "alarm", _obs.alarm },
		} }
	};
}

}
