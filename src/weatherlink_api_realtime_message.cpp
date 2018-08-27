/**
 * @file weatherlink_api_realtime_message.cpp
 * @brief Implementation of the WeatherlinkApiRealtimeMessage class
 * @author Laurent Georget
 * @date 2018-07-23
 */
/*
 * Copyright (C) 2017  SAS Météo Concept <contact@meteo-concept.fr>
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

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

#include <cassandra.h>

#include "message.h"
#include "vantagepro2message.h"
#include "weatherlink_api_realtime_message.h"

namespace meteodata {

namespace chrono = std::chrono;
namespace pt = boost::property_tree;

constexpr int   WeatherlinkApiRealtimeMessage::MAXSIZE;
constexpr int   WeatherlinkApiRealtimeMessage::INVALID_INT;
constexpr float WeatherlinkApiRealtimeMessage::INVALID_FLOAT;

WeatherlinkApiRealtimeMessage::WeatherlinkApiRealtimeMessage() :
	Message()
{}

void WeatherlinkApiRealtimeMessage::parse(std::istream& input)
{
	pt::ptree xmlTree;
	pt::read_xml(input, xmlTree, pt::xml_parser::no_comments | pt::xml_parser::trim_whitespace);

	std::string time = xmlTree.get<std::string>("current_observation.observation_time_rfc822");
	std::istringstream in{time};
	in >> date::parse("%a, %d %b %Y %T %z", _obs.time);
	_obs.pressure = xmlTree.get<float>("current_observation.pressure_mb", INVALID_FLOAT);
	_obs.humidity = xmlTree.get<int>("current_observation.relative_humidity", INVALID_INT);
	_obs.temperature = xmlTree.get<float>("current_observation.temp_c", INVALID_FLOAT);
	_obs.temperatureF = xmlTree.get<float>("current_observation.temp_f", INVALID_FLOAT);
	_obs.windDir = xmlTree.get<int>("current_observation.wind_degrees", INVALID_INT);
	_obs.windSpeed = xmlTree.get<float>("current_observation.wind_mph", INVALID_FLOAT);
	_obs.windGustSpeed = xmlTree.get<float>("current_observation.davis_current_observation.wind_ten_min_gust_mph", INVALID_FLOAT);
	_obs.solarRad = xmlTree.get<int>("current_observation.davis_current_observation.solar_radiation", INVALID_INT);
	_obs.uvIndex = xmlTree.get<float>("current_observation.davis_current_observation.uv_index", INVALID_FLOAT);
}

void WeatherlinkApiRealtimeMessage::populateDataPoint(const CassUuid station, CassStatement* const statement) const
{
	std::cerr << "Populating the new datapoint (archived value)" << std::endl;
	/*************************************************************/
	cass_statement_bind_uuid(statement, 0, station);
	/*************************************************************/
	cass_statement_bind_int64(statement, 1, _obs.time.time_since_epoch().count());
	/*************************************************************/
	// No bar trend
	/*************************************************************/
	if (!isInvalid(_obs.pressure))
		cass_statement_bind_float(statement, 3, _obs.pressure);
	/*************************************************************/
	// No absolute barometric pressure
	/*************************************************************/
	// No raw barometric sensor reading
	/*************************************************************/
	// No inside temperature
	/*************************************************************/
	if (!isInvalid(_obs.temperature))
		cass_statement_bind_float(statement, 7, _obs.temperature);
	/*************************************************************/
	// No inside humidity
	/*************************************************************/
	if (!isInvalid(_obs.humidity))
		cass_statement_bind_int32(statement, 9, _obs.humidity);
	/*************************************************************/
	// No extra temperatures
	/*************************************************************/
	// No leaf temperatures, soil temperatures, leaf temperatures, soil moistures
	/*************************************************************/
	/*************************************************************/
	if (!isInvalid(_obs.windSpeed))
		cass_statement_bind_float(statement, 40, from_mph_to_kph(_obs.windSpeed));
	/*************************************************************/
	if (!isInvalid(_obs.windDir))
		cass_statement_bind_int32(statement, 41, _obs.windDir);
	/*************************************************************/
	// No 10-min or 2-min average wind speed
	/*************************************************************/
	if (!isInvalid(_obs.windGustSpeed))
		cass_statement_bind_float(statement, 44, from_mph_to_kph(_obs.windGustSpeed));
	/*************************************************************/
	// No max wind speed dir
	/*************************************************************/
	// No rainrate
	/*************************************************************/
	// No avg rain rate over hour/day/...
	/*************************************************************/
	// No storm measurement
	/*************************************************************/
	if (!isInvalid(_obs.uvIndex))
		cass_statement_bind_int32(statement, 55, int(_obs.uvIndex * 10));
	/*************************************************************/
	if (!isInvalid(_obs.solarRad))
		cass_statement_bind_int32(statement, 56, _obs.solarRad);
	/*************************************************************/
	if (!isInvalid(_obs.temperature) && !isInvalid(_obs.humidity))
		cass_statement_bind_float(statement, 57,
			dew_point(
				_obs.temperature,
				_obs.humidity
			)
		);
	/*************************************************************/
	if (!isInvalid(_obs.temperatureF) && !isInvalid(_obs.humidity))
		cass_statement_bind_float(statement, 58,
			heat_index(
				_obs.temperatureF,
				_obs.humidity
			)
		);
	/*************************************************************/
	if (!isInvalid(_obs.temperatureF) && !isInvalid(_obs.windSpeed))
		cass_statement_bind_float(statement, 59,
			wind_chill(
				_obs.temperatureF,
				_obs.windSpeed
			)
		);
	/*************************************************************/
	// No THSW
	/*************************************************************/
	// ET is not exploitable, it's given over the last hour
	/*************************************************************/
	// No forecast
	/*************************************************************/
	// No forecast icons
	/*************************************************************/
	// No sunrise time
	/*************************************************************/
	// No sunset time
	/*************************************************************/
	// No rain nor ET
}

void WeatherlinkApiRealtimeMessage::populateV2DataPoint(const CassUuid station, CassStatement* const statement) const
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
		_obs.time.time_since_epoch().count()
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
	if (!isInvalid(_obs.temperatureF) && !isInvalid(_obs.humidity))
		cass_statement_bind_float(statement, 10,
			heat_index(
				_obs.temperatureF,
				_obs.humidity
			)
		);
	/*************************************************************/
	// No inside humidity
	/*************************************************************/
	// No inside temperature
	/*************************************************************/
	// No leaf temp, no leaf wetnesses
	/*************************************************************/
	if (!isInvalid(_obs.humidity))
		cass_statement_bind_int32(statement, 17, _obs.humidity);
	/*************************************************************/
	if (!isInvalid(_obs.temperature))
		cass_statement_bind_float(statement, 18, _obs.temperature);
	/*************************************************************/
	// No rain
	/*************************************************************/
	// No rain
	/*************************************************************/
	// No ETP
	/*************************************************************/
	// No soil moisture
	/*************************************************************/
	// No soil temperature
	/*************************************************************/
	if (!isInvalid(_obs.solarRad))
		cass_statement_bind_int32(statement, 30, _obs.solarRad);
	/*************************************************************/
	if (!isInvalid(_obs.temperatureF) && !isInvalid(_obs.windSpeed)
	 && !isInvalid(_obs.humidity) && !isInvalid(_obs.solarRad))
		cass_statement_bind_float(statement, 31,
			thsw_index(
				_obs.temperatureF,
				_obs.humidity,
				_obs.windSpeed,
				_obs.solarRad
			)
		);
	else if (!isInvalid(_obs.temperatureF) && !isInvalid(_obs.windSpeed)
	 && !isInvalid(_obs.humidity) && isInvalid(_obs.solarRad))
		cass_statement_bind_float(statement, 31,
			thsw_index(
				_obs.temperatureF,
				_obs.humidity,
				_obs.windSpeed
			)
		);
	/*************************************************************/
	if (!isInvalid(_obs.uvIndex))
		cass_statement_bind_int32(statement, 32, int(_obs.uvIndex * 10));
	/*************************************************************/
	if (!isInvalid(_obs.temperatureF) && !isInvalid(_obs.windSpeed))
		cass_statement_bind_float(statement, 33,
			wind_chill(
				_obs.temperatureF,
				_obs.windSpeed
			)
		);
	/*************************************************************/
	if (!isInvalid(_obs.windDir))
		cass_statement_bind_int32(statement, 34, _obs.windDir);
	/*************************************************************/
	if (!isInvalid(_obs.windGustSpeed))
		cass_statement_bind_float(statement, 35, from_mph_to_kph(_obs.windGustSpeed));
	/*************************************************************/
	if (!isInvalid(_obs.windSpeed))
		cass_statement_bind_float(statement, 36, from_mph_to_kph(_obs.windSpeed));
	/*************************************************************/
	// No insolation
	/*************************************************************/
}
}