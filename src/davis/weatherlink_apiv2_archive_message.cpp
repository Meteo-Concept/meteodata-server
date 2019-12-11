/**
 * @file weatherlink_apiv2_archive_message.cpp
 * @brief Implementation of the WeatherlinkApiv2ArchiveMessage class
 * @author Laurent Georget
 * @date 2019-09-09
 */
/*
 * Copyright (C) 2019  SAS Météo Concept <contact@meteo-concept.fr>
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
#include <boost/property_tree/json_parser.hpp>
#include <cassandra.h>
#include <message.h>

#include "vantagepro2_message.h"
#include "weatherlink_apiv2_archive_message.h"

namespace meteodata {

namespace chrono = std::chrono;
namespace pt = boost::property_tree;

using SensorType = WeatherlinkApiv2ArchiveMessage::SensorType;
using DataStructureType = WeatherlinkApiv2ArchiveMessage::DataStructureType;

WeatherlinkApiv2ArchiveMessage::WeatherlinkApiv2ArchiveMessage() :
	AbstractWeatherlinkApiMessage()
{}

void WeatherlinkApiv2ArchiveMessage::parse(std::istream& input)
{
	pt::ptree jsonTree;
	pt::read_json(input, jsonTree);

	for (std::pair<const std::string, pt::ptree>& reading : jsonTree.get_child("sensors")) {
		SensorType sensorType = static_cast<SensorType>(reading.second.get<int>("sensor_type"));
		DataStructureType dataStructureType = static_cast<DataStructureType>(reading.second.get<int>("data_structure_type"));
		// Only parse the last (most recent) element of the collection of data
		auto data = reading.second.get_child("data").back().second;
		ingest(data, sensorType, dataStructureType);
	}
}

void WeatherlinkApiv2ArchiveMessage::ingest(const pt::ptree& data, SensorType sensorType, DataStructureType dataStructureType)
{
	int catalogType = static_cast<int>(sensorType);
	if (((catalogType >= 43 && catalogType <= 52) || sensorType == SensorType::VANTAGE_VUE_ISS) &&
	    dataStructureType == DataStructureType::WEATHERLINK_LIVE_ISS_ARCHIVE_RECORD) {
		_obs.time = date::floor<chrono::milliseconds>(chrono::system_clock::from_time_t(data.get<time_t>("ts")));
		float hum = data.get<float>("hum_last", INVALID_FLOAT) ;
		if (!isInvalid(hum))
			_obs.humidity = static_cast<int>(hum);
		_obs.temperatureF = data.get<float>("temp_last", INVALID_FLOAT);
		if (!isInvalid(_obs.temperatureF))
			_obs.temperature = from_Farenheight_to_Celsius(_obs.temperatureF);
		_obs.windDir = data.get<int>("wind_dir_of_prevail", INVALID_INT);
		_obs.windSpeed = data.get<float>("wind_speed_avg", INVALID_FLOAT);
		_obs.windGustSpeed = data.get<float>("wind_speed_hi", INVALID_FLOAT);
		_obs.rainRate = data.get<float>("rain_rate_hi_in", INVALID_FLOAT);
		_obs.rainFall = data.get<int>("rainfall_clicks", INVALID_INT);
		_obs.solarRad = data.get<int>("solar_rad_avg", INVALID_INT);
		_obs.uvIndex = data.get<float>("uv_index_avg", INVALID_FLOAT);
	} else if (sensorType == SensorType::SENSOR_SUITE &&
		    dataStructureType == DataStructureType::WEATHERLINK_LIVE_ISS_ARCHIVE_RECORD) {
		// This data package must be ingested after the ISS data
		float hum = data.get<float>("hum", INVALID_FLOAT) ;
		if (!isInvalid(hum))
			_obs.humidity = static_cast<int>(hum);
		_obs.temperatureF = data.get<float>("temp", INVALID_FLOAT);
		if (!isInvalid(_obs.temperatureF))
			_obs.temperature = from_Farenheight_to_Celsius(_obs.temperatureF);
		_obs.windDir = data.get<int>("wind_dir_scalar_avg_last_10_min", INVALID_INT);
		_obs.windSpeed = data.get<float>("wind_speed_avg_last_10_min", INVALID_FLOAT);
		_obs.windGustSpeed = data.get<float>("wind_speed_hi_last_10_min", INVALID_FLOAT);
		_obs.rainRate = data.get<float>("rain_rate_hi_in", INVALID_FLOAT);
		_obs.rainFall = data.get<int>("rainfall_last_15_min_clicks", INVALID_INT);
		_obs.solarRad = data.get<int>("solar_rad", INVALID_INT);
		_obs.uvIndex = data.get<float>("uv_index", INVALID_FLOAT);
	} else if (sensorType == SensorType::BAROMETER &&
		   dataStructureType == DataStructureType::BAROMETER_ARCHIVE_RECORD) {
		_obs.time = date::floor<chrono::milliseconds>(chrono::system_clock::from_time_t(data.get<time_t>("ts")));
		_obs.pressure = data.get<float>("bar_sea_level", INVALID_FLOAT);
		if (!isInvalid(_obs.pressure))
			_obs.pressure = from_inHg_to_bar(_obs.pressure) * 1000;
	}
}

}
