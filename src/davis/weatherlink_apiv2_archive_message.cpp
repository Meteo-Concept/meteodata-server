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
#include <cmath>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <cassandra.h>
#include <cassobs/message.h>

#include "vantagepro2_message.h"
#include "weatherlink_apiv2_archive_message.h"
#include "../time_offseter.h"

namespace meteodata
{

namespace chrono = std::chrono;
namespace pt = boost::property_tree;

using SensorType = WeatherlinkApiv2ArchiveMessage::SensorType;
using DataStructureType = WeatherlinkApiv2ArchiveMessage::DataStructureType;

WeatherlinkApiv2ArchiveMessage::WeatherlinkApiv2ArchiveMessage(const TimeOffseter* timeOffseter) :
		AbstractWeatherlinkApiMessage(timeOffseter)
{}

float WeatherlinkApiv2ArchiveMessage::extractRainFall(const pt::ptree& data)
{
	// Sometimes the rainfall is not available in clicks but only in inches
	// in the API messages (maybe when the device is a datalogger IP?)
	auto rainFall = data.get<int>("rainfall_clicks", INVALID_INT);
	if (!isInvalid(rainFall)) {
		return from_rainrate_to_mm(rainFall);
	} else {
		auto rainFallIn = data.get<float>("rainfall_in", INVALID_FLOAT);
		if (!isInvalid(rainFallIn)) {
			return from_in_to_mm(rainFallIn);
		}
	}
	return INVALID_FLOAT;
}

float WeatherlinkApiv2ArchiveMessage::extractRainRate(const pt::ptree& data)
{
	auto rainRate = data.get<int>("rain_rate_in_clicks", INVALID_INT);
	if (!isInvalid(rainRate)) {
		return from_rainrate_to_mm(rainRate);
	} else {
		auto rainRateIn = data.get<float>("rain_rate_hi_in", INVALID_FLOAT);
		if (!isInvalid(rainRateIn)) {
			return from_in_to_mm(rainRateIn);
		}
	}
	return INVALID_FLOAT;
}

void WeatherlinkApiv2ArchiveMessage::parse(std::istream& input)
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

void WeatherlinkApiv2ArchiveMessage::ingest(const pt::ptree& data, wlv2structures::AbstractParser& dedicatedParser)
{
	dedicatedParser.parse(_obs, data);
}

void WeatherlinkApiv2ArchiveMessage::ingest(const pt::ptree& data, SensorType sensorType,
		DataStructureType dataStructureType)
{
	if (isMainStationType(sensorType) &&
			(dataStructureType == DataStructureType::WEATHERLINK_LIVE_ISS_ARCHIVE_RECORD ||
			 dataStructureType == DataStructureType::WEATHERLINK_CONSOLE_ISS_ARCHIVE_RECORD)) {
		_obs.time = date::floor<chrono::milliseconds>(chrono::system_clock::from_time_t(data.get<time_t>("ts")));
		float hum = data.get<float>("hum_last", INVALID_FLOAT);
		if (!isInvalid(hum))
			_obs.humidity = static_cast<int>(hum);
		_obs.temperatureF = data.get<float>("temp_last", INVALID_FLOAT);
		if (!isInvalid(_obs.temperatureF))
			_obs.temperature = from_Farenheit_to_Celsius(_obs.temperatureF);
		_obs.temperatureMinF = data.get<float>("temp_lo", INVALID_FLOAT);
		if (!isInvalid(_obs.temperatureMinF))
			_obs.minTemperature = from_Farenheit_to_Celsius(_obs.temperatureMinF);
		_obs.temperatureMaxF = data.get<float>("temp_hi", INVALID_FLOAT);
		if (!isInvalid(_obs.temperatureMaxF))
			_obs.maxTemperature = from_Farenheit_to_Celsius(_obs.temperatureMaxF);
		_obs.windDir = data.get<int>("wind_dir_of_prevail", INVALID_INT);
		_obs.windSpeed = data.get<float>("wind_speed_avg", INVALID_FLOAT);
		_obs.windGustSpeed = data.get<float>("wind_speed_hi", INVALID_FLOAT);
		auto rainRate = extractRainRate(data);
		if (!isInvalid(rainRate))
			_obs.rainRate = rainRate;
		auto rainFall = extractRainFall(data);
		if (!isInvalid(rainFall))
			_obs.rainFall = rainFall;
		_obs.solarRad = data.get<int>("solar_rad_avg", INVALID_INT);
		_obs.uvIndex = data.get<float>("uv_index_avg", INVALID_FLOAT);

		_obs.supercapVoltage = data.get<float>("supercap_volt_last", INVALID_FLOAT);
		_obs.solarPanelVoltage = data.get<float>("solar_volt_last", INVALID_FLOAT);
		_obs.backupVoltage = data.get<float>("trans_battery", INVALID_FLOAT);
	} else if (isMainStationType(sensorType) &&
			(dataStructureType == DataStructureType::WEATHERLINK_IP_ARCHIVE_RECORD_REVISION_B ||
			 dataStructureType == DataStructureType::ENVIROMONITOR_ISS_ARCHIVE_RECORD)) {
		_obs.time = date::floor<chrono::milliseconds>(chrono::system_clock::from_time_t(data.get<time_t>("ts")));
		float hum = data.get<float>("hum_out", INVALID_FLOAT);
		if (!isInvalid(hum))
			_obs.humidity = static_cast<int>(hum);
		_obs.temperatureF = data.get<float>("temp_out", INVALID_FLOAT);
		if (!isInvalid(_obs.temperatureF))
			_obs.temperature = from_Farenheit_to_Celsius(_obs.temperatureF);
		_obs.temperatureMinF = data.get<float>("temp_out_lo", INVALID_FLOAT);
		if (!isInvalid(_obs.temperatureMinF))
			_obs.minTemperature = from_Farenheit_to_Celsius(_obs.temperatureMinF);
		_obs.temperatureMaxF = data.get<float>("temp_out_hi", INVALID_FLOAT);
		if (!isInvalid(_obs.temperatureMaxF))
			_obs.maxTemperature = from_Farenheit_to_Celsius(_obs.temperatureMaxF);
		_obs.pressure = data.get<float>("bar", INVALID_FLOAT);
		if (!isInvalid(_obs.pressure))
			_obs.pressure = from_inHg_to_bar(_obs.pressure) * 1000;
		int windDir = data.get<int>("wind_dir_of_prevail", INVALID_INT);
		if (!isInvalid(windDir))
			_obs.windDir = static_cast<int>(windDir * 22.5);
		_obs.windSpeed = data.get<float>("wind_speed_avg", INVALID_FLOAT);
		_obs.windGustSpeed = data.get<float>("wind_speed_hi", INVALID_FLOAT);
		auto rainRate = extractRainRate(data);
		if (!isInvalid(rainRate))
			_obs.rainRate = rainRate;
		auto rainFall = extractRainFall(data);
		if (!isInvalid(rainFall))
			_obs.rainFall = rainFall;
		_obs.solarRad = data.get<int>("solar_rad_avg", INVALID_INT);
		_obs.uvIndex = data.get<float>("uv_index_avg", INVALID_FLOAT);
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
	} else if (sensorType == SensorType::SENSOR_SUITE &&
			   (dataStructureType == DataStructureType::WEATHERLINK_LIVE_ISS_ARCHIVE_RECORD ||
			    dataStructureType == DataStructureType::WEATHERLINK_CONSOLE_ISS_ARCHIVE_RECORD)) {
		_obs.time = date::floor<chrono::milliseconds>(chrono::system_clock::from_time_t(data.get<time_t>("ts")));
		// This data package must be ingested after the ISS data
		float hum = data.get<float>("hum_last", INVALID_FLOAT);
		if (!isInvalid(hum))
			_obs.humidity = static_cast<int>(hum);
		_obs.temperatureF = data.get<float>("temp_last", INVALID_FLOAT);
		if (!isInvalid(_obs.temperatureF))
			_obs.temperature = from_Farenheit_to_Celsius(_obs.temperatureF);
		_obs.temperatureMinF = data.get<float>("temp_lo", INVALID_FLOAT);
		if (!isInvalid(_obs.temperatureMinF))
			_obs.minTemperature = from_Farenheit_to_Celsius(_obs.temperatureMinF);
		_obs.temperatureMaxF = data.get<float>("temp_hi", INVALID_FLOAT);
		if (!isInvalid(_obs.temperatureMaxF))
			_obs.maxTemperature = from_Farenheit_to_Celsius(_obs.temperatureMaxF);
		_obs.windDir = data.get<int>("wind_dir_of_prevail", INVALID_INT);
		_obs.windSpeed = data.get<float>("wind_speed_avg", INVALID_FLOAT);
		_obs.windGustSpeed = data.get<float>("wind_speed_hi", INVALID_FLOAT);
		auto rainRate = extractRainRate(data);
		if (!isInvalid(rainRate))
			_obs.rainRate = rainRate;
		auto rainFall = extractRainFall(data);
		if (!isInvalid(rainFall))
			_obs.rainFall = rainFall;
		_obs.solarRad = data.get<int>("solar_rad_avg", INVALID_INT);
		_obs.uvIndex = data.get<float>("uv_index_avg", INVALID_FLOAT);

		_obs.supercapVoltage = data.get<float>("supercap_volt_last", INVALID_FLOAT);
		_obs.solarPanelVoltage = data.get<float>("solar_volt_last", INVALID_FLOAT);
		_obs.backupVoltage = data.get<float>("trans_battery", INVALID_FLOAT);
	} else if (sensorType == SensorType::BAROMETER &&
			   (dataStructureType == DataStructureType::WEATHERLINK_LIVE_NON_ISS_ARCHIVE_RECORD ||
			    dataStructureType == DataStructureType::WEATHERLINK_CONSOLE_BAROMETER_ARCHIVE_RECORD)) {
		_obs.time = date::floor<chrono::milliseconds>(chrono::system_clock::from_time_t(data.get<time_t>("ts")));
		_obs.pressure = data.get<float>("bar_sea_level", INVALID_FLOAT);
		if (!isInvalid(_obs.pressure))
			_obs.pressure = from_inHg_to_bar(_obs.pressure) * 1000;
	} else if (sensorType == SensorType::LEAF_SOIL_SUBSTATION &&
			   (dataStructureType == DataStructureType::WEATHERLINK_LIVE_NON_ISS_ARCHIVE_RECORD ||
			    dataStructureType == DataStructureType::WEATHERLINK_CONSOLE_LEAFSOIL_ARCHIVE_RECORD)) {
		_obs.time = date::floor<chrono::milliseconds>(chrono::system_clock::from_time_t(data.get<time_t>("ts")));
		// The first two temperatures are put in both leaf and soil temperatures fields
		// because we cannot know from the API where the user installed the sensors
		// It's necessary to enable/disable the corresponding sensors from the administration
		// page in the Meteodata website.

		// The temperature conversions are done in the message insertion methods
		_obs.leafTemperature[0] = data.get<float>("temp_last_1", INVALID_FLOAT);
		_obs.leafTemperature[1] = data.get<float>("temp_last_2", INVALID_FLOAT);
		_obs.soilTemperature[0] = data.get<float>("temp_last_1", INVALID_FLOAT);
		_obs.soilTemperature[1] = data.get<float>("temp_last_2", INVALID_FLOAT);
		_obs.soilTemperature[2] = data.get<float>("temp_last_3", INVALID_FLOAT);
		_obs.soilTemperature[3] = data.get<float>("temp_last_4", INVALID_FLOAT);
		// The APIv2 returns a float for leaf wetness and soil moisture but we store an int
		_obs.leafWetness[0] = std::lround(data.get<float>("wet_leaf_last_1", INVALID_FLOAT));
		_obs.leafWetness[1] = std::lround(data.get<float>("wet_leaf_last_2", INVALID_FLOAT));
		_obs.soilMoisture[0] = std::lround(data.get<float>("moist_soil_last_1", INVALID_FLOAT));
		_obs.soilMoisture[1] = std::lround(data.get<float>("moist_soil_last_2", INVALID_FLOAT));
		_obs.soilMoisture[2] = std::lround(data.get<float>("moist_soil_last_3", INVALID_FLOAT));
		_obs.soilMoisture[3] = std::lround(data.get<float>("moist_soil_last_4", INVALID_FLOAT));
	} else if (sensorType == SensorType::ANEMOMETER) {
		_obs.time = date::floor<chrono::milliseconds>(chrono::system_clock::from_time_t(data.get<time_t>("ts")));
		_obs.windDir = data.get<int>("wind_dir_prevail", INVALID_INT);
		_obs.windSpeed = data.get<float>("wind_speed_avg_last_10_min", INVALID_FLOAT);
		_obs.windGustSpeed = data.get<float>("wind_speed_hi", INVALID_FLOAT);
	}
}

}
