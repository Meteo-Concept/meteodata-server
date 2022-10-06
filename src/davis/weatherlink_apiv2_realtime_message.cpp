/**
 * @file weatherlink_apiv2_realtime_message.cpp
 * @brief Implementation of the WeatherlinkApiv2RealtimeMessage class
 * @author Laurent Georget
 * @date 2022-10-04
 */
/*
 * Copyright (C) 2022  SAS Météo Concept <contact@meteo-concept.fr>
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
#include <cmath>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <cassandra.h>
#include <message.h>

#include "vantagepro2_message.h"
#include "weatherlink_apiv2_realtime_message.h"
#include "../time_offseter.h"

namespace meteodata
{

namespace chrono = std::chrono;
namespace pt = boost::property_tree;

using SensorType = WeatherlinkApiv2RealtimeMessage::SensorType;
using DataStructureType = WeatherlinkApiv2RealtimeMessage::DataStructureType;

WeatherlinkApiv2RealtimeMessage::WeatherlinkApiv2RealtimeMessage(const TimeOffseter* timeOffseter, float dayRain) :
		AbstractWeatherlinkApiMessage(timeOffseter),
		_dayRain{dayRain}
{}

void WeatherlinkApiv2RealtimeMessage::parse(std::istream& input)
{
	pt::ptree jsonTree;
	pt::read_json(input, jsonTree);

	for (std::pair<const std::string, pt::ptree>& reading : jsonTree.get_child("sensors")) {
		SensorType sensorType = static_cast<SensorType>(reading.second.get<int>("sensor_type"));
		DataStructureType dataStructureType = static_cast<DataStructureType>(reading.second.get<int>("data_structure_type"));

		auto dataIt = reading.second.find("data");
		if (dataIt == reading.second.not_found() || dataIt->second.empty())
			continue;
		// Only parse the last (most recent) element of the collection of data
		auto data = dataIt->second.back().second;
		ingest(data, sensorType, dataStructureType);
	}
}

void WeatherlinkApiv2RealtimeMessage::ingest(const pt::ptree& data, wlv2structures::AbstractParser& dedicatedParser)
{
	dedicatedParser.parse(_obs, data);
}

void WeatherlinkApiv2RealtimeMessage::ingest(const pt::ptree& data, SensorType sensorType,
	DataStructureType dataStructureType)
{
	if (isMainStationType(sensorType) && dataStructureType == DataStructureType::WEATHERLINK_LIVE_CURRENT_READING) {
		_obs.time = date::sys_time<chrono::milliseconds>(chrono::seconds(data.get<time_t>("ts")));
		float hum = data.get<float>("hum", INVALID_FLOAT);
		if (!isInvalid(hum))
			_obs.humidity = static_cast<int>(hum);
		_obs.temperatureF = data.get<float>("temp", INVALID_FLOAT);
		if (!isInvalid(_obs.temperatureF))
			_obs.temperature = from_Farenheit_to_Celsius(_obs.temperatureF);
		_obs.windDir = data.get<int>("wind_dir_scalar_avg_last_10_min", INVALID_INT);
		_obs.windSpeed = data.get<float>("wind_speed_avg_last_10_min", INVALID_FLOAT);
		_obs.windGustSpeed = data.get<float>("wind_speed_hi_last_10_min", INVALID_FLOAT);
		auto rainRate = data.get<int>("rain_rate_hi_clicks", INVALID_INT);
		if (!isInvalid(rainRate))
			_obs.rainRate = from_rainrate_to_mm(rainRate);
		auto rainFall = data.get<int>("rainfall_daily_clicks", INVALID_INT);
		if (!isInvalid(rainFall)) {
			_newDayRain = from_rainrate_to_mm(rainFall);
			float diff = from_rainrate_to_mm(rainFall) - _dayRain;
			if (diff > -0.1) { // don't compare with exactly 0 because of rouding errors
				// If the diff is negative,
				// either the station clock is off or we
				// are not looking at the correct reset
				// time to compute _dayRain.
				_obs.rainFall = diff;
			}
		}
		_obs.solarRad = data.get<int>("solar_rad", INVALID_INT);
		_obs.uvIndex = data.get<float>("uv_index", INVALID_FLOAT);
	}

	if (isMainStationType(sensorType) &&
		dataStructureType == DataStructureType::WEATHERLINK_IP_CURRENT_READING_REVISION_B) {
		_obs.time = date::sys_time<chrono::milliseconds>(chrono::seconds(data.get<time_t>("ts")));
		_obs.pressure = data.get<float>("bar", INVALID_FLOAT);
		if (!isInvalid(_obs.pressure))
			_obs.pressure = from_inHg_to_bar(_obs.pressure) * 1000;
		float hum = data.get<float>("hum_out", INVALID_FLOAT);
		if (!isInvalid(hum))
			_obs.humidity = static_cast<int>(hum);
		_obs.temperatureF = data.get<float>("temp_out", INVALID_FLOAT);
		if (!isInvalid(_obs.temperatureF))
			_obs.temperature = from_Farenheit_to_Celsius(_obs.temperatureF);
		_obs.windDir = data.get<int>("wind_dir", INVALID_INT);
		_obs.windSpeed = data.get<float>("wind_speed_10_min_avg", INVALID_FLOAT);
		_obs.windGustSpeed = data.get<float>("wind_speed", INVALID_FLOAT);
		auto rainRate = data.get<int>("rain_rate_clicks", INVALID_INT);
		if (!isInvalid(rainRate))
			_obs.rainRate = from_rainrate_to_mm(rainRate);
		auto rainFall = data.get<int>("rain_day_clicks", INVALID_INT);
		if (!isInvalid(rainFall)) {
			_newDayRain = from_rainrate_to_mm(rainFall);
			float diff = from_rainrate_to_mm(rainFall) - _dayRain;
			if (diff > -0.1) {
				_obs.rainFall = diff;
			}
		}
		_obs.solarRad = data.get<int>("solar_rad", INVALID_INT);
		_obs.uvIndex = data.get<float>("uv", INVALID_FLOAT);
		_obs.extraHumidity[0] = data.get<int>("hum_extra_1", INVALID_INT);
		_obs.extraHumidity[1] = data.get<int>("hum_extra_2", INVALID_INT);
		_obs.extraTemperature[0] = data.get<float>("temp_extra_1", INVALID_FLOAT);
		_obs.extraTemperature[1] = data.get<float>("temp_extra_2", INVALID_FLOAT);
		_obs.extraTemperature[2] = data.get<float>("temp_extra_3", INVALID_FLOAT);
		_obs.leafTemperature[0] = data.get<float>("temp_leaf_1", INVALID_FLOAT);
		_obs.leafTemperature[1] = data.get<float>("temp_leaf_2", INVALID_FLOAT);
		_obs.leafWetness[0] = data.get<int>("wet_leaf_1", INVALID_INT);
		_obs.leafWetness[1] = data.get<int>("wet_leaf_2", INVALID_INT);
		_obs.soilMoisture[0] = data.get<int>("moist_soil_1", INVALID_INT);
		_obs.soilMoisture[1] = data.get<int>("moist_soil_2", INVALID_INT);
		_obs.soilMoisture[2] = data.get<int>("moist_soil_3", INVALID_INT);
		_obs.soilMoisture[3] = data.get<int>("moist_soil_4", INVALID_INT);
		_obs.soilTemperature[0] = data.get<float>("temp_soil_1", INVALID_FLOAT);
		_obs.soilTemperature[1] = data.get<float>("temp_soil_2", INVALID_FLOAT);
		_obs.soilTemperature[2] = data.get<float>("temp_soil_3", INVALID_FLOAT);
		_obs.soilTemperature[3] = data.get<float>("temp_soil_4", INVALID_FLOAT);
	}

	if (isMainStationType(sensorType) &&
		dataStructureType == DataStructureType::ENVIROMONITOR_ISS_CURRENT_READING) {
		_obs.time = date::sys_time<chrono::milliseconds>(chrono::seconds(data.get<time_t>("ts")));
		_obs.pressure = data.get<float>("bar", INVALID_FLOAT);
		if (!isInvalid(_obs.pressure))
			_obs.pressure = from_inHg_to_bar(_obs.pressure) * 1000;
		float hum = data.get<float>("hum_out", INVALID_FLOAT);
		if (!isInvalid(hum))
			_obs.humidity = static_cast<int>(hum);
		_obs.temperatureF = data.get<float>("temp_out", INVALID_FLOAT);
		if (!isInvalid(_obs.temperatureF))
			_obs.temperature = from_Farenheit_to_Celsius(_obs.temperatureF);
		_obs.windDir = data.get<int>("wind_dir", INVALID_INT);
		_obs.windSpeed = data.get<float>("wind_speed_10_min", INVALID_FLOAT);
		_obs.windGustSpeed = data.get<float>("wind_gust_10_min", INVALID_FLOAT);
		auto rainRate = data.get<int>("rain_rate_clicks", INVALID_INT);
		if (!isInvalid(rainRate))
			_obs.rainRate = from_rainrate_to_mm(rainRate);
		auto rainFall = data.get<int>("rain_day_clicks", INVALID_INT);
		if (!isInvalid(rainFall)) {
			_newDayRain = from_rainrate_to_mm(rainFall);
			float diff = from_rainrate_to_mm(rainFall) - _dayRain;
			if (diff > -0.1) {
				_obs.rainFall = diff;
			}
		}
		_obs.solarRad = data.get<int>("solar_rad", INVALID_INT);
		_obs.uvIndex = data.get<float>("uv", INVALID_FLOAT);
	}

	if (sensorType == SensorType::SENSOR_SUITE &&
		dataStructureType == DataStructureType::WEATHERLINK_LIVE_CURRENT_READING) {
		_obs.time = date::sys_time<chrono::milliseconds>(chrono::seconds(data.get<time_t>("ts")));
		if (isInvalid(_obs.humidity)) {
			float hum = data.get<float>("hum", INVALID_FLOAT);
			if (!isInvalid(hum))
				_obs.humidity = static_cast<int>(hum);
		}
		if (isInvalid(_obs.temperature)) {
			_obs.temperatureF = data.get<float>("temp", INVALID_FLOAT);
			if (!isInvalid(_obs.temperatureF))
				_obs.temperature = from_Farenheit_to_Celsius(_obs.temperatureF);
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
			auto rainRate = data.get<int>("rain_rate_hi_clicks", INVALID_INT);
			if (!isInvalid(rainRate))
				_obs.rainRate = from_rainrate_to_mm(rainRate);
		}
		if (isInvalid(_obs.rainFall)) {
			auto rainFall = data.get<int>("rainfall_last_15_min_clicks", INVALID_INT);
			if (!isInvalid(rainFall))
				_obs.rainFall = from_rainrate_to_mm(rainFall);
		}
		if (isInvalid(_obs.solarRad)) {
			_obs.solarRad = data.get<int>("solar_rad", INVALID_INT);
		}
		if (isInvalid(_obs.uvIndex)) {
			_obs.uvIndex = data.get<float>("uv_index", INVALID_FLOAT);
		}
	}

	if (sensorType == SensorType::BAROMETER &&
		dataStructureType == DataStructureType::WEATHERLINK_LIVE_NON_ISS_CURRENT_READING) {
		_obs.time = date::sys_time<chrono::milliseconds>(chrono::seconds(data.get<time_t>("ts")));
		_obs.pressure = data.get<float>("bar_sea_level", INVALID_FLOAT);
		if (!isInvalid(_obs.pressure))
			_obs.pressure = from_inHg_to_bar(_obs.pressure) * 1000;
	}

	if (sensorType == SensorType::LEAF_SOIL_SUBSTATION &&
		dataStructureType == DataStructureType::WEATHERLINK_LIVE_NON_ISS_CURRENT_READING) {
		_obs.time = date::sys_time<chrono::milliseconds>(chrono::seconds(data.get<time_t>("ts")));
		// The first two temperatures are put in both leaf and soil temperatures fields
		// because we cannot know from the API where the user installed the sensors
		// It's necessary to enable/disable the corresponding sensors from the administration
		// page in the Meteodata website.
		// The temperature conversions are done in the message insertion methods
		_obs.leafTemperature[0] = data.get<float>("temp_1", INVALID_FLOAT);
		_obs.leafTemperature[1] = data.get<float>("temp_2", INVALID_FLOAT);
		_obs.soilTemperature[0] = data.get<float>("temp_1", INVALID_FLOAT);
		_obs.soilTemperature[1] = data.get<float>("temp_2", INVALID_FLOAT);
		_obs.soilTemperature[2] = data.get<float>("temp_3", INVALID_FLOAT);
		_obs.soilTemperature[3] = data.get<float>("temp_4", INVALID_FLOAT);
		_obs.extraTemperature[0] = data.get<float>("temp_1", INVALID_FLOAT);
		_obs.extraTemperature[1] = data.get<float>("temp_2", INVALID_FLOAT);
		_obs.extraTemperature[2] = data.get<float>("temp_3", INVALID_FLOAT);
		// The APIv2 returns a float for leaf wetness and soil moisture but we store an int
		float temp;
		temp = data.get<float>("wet_leaf_1", INVALID_FLOAT);
		_obs.leafWetness[0] = isInvalid(temp) ? INVALID_INT : std::lround(temp);
		temp = data.get<float>("wet_leaf_2", INVALID_FLOAT);
		_obs.leafWetness[1] = isInvalid(temp) ? INVALID_INT : std::lround(temp);
		temp = data.get<float>("moist_soil_1", INVALID_FLOAT);
		_obs.soilMoisture[0] = isInvalid(temp) ? INVALID_INT : std::lround(temp);
		temp = data.get<float>("moist_soil_2", INVALID_FLOAT);
		_obs.soilMoisture[1] = isInvalid(temp) ? INVALID_INT : std::lround(temp);
		temp = data.get<float>("moist_soil_3", INVALID_FLOAT);
		_obs.soilMoisture[2] = isInvalid(temp) ? INVALID_INT : std::lround(temp);
		temp = data.get<float>("moist_soil_4", INVALID_FLOAT);
		_obs.soilMoisture[3] = isInvalid(temp) ? INVALID_INT : std::lround(temp);
	}

	if (sensorType == SensorType::ANEMOMETER) {
		_obs.time = date::sys_time<chrono::milliseconds>(chrono::seconds(data.get<time_t>("ts")));
		_obs.windDir = data.get<int>("wind_dir_prevail", INVALID_INT);
		_obs.windSpeed = data.get<float>("wind_speed_avg_last_10_min", INVALID_FLOAT);
		_obs.windGustSpeed = data.get<float>("wind_speed_hi", INVALID_FLOAT);
	}
}

}
