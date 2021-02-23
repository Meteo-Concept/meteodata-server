/**
 * @file objenious_api_archive_message.cpp
 * @brief Implementation of the ObjeniousApiArchiveMessage class
 * @author Laurent Georget
 * @date 2021-02-23
 */
/*
 * Copyright (C) 2021  SAS JD Environnement <contact@meteo-concept.fr>
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

#include <iostream>
#include <sstream>
#include <string>
#include <limits>
#include <algorithm>
#include <functional>
#include <map>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <cassandra.h>
#include <message.h>

#include "objenious_archive_message.h"
#include "../davis/vantagepro2_message.h"

namespace meteodata {

namespace chrono = std::chrono;
namespace pt = boost::property_tree;

const std::map<std::string, float ObjeniousApiArchiveMessage::Observation::*> ObjeniousApiArchiveMessage::FIELDS = {
	{"temperature", &ObjeniousApiArchiveMessage::Observation::temperature},
	{"humidity", &ObjeniousApiArchiveMessage::Observation::humidity},
	{"wind", &ObjeniousApiArchiveMessage::Observation::windSpeed},
	{"gust", &ObjeniousApiArchiveMessage::Observation::windGustSpeed},
	{"direction", &ObjeniousApiArchiveMessage::Observation::windDir},
	{"rainrate", &ObjeniousApiArchiveMessage::Observation::rainRate},
	{"rainfall", &ObjeniousApiArchiveMessage::Observation::rainFall},
	{"uv", &ObjeniousApiArchiveMessage::Observation::uvIndex}
};

ObjeniousApiArchiveMessage::ObjeniousApiArchiveMessage(const std::map<std::string, std::string>* variables) :
	_variables{variables}
{
}

void ObjeniousApiArchiveMessage::ingest(const pt::ptree& jsonTree, int index)
{
	// Every individual message will receive the full tree, but a specific
	// index (one index = one date = one datapoint)
	const pt::ptree& data = jsonTree.get_child("values");
	auto entryIt = std::next(data.begin(), index);

	using namespace date;

	std::istringstream rawDate{entryIt->second.get_value<std::string>("timestamp")};
	rawDate >> parse("%FT%T%Z", _obs.time);
	for (auto&& [mdVar, objVar] : *_variables) {
		// is this ungodly?
		_obs.*(FIELDS.at(mdVar)) = data.get("data." + objVar, INVALID_FLOAT);
	}
}

void ObjeniousApiArchiveMessage::populateDataPoint(const CassUuid, CassStatement* const) const
{
	// not implemented
}

void ObjeniousApiArchiveMessage::populateV2DataPoint(const CassUuid station, CassStatement* const statement) const
{
	/*************************************************************/
	cass_statement_bind_uuid(statement, 0, station);
	/*************************************************************/
	cass_statement_bind_uint32(statement, 1,
		cass_date_from_epoch(
			date::floor<chrono::seconds>(
				_obs.time
			).time_since_epoch().count()
		)
	);
	/*************************************************************/
	cass_statement_bind_int64(statement, 2,
		chrono::system_clock::to_time_t(_obs.time) * 1000
	);
	/*************************************************************/
	if (!isInvalid(_obs.pressure))
		cass_statement_bind_float(statement, 3, _obs.pressure);
	/*************************************************************/
	if (!isInvalid(_obs.temperature) && !isInvalid(_obs.humidity))
		cass_statement_bind_float(statement, 4,
			dew_point(
				_obs.temperature,
				_obs.humidity
			)
		);
	/*************************************************************/
	// No extra humidities
	/*************************************************************/
	// No extra temperatures
	/*************************************************************/
	if (!isInvalid(_obs.temperature) && !isInvalid(_obs.humidity))
		cass_statement_bind_float(statement, 10,
			heat_index(
				from_Celsius_to_Farenheit(_obs.temperature),
				_obs.humidity
			)
		);
	/*************************************************************/
	// No inside humidity
	/*************************************************************/
	// No inside temperature
	/*************************************************************/
	// No leaf temperature and wetness
	/*************************************************************/
	if (!isInvalid(_obs.humidity))
		cass_statement_bind_int32(statement, 17, _obs.humidity);
	/*************************************************************/
	if (!isInvalid(_obs.temperature))
		cass_statement_bind_float(statement, 18, _obs.temperature);
	/*************************************************************/
	if (!isInvalid(_obs.rainRate))
		cass_statement_bind_float(statement, 19, _obs.rainRate);
	/*************************************************************/
	if (!isInvalid(_obs.rainFall))
		cass_statement_bind_float(statement, 20, _obs.rainFall);
	/*************************************************************/
	// No ETP
	/*************************************************************/
	// No soil moistures and temperatures
	/*************************************************************/
	if (!isInvalid(_obs.solarRad))
		cass_statement_bind_int32(statement, 30, _obs.solarRad);
	/*************************************************************/
	if (!isInvalid(_obs.temperature) && !isInvalid(_obs.windSpeed)
	 && !isInvalid(_obs.humidity) && !isInvalid(_obs.solarRad))
		cass_statement_bind_float(statement, 31,
			thsw_index(
				from_Celsius_to_Farenheit(_obs.temperature),
				_obs.humidity,
				_obs.windSpeed,
				_obs.solarRad
			)
		);
	else if (!isInvalid(_obs.temperature) && !isInvalid(_obs.windSpeed)
	 && !isInvalid(_obs.humidity) && isInvalid(_obs.solarRad))
		cass_statement_bind_float(statement, 31,
			thsw_index(
				from_Celsius_to_Farenheit(_obs.temperature),
				_obs.humidity,
				_obs.windSpeed
			)
		);
	/*************************************************************/
	if (!isInvalid(_obs.uvIndex))
		cass_statement_bind_int32(statement, 32, int(_obs.uvIndex * 10));
	/*************************************************************/
	if (!isInvalid(_obs.temperature) && !isInvalid(_obs.windSpeed))
		cass_statement_bind_float(statement, 33,
			wind_chill(
				from_Celsius_to_Farenheit(_obs.temperature),
				_obs.windSpeed
			)
		);
	/*************************************************************/
	if (!isInvalid(_obs.windDir))
		cass_statement_bind_int32(statement, 34, _obs.windDir);
	/*************************************************************/
	if (!isInvalid(_obs.windGustSpeed))
		cass_statement_bind_float(statement, 35, from_mps_to_kph(_obs.windGustSpeed));
	/*************************************************************/
	if (!isInvalid(_obs.windSpeed))
		cass_statement_bind_float(statement, 36, from_mps_to_kph(_obs.windSpeed));
	/*************************************************************/
	// No insolation
	/*************************************************************/
	// No min/max temperature
	/*************************************************************/
	/*************************************************************/
	// No leaf wetnesses ratio
}

}
