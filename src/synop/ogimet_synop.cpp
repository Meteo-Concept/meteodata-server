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
#include <observation.h>

#include "ogimet_synop.h"
#include "synop_decoder/synop_message.h"
#include "../davis/vantagepro2_message.h"

namespace
{
// temperature and dew point both in tenths of Celsius degrees
int computeHumidity(int temperature, int dewPoint)
{
	double rh = std::expm1(((17.27 * dewPoint) / (2377 + dewPoint)) - ((17.27 * temperature) / (2377 + temperature)));
	return int((rh + 1.) * 100);
}
}

namespace meteodata
{
OgimetSynop::OgimetSynop(const SynopMessage& data, const TimeOffseter* timeOffseter) :
		_timeOffseter(timeOffseter),
		_data(data),
		_humidity(_data._relativeHumidity ? *_data._relativeHumidity : _data._dewPoint && _data._meanTemperature
																	   ? computeHumidity(*_data._meanTemperature,
																						 *_data._dewPoint)
																	   : std::optional<int>()),
		_wind_mps(_data._meanWindSpeed ? (_data._windSpeedUnit == SynopMessage::WindSpeedUnit::KNOTS ?
										  *_data._meanWindSpeed * 0.51444 : *_data._meanWindSpeed)
									   : std::optional<float>())
{
	auto it = std::find_if(_data._precipitation.begin(), _data._precipitation.end(),
						   [](const auto& pr) { return pr._duration == 1; });
	if (it != _data._precipitation.end())
		_rainfall = it->_amount;

	auto it2 = std::find_if(_data._gustObservations.begin(), _data._gustObservations.end(),
							[](const auto& go) { return go._duration == 60; });
	if (it2 != _data._gustObservations.end())
		_gust = _data._windSpeedUnit == SynopMessage::WindSpeedUnit::KNOTS ? it2->_speed * 1.85200 : it2->_speed * 3.6;
}

Observation OgimetSynop::getObservations(const CassUuid station) const
{
	Observation result;

	result.station = station;
	result.day = date::floor<date::days>(_data._observationTime);
	result.time = date::floor<chrono::seconds>(_data._observationTime);
	result.barometer = {bool(_data._pressureAtSeaLevel), *_data._pressureAtSeaLevel / 10.};
	if (_data._dewPoint) {
		result.dewpoint = {true, *_data._dewPoint / 10.};
	} else if (_data._meanTemperature && _humidity) {
		result.dewpoint = {true, dew_point(*_data._meanTemperature / 10., *_humidity)};
	}
	if (_data._meanTemperature && _humidity) {
		result.heatindex = {true, heat_index(from_Celsius_to_Farenheit(*_data._meanTemperature / 10.), *_humidity)};
	}
	result.outsidehum = {bool(_humidity), *_humidity};
	result.outsidetemp = {bool(_data._meanTemperature), *_data._meanTemperature / 10.};
	result.rainfall = {bool(_rainfall), *_rainfall};
	if (_data._evapoMaybeTranspiRation) {
		result.et = {true, (*_data._evapoMaybeTranspiRation)._amount};
	} else if (_data._meanTemperature && _wind_mps && _humidity && _data._globalSolarRadiationLastHour) {
		float etp = evapotranspiration(*_data._meanTemperature / 10., *_humidity, *_wind_mps,
			*_data._globalSolarRadiationLastHour, _timeOffseter->getLatitude(),
			_timeOffseter->getLongitude(), _timeOffseter->getElevation(),
			date::round<chrono::seconds>(_data._observationTime).time_since_epoch().count(),
			_timeOffseter->getMeasureStep());
		result.et = {true, etp};
	}
	result.solarrad = {bool(_data._globalSolarRadiationLastHour), *_data._globalSolarRadiationLastHour / 3.6};

	if (_data._meanTemperature && _wind_mps && _humidity && _data._globalSolarRadiationLastHour) {
		result.thswindex = {true, thsw_index(*_data._meanTemperature / 10., *_humidity, *_wind_mps,
			*_data._globalSolarRadiationLastHour / 3.6)};
	} else if (_data._meanTemperature && _wind_mps && _humidity) {
		result.thswindex = {true, thsw_index(*_data._meanTemperature / 10., *_humidity, *_wind_mps)};
	}

	if (_data._meanTemperature && _wind_mps) {
		result.windchill = {true, wind_chill(from_Celsius_to_Farenheit(*_data._meanTemperature / 10.), *_wind_mps * 3.6)};
	}

	result.winddir = {bool(_data._meanWindDirection), *_data._meanWindDirection};
	result.windgust = {bool(_gust), *_gust};
	result.windspeed = {bool(_wind_mps), *_wind_mps * 3.6};
	result.insolation_time = {bool(_data._minutesOfSunshineLastHour), *_data._minutesOfSunshineLastHour};

	return result;
}

}
