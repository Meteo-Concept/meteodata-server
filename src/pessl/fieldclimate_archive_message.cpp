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
#include <string>
#include <functional>

#include <boost/property_tree/ptree.hpp>
#include <cassandra.h>
#include <observation.h>

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
	getValueForSensor("temperature", "min", _obs.minTemperature);
	getValueForSensor("temperature", "max", _obs.maxTemperature);

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

	// leaf wetness given in minutes
	getValueForSensor("leaf wetness time ratio 1", "time", _obs.leafWetnessTimeRatio[0]);
}

Observation FieldClimateApiArchiveMessage::getObservation(const CassUuid station) const
{
    Observation result;

    result.station = station;
    result.day = date::floor<date::days>(_obs.time);
    result.time = _obs.time;
    result.barometer = { !isInvalid(_obs.pressure), _obs.pressure };
    result.dewpoint = {
            !isInvalid(_obs.temperature) && !isInvalid(_obs.humidity),
            dew_point(_obs.temperature, _obs.humidity)
    };
    for (int i=0 ; i<2 ; i++)
        result.extrahum[i] = { !isInvalid(_obs.extraHumidity[i]), _obs.extraHumidity[i] };
    for (int i=0 ; i<3 ; i++)
        result.extratemp[i] = { !isInvalid(_obs.extraTemperature[i]), _obs.extraTemperature[i] };
    result.heatindex = {
            !isInvalid(_obs.temperature) && !isInvalid(_obs.humidity),
            heat_index(from_Celsius_to_Farenheit(_obs.temperature), _obs.humidity)
    };
    for (int i=0 ; i<2 ; i++) {
        result.leaftemp[i] = { !isInvalid(_obs.leafTemperature[i]), _obs.leafTemperature[i] };
        result.leafwetnesses[i] = { !isInvalid(_obs.leafWetness[i]), _obs.leafWetness[i] };
    }
    result.outsidehum = { !isInvalid(_obs.humidity), _obs.humidity };
    result.outsidetemp = { !isInvalid(_obs.temperature), _obs.temperature };
    result.rainrate = { !isInvalid(_obs.rainRate), _obs.rainRate };
    result.rainfall = { !isInvalid(_obs.rainFall), _obs.rainFall };
    for (int i=0 ; i<4 ; i++) {
        result.soilmoistures[i] = { !isInvalid(_obs.soilMoisture[i]), _obs.soilMoisture[i] };
        result.soiltemp[i] = { !isInvalid(_obs.soilTemperature[i]), _obs.soilTemperature[i] };
    }

    if (!isInvalid(_obs.temperature) && !isInvalid(_obs.windSpeed)
	 && !isInvalid(_obs.humidity) && !isInvalid(_obs.solarRad)) {
        result.et = {
                true,
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
        };
        result.thswindex = {
                true,
                thsw_index(
                        _obs.temperature,
                        _obs.humidity,
                        from_kph_to_mps(_obs.windSpeed),
                        _obs.solarRad
                )
        };
    } else if (!isInvalid(_obs.temperature) && !isInvalid(_obs.windSpeed) && !isInvalid(_obs.humidity)) {
        result.thswindex = {
                true,
                thsw_index(
                        _obs.temperature,
                        _obs.humidity,
                        from_kph_to_mps(_obs.windSpeed)
                )
        };
    }
    result.solarrad = { !isInvalid(_obs.solarRad), _obs.solarRad };
    result.uv = { !isInvalid(_obs.uvIndex), _obs.uvIndex * 10 };
    result.windchill = {
            !isInvalid(_obs.temperature) && !isInvalid(_obs.windSpeed),
            wind_chill(_obs.temperature, _obs.windSpeed)
    };
    result.winddir = { !isInvalid(_obs.windDir), _obs.windDir };
    result.windgust = { !isInvalid(_obs.windGustSpeed), _obs.windGustSpeed };
    result.windspeed = { !isInvalid(_obs.windSpeed), _obs.windSpeed };
    if (!isInvalid(_obs.solarRad)) {
        bool ins = insolated(
                _obs.solarRad,
                _timeOffseter->getLatitude(),
                _timeOffseter->getLongitude(),
                _obs.time.time_since_epoch().count()
        );
        result.insolation_time = { true, ins ? _timeOffseter->getMeasureStep() : 0 };
    }
    result.min_outside_temperature = { !isInvalid(_obs.minTemperature), _obs.minTemperature };
    result.max_outside_temperature = { !isInvalid(_obs.maxTemperature), _obs.maxTemperature };

    result.leafwetness_timeratio1 = { !isInvalid(_obs.leafWetnessTimeRatio[0]), _obs.leafWetnessTimeRatio[0] };
    return result;
}

}
