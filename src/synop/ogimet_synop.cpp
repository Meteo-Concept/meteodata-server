/**
 * @file ogimetsynop.cpp
 * @brief Implementation of the OgimetSynop class
 * @author Laurent Georget
 * @date 2017-10-11
 */
/*
 * Copyright (C) 2017 SAS Météo Concept <contact@meteo-concept.fr>
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
#include <chrono>
#include <algorithm>

#include <date/date.h>
#include <date/tz.h>
#include <message.h>

#include "ogimet_synop.h"
#include "synop_decoder/synop_message.h"
#include "../davis/vantagepro2_message.h"

namespace {
// temperature and dew point both in tenths of Celsius degrees
int computeHumidity(int temperature, int dewPoint)
{
	double rh = std::expm1(((17.27 * dewPoint) / (2377 + dewPoint)) - ((17.27 * temperature) / (2377 + temperature)));
	return int((rh + 1.) * 100);
}
}

namespace meteodata
{

OgimetSynop::OgimetSynop(const SynopMessage& data) :
	Message(),
	_data(data),
	_humidity(
		_data._relativeHumidity ? *_data._relativeHumidity :
		_data._dewPoint && _data._meanTemperature ? computeHumidity(*_data._meanTemperature, *_data._dewPoint) :
		std::experimental::optional<int>()
	),
	_wind_mps(
		_data._meanWindSpeed ?
			(_data._windSpeedUnit == SynopMessage::WindSpeedUnit::KNOTS ?
				*_data._meanWindSpeed * 0.51444 :
				*_data._meanWindSpeed) :
			std::experimental::optional<float>()
		 )

{
	auto it = std::find_if(_data._precipitation.begin(), _data._precipitation.end(),
			[](const auto& pr) { return pr._duration == 1; });
	if (it != _data._precipitation.end())
		_rainfall = it->_amount;

	auto it2 = std::find_if(_data._gustObservations.begin(), _data._gustObservations.end(),
			[](const auto& go) { return go._duration == 60; });
	if (it2 != _data._gustObservations.end())
		_gust = _data._windSpeedUnit == SynopMessage::WindSpeedUnit::KNOTS ?
				it2->_speed * 1.85200 :
				it2->_speed * 3.6;
}



void OgimetSynop::populateDataPoint(const CassUuid station, CassStatement* const statement) const
{
	std::cerr << "Populating the new datapoint (archived value)" << std::endl;
	/*************************************************************/
	cass_statement_bind_uuid(statement, 0, station);
	/*************************************************************/
	cass_statement_bind_int64(statement, 1,
		date::floor<chrono::milliseconds>(_data._observationTime).time_since_epoch().count()
		);
	/*************************************************************/
	if (_data._pressureTendency)
		cass_statement_bind_float(statement, 3, (*_data._pressureTendency)._amount / 10.f);
	/*************************************************************/
	if (_data._pressureAtSeaLevel)
		cass_statement_bind_float(statement, 3, *_data._pressureAtSeaLevel / 10.);
	/*************************************************************/
	// No absolute barometric pressure
	/*************************************************************/
	if (_data._pressureAtStation)
		cass_statement_bind_float(statement, 3, *_data._pressureAtStation / 10.);
	/*************************************************************/
	// No inside temperature
	/*************************************************************/
	if (_data._meanTemperature)
		cass_statement_bind_float(statement, 7, *_data._meanTemperature / 10.f);
	/*************************************************************/
	// No inside temperature
	/*************************************************************/
	if (_humidity)
		cass_statement_bind_int32(statement, 9, *_humidity);
	/*************************************************************/
	// No extra temperature
	/*************************************************************/
	// No soil of leaf measurement
	// Actually, we can have a soil temperature but only the minimal value over the last night
	/*************************************************************/
	// No soil moisture
	/*************************************************************/
	if (_wind_mps)
		cass_statement_bind_float(statement, 40, *_wind_mps * 3.6);
	/*************************************************************/
	if (_data._meanWindDirection)
		cass_statement_bind_int32(statement, 41, *_data._meanWindDirection);
	/*************************************************************/
	// No 10-min or 2-min average wind speed
	/*************************************************************/
	if (_gust)
		cass_statement_bind_float(statement, 44, *_gust);
	/*************************************************************/
	// No wind gust dir
	/*************************************************************/
	// No precipitation intensity
	/*************************************************************/
	if (_rainfall)
		cass_statement_bind_int32(statement, 48, *_rainfall);
	/*************************************************************/
	// No storm measurement
	/*************************************************************/
	// No UV
	/*************************************************************/
	if (_data._globalSolarRadiationLastHour)
		cass_statement_bind_int32(statement, 56, *_data._globalSolarRadiationLastHour / 3.6);
	/*************************************************************/
	if (_data._dewPoint)
		cass_statement_bind_float(statement, 57, *_data._dewPoint / 10.);
	else if (_data._meanTemperature && _humidity)
		cass_statement_bind_float(statement, 57,
			dew_point(
				*_data._meanTemperature / 10.,
				*_humidity
			)
		);
	/*************************************************************/
	if (_data._meanTemperature && _humidity)
		cass_statement_bind_float(statement, 58,
			heat_index(
				from_Celsius_to_Farenheit(*_data._meanTemperature / 10.),
				*_humidity
			)
		);
	/*************************************************************/
	if (_data._meanTemperature && _wind_mps)
		cass_statement_bind_float(statement, 59,
			wind_chill(
				from_Celsius_to_Farenheit(*_data._meanTemperature / 10.),
				*_wind_mps * 3.6
			)
		);
	/*************************************************************/
	if (_data._meanTemperature && _wind_mps
	&& _humidity && _data._globalSolarRadiationLastHour)
		cass_statement_bind_float(statement, 60,
			thsw_index(
				*_data._meanTemperature / 10.,
				*_humidity,
				*_wind_mps,
				*_data._netRadiationLastHour / 3.6
			)
		);
	else if (_data._meanTemperature && _wind_mps && _humidity)
		cass_statement_bind_float(statement, 60,
			thsw_index(
				*_data._meanTemperature / 10.,
				*_humidity,
				*_wind_mps
			)
		);
	/*************************************************************/
	if (_data._evapoMaybeTranspiRation)
		cass_statement_bind_float(statement, 61, (*_data._evapoMaybeTranspiRation)._amount);
	/*************************************************************/
	// No forecast
	/*************************************************************/
	// No forecast icons
	/*************************************************************/
	// No sunrise time
	/*************************************************************/
	// No sunset time
	/*************************************************************/
	// No VantagePro2 archive rainfall or ET
}

void OgimetSynop::populateV2DataPoint(const CassUuid station, CassStatement* const statement) const
{
	/*************************************************************/
	cass_statement_bind_uuid(statement, 0, station);
	/*************************************************************/
	cass_statement_bind_uint32(statement, 1,
		cass_date_from_epoch(
			date::floor<chrono::seconds>(
				_data._observationTime
			).time_since_epoch().count()
		)
	);
	/*************************************************************/
	cass_statement_bind_int64(statement, 2,
		date::floor<chrono::milliseconds>(
			_data._observationTime
		).time_since_epoch().count()
	);
	/*************************************************************/
	if (_data._pressureAtSeaLevel)
		cass_statement_bind_float(statement, 3, *_data._pressureAtSeaLevel / 10.);
	/*************************************************************/
	if (_data._dewPoint)
		cass_statement_bind_float(statement, 4, *_data._dewPoint / 10.);
	else if (_data._meanTemperature && _humidity)
		cass_statement_bind_float(statement, 4,
			dew_point(
				*_data._meanTemperature / 10.,
				*_humidity
			)
		);
	/*************************************************************/
	// No extra humidity
	/*************************************************************/
	// No extra temperature
	/*************************************************************/
	if (_data._meanTemperature && _humidity)
		cass_statement_bind_float(statement, 10,
			heat_index(
				from_Celsius_to_Farenheit(*_data._meanTemperature / 10.),
				*_humidity
			)
		);
	/*************************************************************/
	// No inside humidity
	/*************************************************************/
	// No inside temperature
	/*************************************************************/
	// No leaf measurements
	/*************************************************************/
	if (_humidity)
		cass_statement_bind_int32(statement, 17, *_humidity);
	/*************************************************************/
	if (_data._meanTemperature)
		cass_statement_bind_float(statement, 18, *_data._meanTemperature / 10.);
	/*************************************************************/
	// No max precipitation rate
	/*************************************************************/
	if (_rainfall)
		cass_statement_bind_float(statement, 20, *_rainfall);
	/*************************************************************/
	if (_data._evapoMaybeTranspiRation)
		cass_statement_bind_float(statement, 21, (*_data._evapoMaybeTranspiRation)._amount);
	/*************************************************************/
	// No soil moistures
	/*************************************************************/
	// No soil temperature
	/*************************************************************/
	if (_data._globalSolarRadiationLastHour)
		cass_statement_bind_int32(statement, 30, *_data._globalSolarRadiationLastHour / 3.6);
	/*************************************************************/
	if (_data._meanTemperature && _wind_mps
	&& _humidity && _data._globalSolarRadiationLastHour)
		cass_statement_bind_float(statement, 31,
			thsw_index(
				*_data._meanTemperature / 10.,
				*_humidity,
				*_wind_mps,
				*_data._netRadiationLastHour / 3.6
			)
		);
	else if (_data._meanTemperature && _wind_mps && _humidity)
		cass_statement_bind_float(statement, 31,
			thsw_index(
				*_data._meanTemperature / 10.,
				*_humidity,
				*_wind_mps
			)
		);
	/*************************************************************/
	// No UV index
	/*************************************************************/
	if (_data._meanTemperature && _wind_mps)
		cass_statement_bind_float(statement, 33,
			wind_chill(
				from_Celsius_to_Farenheit(*_data._meanTemperature / 10.),
				*_wind_mps * 3.6
			)
		);
	/*************************************************************/
	if (_data._meanWindDirection)
		cass_statement_bind_int32(statement, 34, *_data._meanWindDirection);
	/*************************************************************/
	if (_gust)
		cass_statement_bind_float(statement, 35, *_gust);
	/*************************************************************/
	if (_wind_mps)
		cass_statement_bind_float(statement, 36, *_wind_mps * 3.6);
	/*************************************************************/
	if (_data._minutesOfSunshineLastHour)
		cass_statement_bind_int32(statement, 37, *_data._minutesOfSunshineLastHour);
	/*************************************************************/
}

}
