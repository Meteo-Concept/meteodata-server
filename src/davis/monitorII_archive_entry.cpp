/**
 * @file monitorII_archive_entry.cpp
 * @brief Implementation of the MonitorIIArchiveEntry class
 * @author Laurent Georget
 * @date 2024-12-17
 */
/*
 * Copyright (C) 2024 SAS Météo Concept <contact@meteo-concept.fr>
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

#include <date/date.h>
#include <date/tz.h>
#include <cassobs/observation.h>

#include "davis/monitorII_archive_entry.h"
#include "davis/vantagepro2_message.h"

namespace chrono = std::chrono;

namespace meteodata
{

MonitorIIArchiveEntry::MonitorIIArchiveEntry(const MonitorIIArchiveEntry::DataPoint& data) :
	_datapoint(data)
{}

Observation MonitorIIArchiveEntry::getObservation(CassUuid station) const
{
	Observation result;

	auto timestamp = getTimestamp();

	result.station = station;
	result.day = date::floor<date::days>(timestamp);
	result.time = timestamp;
	result.barometer = {_datapoint.barometer != 0xFFFF, from_inHg_to_bar(_datapoint.barometer)};
	result.dewpoint = {_datapoint.avgOutsideTemperature != 0xFFFF && _datapoint.outsideHumidity != 0xFF,
		dew_point(from_Farenheit_to_Celsius(_datapoint.avgOutsideTemperature / 10), _datapoint.outsideHumidity)};
	result.heatindex = {_datapoint.avgOutsideTemperature != 0xFFFF && _datapoint.outsideHumidity != 0xFF,
		heat_index(_datapoint.avgOutsideTemperature / 10, _datapoint.outsideHumidity)};
	result.outsidehum = {_datapoint.outsideHumidity != 0xFF, _datapoint.outsideHumidity};
	result.outsidetemp = {_datapoint.avgOutsideTemperature != 0xFFFF, from_Farenheit_to_Celsius(_datapoint.avgOutsideTemperature / 10.)};
	result.rainfall = {true, from_rainrate_to_mm(_datapoint.rainfall)};
	result.windchill = {_datapoint.avgOutsideTemperature != 0xFFFF && _datapoint.avgWindSpeed != 0xFF,
		wind_chill(_datapoint.avgOutsideTemperature / 10, _datapoint.avgWindSpeed)};
	result.winddir = {_datapoint.dominantWindDir != 0xFF, static_cast<int>(_datapoint.dominantWindDir * 22.5)};
	result.windgust = {_datapoint.hiWindSpeed != 0xFF, from_mph_to_kph(_datapoint.hiWindSpeed)};
	result.windspeed = {_datapoint.avgWindSpeed != 0xFF, from_mph_to_kph(_datapoint.avgWindSpeed)};
	result.min_outside_temperature = {_datapoint.lowOutsideTemperature != 0xFFFF,
		from_Farenheit_to_Celsius(_datapoint.lowOutsideTemperature / 10.0)};
	result.max_outside_temperature = {_datapoint.hiOutsideTemperature != 0xFFFF,
		from_Farenheit_to_Celsius(_datapoint.hiOutsideTemperature / 10.0)};

	result.insidehum = {_datapoint.insideHumidity != 0xFF, _datapoint.insideHumidity};
	result.insidetemp = {_datapoint.avgInsideTemperature != 0xFFFF, from_Farenheit_to_Celsius(_datapoint.avgInsideTemperature / 10.0)};

	return result;
}

}
