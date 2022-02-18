/**
 * @file vantagepro2archivemessage.cpp
 * @brief Implementation of the VantagePro2ArchiveMessage class
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

#include <date/date.h>
#include <date/tz.h>
#include <observation.h>

#include "vantagepro2_archive_message.h"
#include "vantagepro2_message.h"
#include "../time_offseter.h"

namespace chrono = std::chrono;

namespace meteodata
{

VantagePro2ArchiveMessage::VantagePro2ArchiveMessage(const ArchiveDataPoint& data, const TimeOffseter* timeOffseter) :
		_data(data),
		_timeOffseter(timeOffseter)
{}

Observation VantagePro2ArchiveMessage::getObservation(CassUuid station) const
{
	Observation result;

	auto timestamp = _timeOffseter->convertFromLocalTime(_data.day, _data.month, _data.year + 2000, _data.time / 100,
														 _data.time % 100);

	result.station = station;
	result.day = date::floor<date::days>(timestamp);
	result.time = date::floor<chrono::seconds>(timestamp);
	result.barometer = {_data.barometer != 0, from_inHg_to_bar(_data.barometer)};
	result.dewpoint = {_data.outsideTemp != 32767 && _data.outsideHum != 255,
					   dew_point(from_Farenheit_to_Celsius(_data.outsideTemp / 10), _data.outsideHum)};
	for (int i = 0 ; i < 2 ; i++)
		result.extrahum[i] = {_data.extraHum[i] != 255, _data.extraHum[i]};
	for (int i = 0 ; i < 3 ; i++)
		result.extratemp[i] = {_data.extraTemp[i] != 255, from_Farenheit_to_Celsius(_data.extraTemp[i] - 90)};
	result.heatindex = {_data.outsideTemp != 32767 && _data.outsideHum != 255,
						heat_index(_data.outsideTemp / 10, _data.outsideHum)};
	for (int i = 0 ; i < 2 ; i++) {
		result.leaftemp[i] = {_data.leafTemp[i] != 255, from_Farenheit_to_Celsius(_data.leafTemp[i] - 90)};
		result.leafwetnesses[i] = {_data.leafWetness[i] >= 0 && _data.leafWetness[i] <= 15, _data.leafWetness[i]};
	}
	result.outsidehum = {_data.outsideHum != 255, _data.outsideHum};
	result.outsidetemp = {_data.outsideTemp != 32767, from_Farenheit_to_Celsius(_data.outsideTemp / 10.)};
	result.rainrate = {_data.maxRainRate != 65535, from_rainrate_to_mm(_data.maxRainRate)};
	result.rainfall = {true, from_rainrate_to_mm(_data.rainfall)};
	result.et = {true, from_in_to_mm(_data.et) / 1000};
	for (int i = 0 ; i < 4 ; i++) {
		result.soilmoistures[i] = {_data.soilMoisture[i] != 255, _data.soilMoisture[i]};
		result.soiltemp[i] = {_data.soilTemp[i] != 255, from_Farenheit_to_Celsius(_data.soilTemp[i] - 90)};
	}
	result.solarrad = {_data.solarRad != 32767, _data.solarRad};
	result.thswindex = {_data.outsideTemp != 32767 && _data.avgWindSpeed != 255 && _data.outsideHum != 255,
						thsw_index(from_Farenheit_to_Celsius(_data.outsideTemp / 10), _data.outsideHum,
								   from_mph_to_mps(_data.avgWindSpeed))};
	result.uv = {_data.uv != 255, _data.uv};
	result.windchill = {_data.outsideTemp != 32767 && _data.avgWindSpeed != 255,
						wind_chill(_data.outsideTemp / 10, _data.avgWindSpeed)};
	result.winddir = {_data.prevailingWindDir != 255, static_cast<int>(_data.prevailingWindDir * 22.5)};
	result.windgust = {_data.maxWindSpeed != 255, from_mph_to_kph(_data.maxWindSpeed)};
	result.windspeed = {_data.avgWindSpeed != 255, from_mph_to_kph(_data.avgWindSpeed)};
	if (_data.solarRad != 32767) {
		bool ins = insolated(_data.solarRad, _timeOffseter->getLatitude(), _timeOffseter->getLongitude(),
							 date::floor<chrono::seconds>(timestamp).time_since_epoch().count());
		result.insolation_time = {true, ins ? _timeOffseter->getMeasureStep() : 0};
	}
	result.min_outside_temperature = {_data.minOutsideTemp != 32767,
									  from_Farenheit_to_Celsius(_data.minOutsideTemp / 10.0)};
	result.max_outside_temperature = {_data.maxOutsideTemp != -32768,
									  from_Farenheit_to_Celsius(_data.maxOutsideTemp / 10.0)};

	return result;
}

}
