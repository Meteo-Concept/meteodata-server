/**
 * @file abstract_weatherlink_api_message.cpp
 * @brief Implementation of the AbstractWeatherlinkApiMessage class
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
#include <boost/property_tree/xml_parser.hpp>
#include <cassandra.h>
#include <message.h>

#include "vantagepro2_message.h"
#include "abstract_weatherlink_api_message.h"
#include "../time_offseter.h"

namespace meteodata
{

namespace chrono = std::chrono;
namespace pt = boost::property_tree;

AbstractWeatherlinkApiMessage::AbstractWeatherlinkApiMessage(const TimeOffseter* timeOffseter) :
		_timeOffseter{timeOffseter}
{}

Observation AbstractWeatherlinkApiMessage::getObservation(CassUuid station) const
{
	Observation result;

	result.station = station;
	result.day = date::floor<date::days>(_obs.time);
	result.time = date::floor<chrono::seconds>(_obs.time);
	result.barometer = {!isInvalid(_obs.pressure), _obs.pressure};
	result.dewpoint = {!isInvalid(_obs.temperature) && !isInvalid(_obs.humidity),
					   dew_point(_obs.temperature, _obs.humidity)};
	for (int i = 0 ; i < 2 ; i++)
		result.extrahum[i] = {!isInvalid(_obs.extraHumidity[i]), _obs.extraHumidity[i]};
	for (int i = 0 ; i < 3 ; i++) {
		result.extratemp[i] = {!isInvalid(_obs.extraTemperature[i]),
							   from_Farenheit_to_Celsius(_obs.extraTemperature[i])};
	}
	result.heatindex = {!isInvalid(_obs.temperatureF) && !isInvalid(_obs.humidity),
						heat_index(_obs.temperatureF, _obs.humidity)};
	for (int i = 0 ; i < 2 ; i++) {
		result.leaftemp[i] = {!isInvalid(_obs.leafTemperature[i]), from_Farenheit_to_Celsius(_obs.leafTemperature[i])};
		result.leafwetnesses[i] = {!isInvalid(_obs.leafWetness[i]), _obs.leafWetness[i]};
	}
	result.outsidehum = {!isInvalid(_obs.humidity), _obs.humidity};
	result.outsidetemp = {!isInvalid(_obs.temperature), _obs.temperature};
	result.rainrate = {!isInvalid(_obs.rainRate), _obs.rainRate};
	result.rainfall = {!isInvalid(_obs.rainFall), _obs.rainFall};

	if (!isInvalid(_obs.temperature) && !isInvalid(_obs.windSpeed) && !isInvalid(_obs.humidity) &&
		!isInvalid(_obs.solarRad)) {
		result.et = {true,
					 evapotranspiration(_obs.temperature, _obs.humidity, from_mph_to_mps(_obs.windSpeed), _obs.solarRad,
										_timeOffseter->getLatitude(), _timeOffseter->getLongitude(),
										_timeOffseter->getElevation(), chrono::system_clock::to_time_t(_obs.time),
										_timeOffseter->getMeasureStep())};
	}

	for (int i = 0 ; i < 4 ; i++) {
		result.soilmoistures[i] = {!isInvalid(_obs.soilMoisture[i]), _obs.soilMoisture[i]};
		result.soiltemp[i] = {!isInvalid(_obs.soilTemperature[i]), from_Farenheit_to_Celsius(_obs.soilTemperature[i])};
	}

	result.solarrad = {!isInvalid(_obs.solarRad), _obs.solarRad};

	if (!isInvalid(_obs.temperatureF) && !isInvalid(_obs.windSpeed) && !isInvalid(_obs.humidity) &&
		!isInvalid(_obs.solarRad)) {
		result.thswindex = {true, thsw_index(_obs.temperatureF, _obs.humidity, _obs.windSpeed, _obs.solarRad)};
	} else if (!isInvalid(_obs.temperatureF) && !isInvalid(_obs.windSpeed) && !isInvalid(_obs.humidity) &&
			   isInvalid(_obs.solarRad)) {
		result.thswindex = {true, thsw_index(_obs.temperatureF, _obs.humidity, _obs.windSpeed)};
	}
	result.uv = {!isInvalid(_obs.uvIndex), _obs.uvIndex * 10};
	result.windchill = {!isInvalid(_obs.temperatureF) && !isInvalid(_obs.windSpeed),
						wind_chill(_obs.temperatureF, _obs.windSpeed)};
	result.winddir = {!isInvalid(_obs.windDir), _obs.windDir};
	result.windgust = {!isInvalid(_obs.windGustSpeed), from_mph_to_kph(_obs.windGustSpeed)};
	result.windspeed = {!isInvalid(_obs.windSpeed), from_mph_to_kph(_obs.windSpeed)};

	return result;
}
}
