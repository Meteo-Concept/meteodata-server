/**
 * @file generic_message.cpp
 * @brief Implementation of the GenericMessage class
 * @author Laurent Georget
 * @date 2023-06-16
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

#include <algorithm>
#include <iostream>
#include <string>
#include <systemd/sd-daemon.h>

#include "generic_message.h"
#include "cassandra_utils.h"

namespace meteodata
{

bool GenericMessage::looksValid() const
{
	return _obs.valid;
}

GenericMessage GenericMessage::buildMessage(DbConnectionObservations& db,
	const boost::property_tree::ptree& json, date::sys_seconds& timestamp)
{
	GenericMessage m;
	m._content = json;

	auto inbandTimestamp = json.get<int>("timestamp", 0);
	if (inbandTimestamp == 0) {
		m._obs.valid = false;
	} else {
		m._obs.valid = true;

		using namespace date;
		m._obs.time = date::floor<chrono::seconds>(chrono::system_clock::from_time_t(inbandTimestamp));
		timestamp = m._obs.time;

		std::cout << SD_DEBUG << "Parsing message with timestamp " << timestamp << std::endl;

		m._obs.windAvg = m._content.get<float>("wind_avg", NAN);
		m._obs.windMax = m._content.get<float>("wind_max", NAN);
		m._obs.temperature = m._content.get<float>("temperature", NAN);
		m._obs.temperature_min = m._content.get<float>("temperature_min", NAN);
		m._obs.temperature_max = m._content.get<float>("temperature_max", NAN);
		m._obs.humidity = m._content.get<float>("humidity", NAN);
		m._obs.windDir = m._content.get<float>("wind_dir_avg", NAN);
		m._obs.dewPoint = m._content.get<float>("dew_point", NAN);
		m._obs.rainfall = m._content.get<float>("rainfall", NAN);
		m._obs.rainrate = m._content.get<float>("rainrate", NAN);
		m._obs.solarrad = m._content.get<float>("solar_radiation", NAN);
		m._obs.uv = m._content.get<float>("uv", NAN);
	}
	return m;
}

Observation GenericMessage::getObservation(const CassUuid& station) const
{
	Observation result;

	if (_obs.valid) {
		result.station = station;
		result.day = date::floor<date::days>(_obs.time);
		result.time = _obs.time;

		result.windspeed = { !std::isnan(_obs.windAvg), _obs.windAvg };
		result.windgust = { !std::isnan(_obs.windMax), _obs.windMax };
		result.winddir = { !std::isnan(_obs.windDir), _obs.windDir };
		result.outsidetemp = { !std::isnan(_obs.temperature), _obs.temperature };
		result.min_outside_temperature = { !std::isnan(_obs.temperature_min), _obs.temperature_min };
		result.max_outside_temperature = { !std::isnan(_obs.temperature_max), _obs.temperature_max };
		result.outsidehum = { !std::isnan(_obs.humidity), int(_obs.humidity) };
		result.dewpoint = { !std::isnan(_obs.dewPoint), _obs.dewPoint };
		result.rainfall = { !std::isnan(_obs.rainfall), _obs.rainfall };
		result.rainrate = { !std::isnan(_obs.rainrate), _obs.rainrate };
		result.solarrad = { !std::isnan(_obs.solarrad), int(_obs.solarrad) };
		result.uv = { !std::isnan(_obs.uv), int(_obs.uv) };
	}

	return result;
}

}
