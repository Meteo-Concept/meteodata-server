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
#include <observation.h>

#include "objenious_archive_message.h"
#include "../davis/vantagepro2_message.h"

namespace meteodata
{

namespace chrono = std::chrono;
namespace pt = boost::property_tree;

const std::map<std::string, float ObjeniousApiArchiveMessage::DataPoint::*> ObjeniousApiArchiveMessage::FIELDS = {{"temperature", &ObjeniousApiArchiveMessage::DataPoint::temperature},
																												  {"humidity",    &ObjeniousApiArchiveMessage::DataPoint::humidity},
																												  {"wind",        &ObjeniousApiArchiveMessage::DataPoint::windSpeed},
																												  {"gust",        &ObjeniousApiArchiveMessage::DataPoint::windGustSpeed},
																												  {"direction",   &ObjeniousApiArchiveMessage::DataPoint::windDir},
																												  {"rainrate",    &ObjeniousApiArchiveMessage::DataPoint::rainRate},
																												  {"rainfall",    &ObjeniousApiArchiveMessage::DataPoint::rainFall},
																												  {"uv",          &ObjeniousApiArchiveMessage::DataPoint::uvIndex}};

ObjeniousApiArchiveMessage::ObjeniousApiArchiveMessage(const std::map<std::string, std::string>* variables) :
		_variables{variables}
{
}

void ObjeniousApiArchiveMessage::ingest(const pt::ptree& data)
{
	using namespace date;

	std::istringstream rawDate{data.get<std::string>("timestamp", std::string{})};
	rawDate >> parse("%FT%T%Z", _obs.time);
	for (auto&&[mdVar, objVar] : *_variables) {
		// is this ungodly?
		_obs.*(FIELDS.at(mdVar)) = data.get("data." + objVar, INVALID_FLOAT);
	}
}

Observation ObjeniousApiArchiveMessage::getObservation(const CassUuid station) const
{
	Observation result;

	result.station = station;
	result.day = date::floor<date::days>(_obs.time);
	result.time = date::floor<chrono::seconds>(_obs.time);
	result.barometer = {!isInvalid(_obs.pressure), _obs.pressure};
	if (!isInvalid(_obs.temperature) && !isInvalid(_obs.humidity)) {
		result.dewpoint = {true, dew_point(_obs.temperature, _obs.humidity)};
		result.heatindex = {true, heat_index(from_Celsius_to_Farenheit(_obs.temperature), _obs.humidity)};
	}
	result.outsidehum = {!isInvalid(_obs.humidity), _obs.humidity};
	result.outsidetemp = {!isInvalid(_obs.temperature), _obs.temperature};
	result.rainrate = {!isInvalid(_obs.rainRate), _obs.rainRate};
	result.rainfall = {!isInvalid(_obs.rainFall), _obs.rainFall};
	result.winddir = {!isInvalid(_obs.windDir), _obs.windDir};
	result.windgust = {!isInvalid(_obs.windGustSpeed), _obs.windGustSpeed};
	result.windspeed = {!isInvalid(_obs.windSpeed), _obs.windSpeed};
	result.solarrad = {!isInvalid(_obs.solarRad), _obs.solarRad};
	// TODO insolation time ? it requires the time offseter
	result.uv = {!isInvalid(_obs.uvIndex), _obs.uvIndex};

	if (!isInvalid(_obs.temperature) && !isInvalid(_obs.windSpeed) && !isInvalid(_obs.humidity) &&
		!isInvalid(_obs.solarRad)) {
		result.thswindex = {true, thsw_index(from_Celsius_to_Farenheit(_obs.temperature), _obs.humidity, _obs.windSpeed,
											 _obs.solarRad)};
	} else if (!isInvalid(_obs.temperature) && !isInvalid(_obs.windSpeed) && !isInvalid(_obs.humidity) &&
			   isInvalid(_obs.solarRad)) {
		result.thswindex = {true,
							thsw_index(from_Celsius_to_Farenheit(_obs.temperature), _obs.humidity, _obs.windSpeed)};
	}
	return result;
}

}
