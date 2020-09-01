/**
 * @file fieldclimate_api_archive_message.cpp
 * @brief Implementation of the FieldClimateApiArchiveMessage class
 * @author Laurent Georget
 * @date 2020-09-03
 */
/*
 * Copyright (C) 2020  SAS JD Environnement <contact@meteo-concept.fr>
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

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <cassandra.h>
#include <message.h>

#include "fieldclimate_archive_message.h"
#include "../davis/vantagepro2_message.h"

namespace {
	template<class Iterable, class Distance>
	inline auto beginAt(Iterable& c, Distance n) -> decltype(std::begin(c)) {
		auto it = std::begin(c);
		std::advance(it, n);
		return it;
	}
}

namespace meteodata {

namespace chrono = std::chrono;
namespace pt = boost::property_tree;

FieldClimateApiArchiveMessage::FieldClimateApiArchiveMessage(
		const TimeOffseter* timeOffseter,
		const std::map<std::string, std::string>* sensors
	) : _timeOffseter(timeOffseter),
	    _sensors(sensors)
{
}

void FieldClimateApiArchiveMessage::ingest(const pt::ptree& jsonTree, int index)
{
	// Every individual message will receive the full tree, but a specific
	// index (one index = one date = one datapoint)
	const pt::ptree& dates = jsonTree.get_child("dates");
	const pt::ptree& data = jsonTree.get_child("data");
	auto itDate = ::beginAt(dates, index);

	using namespace date;
	std::istringstream rawDate{itDate->second.get_value<std::string>()};
	local_seconds stationArchiveDate;
	rawDate >> parse("%Y-%m-%d %H:%M:%S", stationArchiveDate);
	_obs.time = _timeOffseter->convertFromLocalTime(stationArchiveDate);

	std::map<std::string, const pt::ptree*> variables;
	for (auto entryIt = data.begin() ; entryIt != data.end() ; ++entryIt) {
		auto code = entryIt->second.get_optional<std::string>("code");
		if (code) {
			variables.emplace(*code, &(entryIt->second));
		}
	}

	// An iterator for all lookups in the sensors map
	auto sensorIt = _sensors->end();

	auto getValueForSensor = [&](const std::string& variable, const std::string& aggregation, auto& result) {
		sensorIt = _sensors->find(variable);
		if (sensorIt != _sensors->end()) {
			const std::string& sensorId = sensorIt->second;
			auto entryIt = variables.find(sensorId);
			if (entryIt != variables.end()) {
				auto sensorOutput = entryIt->second->get_child_optional("values." + aggregation);
				if (sensorOutput) {
					auto itEntry = ::beginAt(*sensorOutput, index);
					result = itEntry->second.get_value<>(invalidDefault(result));
				}
			}
		}
		return result;
	};

	// atmospheric pressure
	getValueForSensor("pressure", "avg", _obs.pressure);

	// relative humidity
	getValueForSensor("humidity", "avg", _obs.humidity);

	// temperature
	getValueForSensor("temperature", "avg", _obs.temperature);

	// dominant wind direction
	getValueForSensor("wind direction", "avg", _obs.windDir);

	// average wind speed
	getValueForSensor("wind speed", "avg", _obs.windSpeed);

	// max wind gust speed
	getValueForSensor("wind gust speed", "max", _obs.windGustSpeed);

	// max rainrate
	getValueForSensor("rain rate", "max", _obs.rainRate);

	// total rainfall
	getValueForSensor("rainfall", "sum", _obs.rainFall);

	// solar radiation
	getValueForSensor("solar radiation", "avg", _obs.solarRad);

	// UV index
	getValueForSensor("uv index", "avg", _obs.uvIndex);

	// extra temperatures
	for (int i=0 ; i<3 ; i++)
		getValueForSensor("extra temperature " + std::to_string(i+1), "avg", _obs.extraTemperature[i]);

	// extra humidities
	for (int i=0 ; i<2 ; i++)
		getValueForSensor("extra humidity " + std::to_string(i+1), "avg", _obs.extraHumidity[i]);

	// leaf temperature
	for (int i=0 ; i<2 ; i++)
		getValueForSensor("leaf temperature " + std::to_string(i+1), "avg", _obs.leafTemperature[i]);

	// leaf wetness
	for (int i=0 ; i<2 ; i++)
		getValueForSensor("leaf wetness " + std::to_string(i+1), "avg", _obs.leafWetness[i]);

	// soil moisture
	for (int i=0 ; i<4 ; i++)
		getValueForSensor("soil moisture " + std::to_string(i+1), "avg", _obs.soilMoisture[i]);

	// soil temperature
	for (int i=0 ; i<4 ; i++)
		getValueForSensor("soil temperature " + std::to_string(i+1), "avg", _obs.soilTemperature[i]);
}

void FieldClimateApiArchiveMessage::populateDataPoint(const CassUuid, CassStatement* const) const
{
	// not implemented
}

void FieldClimateApiArchiveMessage::populateV2DataPoint(const CassUuid station, CassStatement* const statement) const
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
	for (int i=0 ; i<2 ; i++)
		if (!isInvalid(_obs.extraHumidity[i]))
			cass_statement_bind_int32(statement, 5+i, _obs.extraHumidity[i]);
	/*************************************************************/
	for (int i=0 ; i<3 ; i++)
		if (!isInvalid(_obs.extraTemperature[i]))
			cass_statement_bind_float(statement, 7+i, _obs.extraTemperature[i]);
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
	for (int i=0 ; i<2 ; i++) {
		if (!isInvalid(_obs.leafTemperature[i]))
			cass_statement_bind_float(statement, 13+i, _obs.leafTemperature[i]);
		if (!isInvalid(_obs.leafWetness[i]))
			cass_statement_bind_int32(statement, 15+i,_obs.leafWetness[i]);
	}
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
	if (!isInvalid(_obs.temperature) && !isInvalid(_obs.windSpeed)
	 && !isInvalid(_obs.humidity) && !isInvalid(_obs.solarRad))
		cass_statement_bind_float(statement, 21,
			evapotranspiration(
				_obs.temperature,
				_obs.humidity,
				from_kph_to_mps(_obs.windSpeed),
				_obs.solarRad,
				_timeOffseter->getLatitude(),
				_timeOffseter->getLongitude(),
				_timeOffseter->getElevation(),
				chrono::system_clock::to_time_t(_obs.time),
				_timeOffseter->getMeasureStep()
			)
		);
	/*************************************************************/
	for (int i=0 ; i<4 ; i++) {
		if (!isInvalid(_obs.soilMoisture[i]))
			cass_statement_bind_int32(statement, 22+i, _obs.soilMoisture[i]);
		if (!isInvalid(_obs.soilTemperature[i]))
			cass_statement_bind_float(statement, 26+i, _obs.soilTemperature[i]);
	}
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
}

}
