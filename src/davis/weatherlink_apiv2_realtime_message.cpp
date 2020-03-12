/**
 * @file weatherlink_apiv2_realtime_message.cpp
 * @brief Implementation of the WeatherlinkApiv1RealtimeMessage class
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
#include <vector>
#include <algorithm>
#include <functional>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <cassandra.h>
#include <message.h>

#include "vantagepro2_message.h"
#include "weatherlink_apiv2_realtime_message.h"
#include "weatherlink_apiv2_parser_trait.h"
#include "../time_offseter.h"
#include "../cassandra_utils.h"

namespace meteodata {

namespace chrono = std::chrono;
namespace pt = boost::property_tree;

using SensorType = WeatherlinkApiv2RealtimeMessage::SensorType;
using DataStructureType = WeatherlinkApiv2RealtimeMessage::DataStructureType;

namespace {
	constexpr bool compareDataPackages(
		const std::tuple<SensorType, DataStructureType, pt::ptree>& entry1,
		const std::tuple<SensorType, DataStructureType, pt::ptree>& entry2
	) {
		SensorType sensorType2 = std::get<0>(entry2);
		int catalogType2 = static_cast<int>(sensorType2);
		// Ingest first the ISS so that when reading the data from the aux. sensor suites,
		// we can check for the missing data
		// The ordering of the rest is irrelevant.
		if (std::get<0>(entry1) == SensorType::SENSOR_SUITE &&
		    ((catalogType2 >= 43 && catalogType2 <= 52) || sensorType2 == SensorType::VANTAGE_VUE_ISS))
			return false;

		return true;
	}
}

WeatherlinkApiv2RealtimeMessage::WeatherlinkApiv2RealtimeMessage(const TimeOffseter* timeOffseter) :
	AbstractWeatherlinkApiMessage(timeOffseter),
	WeatherlinkApiv2ParserTrait()
{}

void WeatherlinkApiv2RealtimeMessage::parse(std::istream& input)
{
	doParse(input, std::bind(&WeatherlinkApiv2RealtimeMessage::acceptEntry, this, std::placeholders::_1));
}

void WeatherlinkApiv2RealtimeMessage::parse(std::istream& input, const std::map<int, CassUuid>& substations, const CassUuid& station)
{
	doParse(input, std::bind(&WeatherlinkApiv2RealtimeMessage::acceptEntryWithSubstations, this, std::placeholders::_1, substations, station));
}

void WeatherlinkApiv2RealtimeMessage::doParse(std::istream& input, const Acceptor& acceptable)
{
	pt::ptree jsonTree;
	pt::read_json(input, jsonTree);

	std::vector<std::tuple<SensorType, DataStructureType, pt::ptree>> entries;

	for (std::pair<const std::string, pt::ptree>& reading : jsonTree.get_child("sensors")) {
		if (!acceptable(reading))
			continue;

		// we expect exactly one element, the current condition
		auto data = reading.second.get_child("data").front().second;
		SensorType sensorType = static_cast<SensorType>(reading.second.get<int>("sensor_type"));
		DataStructureType dataStructureType = static_cast<DataStructureType>(reading.second.get<int>("data_structure_type"));
		entries.push_back(std::make_tuple(sensorType, dataStructureType, data));
	}

	std::sort(entries.begin(), entries.end(), &compareDataPackages);

	for (const auto& entry : entries) {
		SensorType sensorType;
		DataStructureType dataStructureType;
		pt::ptree data;
		std::tie(sensorType, dataStructureType, data) = entry;

		int catalogType = static_cast<int>(sensorType);
		if (((catalogType >= 43 && catalogType <= 52) || sensorType == SensorType::VANTAGE_VUE_ISS) &&
		    dataStructureType == DataStructureType::WEATHERLINK_LIVE_CURRENT_READING) {
			_obs.time = date::sys_time<chrono::milliseconds>(chrono::seconds(data.get<time_t>("ts")));
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
		}

		if (sensorType == SensorType::SENSOR_SUITE &&
		    dataStructureType == DataStructureType::WEATHERLINK_LIVE_CURRENT_READING) {
			if (isInvalid(_obs.humidity)) {
				float hum = data.get<float>("hum", INVALID_FLOAT) ;
				if (!isInvalid(hum))
					_obs.humidity = static_cast<int>(hum);
			}
			if (isInvalid(_obs.temperature)) {
				_obs.temperatureF = data.get<float>("temp", INVALID_FLOAT);
				if (!isInvalid(_obs.temperatureF))
					_obs.temperature = from_Farenheight_to_Celsius(_obs.temperatureF);
			}
			if (isInvalid(_obs.windDir)) {
				_obs.windDir = data.get<int>("wind_dir_scalar_avg_last_10_min", INVALID_INT);
			}
			if (isInvalid(_obs.windSpeed)) {
				_obs.windSpeed = data.get<float>("wind_speed_avg_last_10_min", INVALID_FLOAT);
			}
			if (isInvalid(_obs.windGustSpeed)) {
				_obs.windGustSpeed = data.get<float>("wind_speed_hi_last_10_min", INVALID_FLOAT);
			}
			if (isInvalid(_obs.rainRate)) {
				_obs.rainRate = data.get<float>("rain_rate_hi_in", INVALID_FLOAT);
			}
			if (isInvalid(_obs.rainFall)) {
				_obs.rainFall = data.get<int>("rainfall_last_15_min_clicks", INVALID_INT);
			}
			if (isInvalid(_obs.solarRad)) {
				_obs.solarRad = data.get<int>("solar_rad", INVALID_INT);
			}
			if (isInvalid(_obs.uvIndex)) {
				_obs.uvIndex = data.get<float>("uv_index", INVALID_FLOAT);
			}
		}

		if (sensorType == SensorType::BAROMETER && dataStructureType == DataStructureType::BAROMETER_CURRENT_READING) {
			_obs.pressure = data.get<float>("bar_sea_level", INVALID_FLOAT);
			if (!isInvalid(_obs.pressure))
				_obs.pressure = from_inHg_to_bar(_obs.pressure) * 1000;
		}
	}
}

}
