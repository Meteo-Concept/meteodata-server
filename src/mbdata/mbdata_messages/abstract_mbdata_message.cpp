/**
 * @file abstractmbdatamessage.cpp
 * @brief Definition of the AbstractMBDataMessage class
 * @author Laurent Georget
 * @date 2019-02-21
 */
/*
 * Copyright (C) 2019  JD Environnement <contact@meteo-concept.fr>
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

#include <algorithm>
#include <iterator>
#include <regex>

#include "../../time_offseter.h"
#include "../../davis/vantagepro2_message.h"
#include "abstract_mbdata_message.h"

namespace meteodata
{

namespace chrono = std::chrono;

AbstractMBDataMessage::AbstractMBDataMessage(date::sys_seconds datetime, const std::string& content,
											 const TimeOffseter& timeOffseter) :
		_datetime(datetime),
		_content(content),
		_timeOffseter(timeOffseter)
{
}

Observation AbstractMBDataMessage::getObservation(const CassUuid station) const
{
	Observation result;

	result.station = station;
	result.day = date::floor<date::days>(_datetime);
	result.time = date::floor<chrono::seconds>(_datetime);
	result.barometer = {bool(_pressure), *_pressure};
	if (_dewPoint) {
		result.dewpoint = {true, *_dewPoint};
	} else if (_airTemp && _humidity) {
		result.dewpoint = {true, dew_point(*_airTemp, *_humidity)};
	}
	result.outsidehum = {bool(_humidity), *_humidity};
	result.outsidetemp = {bool(_airTemp), *_airTemp};
	result.rainrate = {bool(_rainRate), *_rainRate};
	result.rainfall = {bool(_computedRainfall), *_computedRainfall};
	result.winddir = {bool(_windDir), *_windDir};
	result.windgust = {bool(_gust), *_gust};
	result.windspeed = {bool(_wind), *_wind};
	result.solarrad = {bool(_solarRad), *_solarRad};
	if (_solarRad) {
		bool ins = insolated(*_solarRad, _timeOffseter.getLatitude(), _timeOffseter.getLongitude(),
							 date::floor<chrono::seconds>(_datetime).time_since_epoch().count());
		result.insolation_time = {true, ins ? _timeOffseter.getMeasureStep() : 0};
	}

	return result;
}

}
