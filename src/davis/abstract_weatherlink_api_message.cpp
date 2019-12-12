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

namespace meteodata {

namespace chrono = std::chrono;
namespace pt = boost::property_tree;

constexpr size_t AbstractWeatherlinkApiMessage::MAXSIZE;
constexpr int    AbstractWeatherlinkApiMessage::INVALID_INT;
constexpr float  AbstractWeatherlinkApiMessage::INVALID_FLOAT;

AbstractWeatherlinkApiMessage::AbstractWeatherlinkApiMessage(const TimeOffseter* timeOffseter) :
	Message(),
	_timeOffseter{timeOffseter}
{}

void AbstractWeatherlinkApiMessage::populateDataPoint(const CassUuid station, CassStatement* const statement) const
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
	if (!isInvalid(_obs.rainRate))
		cass_statement_bind_float(statement, 46, from_in_to_mm(_obs.rainRate));
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

void AbstractWeatherlinkApiMessage::populateV2DataPoint(const CassUuid station, CassStatement* const statement) const
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
	for (int i=0 ; i<2 ; i++)
		if (!isInvalid(_obs.extraHumidity[i]))
			cass_statement_bind_int32(statement, 5+i, _obs.extraHumidity[i]);
	/*************************************************************/
	for (int i=0 ; i<3 ; i++)
		if (!isInvalid(_obs.extraTemperature[i]))
			cass_statement_bind_float(statement, 7+i, _obs.extraTemperature[i]);
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
	for (int i=0 ; i<2 ; i++) {
		if (!isInvalid(_obs.leafTemperature[i]))
			cass_statement_bind_float(statement, 13+i, from_Farenheight_to_Celsius(_obs.leafTemperature[i]));
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
		cass_statement_bind_float(statement, 19, from_in_to_mm(_obs.rainRate));
	/*************************************************************/
	if (!isInvalid(_obs.rainFall))
		cass_statement_bind_float(statement, 20, from_rainrate_to_mm(_obs.rainFall));
	/*************************************************************/
	if (!isInvalid(_obs.temperature) && !isInvalid(_obs.windSpeed)
	 && !isInvalid(_obs.humidity) && !isInvalid(_obs.solarRad))
		cass_statement_bind_float(statement, 21,
			evapotranspiration(
				_obs.temperature,
				_obs.humidity,
				from_mph_to_mps(_obs.windSpeed),
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
			cass_statement_bind_float(statement, 26+i, from_Farenheight_to_Celsius(_obs.soilTemperature[i]));
	}
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
