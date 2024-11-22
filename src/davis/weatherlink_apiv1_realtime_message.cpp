/**
 * @file weatherlink_apiv1_realtime_message.cpp
 * @brief Implementation of the WeatherlinkApiv1RealtimeMessage class
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
#include <cassobs/message.h>

#include "vantagepro2_message.h"
#include "weatherlink_apiv1_realtime_message.h"
#include "../time_offseter.h"

namespace meteodata
{

namespace chrono = std::chrono;
namespace pt = boost::property_tree;

WeatherlinkApiv1RealtimeMessage::WeatherlinkApiv1RealtimeMessage(const TimeOffseter* timeOffseter) :
		AbstractWeatherlinkApiMessage(timeOffseter)
{}

void WeatherlinkApiv1RealtimeMessage::parse(std::istream& input)
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
	_obs.windGustSpeed = xmlTree.get<float>("current_observation.davis_current_observation.wind_ten_min_gust_mph",
											INVALID_FLOAT);
	_obs.rainRate = xmlTree.get<float>("current_observation.davis_current_observation.rain_rate_in_per_hr",
									   INVALID_FLOAT);
	if (_obs.rainRate != INVALID_FLOAT)
		_obs.rainRate = from_in_to_mm(_obs.rainRate);
	_obs.solarRad = xmlTree.get<int>("current_observation.davis_current_observation.solar_radiation", INVALID_INT);
	_obs.uvIndex = xmlTree.get<float>("current_observation.davis_current_observation.uv_index", INVALID_FLOAT);
	for (int i = 0 ; i < 2 ; i++)
		_obs.extraHumidity[i] = xmlTree.get<int>(
				"current_observation.davis_current_observation.relative_humidity_" + std::to_string(i + 1),
				INVALID_INT);
	for (int i = 0 ; i < 3 ; i++)
		_obs.extraTemperature[i] = xmlTree.get<float>(
				"current_observation.davis_current_observation.temp_extra_" + std::to_string(i + 1), INVALID_FLOAT);
	for (int i = 0 ; i < 2 ; i++) {
		_obs.leafTemperature[i] = xmlTree.get<float>(
				"current_observation.davis_current_observation.temp_leaf_" + std::to_string(i + 1), INVALID_FLOAT);
		_obs.leafWetness[i] = xmlTree.get<int>(
				"current_observation.davis_current_observation.leaf_wetness_" + std::to_string(i + 1), INVALID_INT);
	}
	for (int i = 0 ; i < 4 ; i++) {
		_obs.soilMoisture[i] = xmlTree.get<int>(
				"current_observation.davis_current_observation.soil_moisture_" + std::to_string(i + 1), INVALID_INT);
		_obs.soilTemperature[i] = xmlTree.get<float>(
				"current_observation.davis_current_observation.temp_soil_" + std::to_string(i + 1), INVALID_INT);
	}
}

}
