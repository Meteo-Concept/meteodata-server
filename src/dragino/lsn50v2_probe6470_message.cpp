/**
 * @file lsn50v2_probe6470_message.cpp
 * @brief Implementation of the Lsn50v2Probe6470Message class
 * @author Laurent Georget
 * @date 2024-03-19
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

#include "lsn50v2_probe6470_message.h"
#include "hex_parser.h"

namespace meteodata
{

namespace json = boost::json;

void Lsn50v2Probe6470Message::ingest(const CassUuid&, const std::string& payload, const date::sys_seconds& datetime)
{
	using namespace hex_parser;

	if (!validateInput(payload, 22)) {
		_obs.valid = false;
		return;
	}

	_obs.time = datetime;

	std::istringstream is{payload};
	uint16_t bat;
	uint16_t resistance;
	uint16_t adc0;
	is >> parse(bat, 4, 16)
	   >> parse(resistance, 4, 16)
	   >> parse(adc0, 4, 16)
	   >> ignore(10);

	if (bat <= adc0) {
		_obs.valid = false;
		return;
	}

	float lr0 = std::log(adc0*resistance/(bat-adc0));
	_obs.temperature = -273.15 + 1/(1.140e-3 + 2.320e-4 * lr0 + 9.860e-8 * std::pow(lr0, 3));
	_obs.battery = bat;

	_obs.valid = true;
}

Observation Lsn50v2Probe6470Message::getObservation(const CassUuid& station) const
{
	Observation obs;
	obs.station = station;
	obs.day = date::floor<date::days>(_obs.time);
	obs.time = _obs.time;
	obs.outsidetemp = {!std::isnan(_obs.temperature), _obs.temperature};
	obs.voltage_battery = {!std::isnan(_obs.battery), _obs.battery};
	return obs;
}

json::object Lsn50v2Probe6470Message::getDecodedMessage() const
{
	return json::object{
		{ "model", "dragino_6470_20240319" },
		{ "value", {
			{ "battery", _obs.battery },
			{ "temperature", _obs.temperature },
		} }
	};
}

}
