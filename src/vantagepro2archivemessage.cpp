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

#include "vantagepro2archivemessage.h"
#include "vantagepro2message.h"
#include "timeoffseter.h"

namespace meteodata
{

VantagePro2ArchiveMessage::VantagePro2ArchiveMessage(const ArchiveDataPoint& data, const TimeOffseter* timeOffseter) :
	Message(),
	_data(data),
	_timeOffseter(timeOffseter)
{}

void VantagePro2ArchiveMessage::populateDataPoint(const CassUuid station, CassStatement* const statement) const
{
	std::cerr << "Populating the new datapoint (archived value)" << std::endl;
	/*************************************************************/
	cass_statement_bind_uuid(statement, 0, station);
	/*************************************************************/
	cass_statement_bind_int64(statement, 1,
		date::floor<chrono::milliseconds>(_timeOffseter->convertFromLocalTime(
				_data.day,
				_data.month,
				_data.year + 2000,
				_data.time / 100,
				_data.time % 100
			)).time_since_epoch().count()
		);
	/*************************************************************/
	// No bar trend
	/*************************************************************/
	std::cerr << "barometer: " << from_inHg_to_bar(_data.barometer) << std::endl;
	cass_statement_bind_float(statement, 3, from_inHg_to_bar(_data.barometer));
	/*************************************************************/
	// No absolute barometric pressure
	/*************************************************************/
	// No raw barometric sensor reading
	/*************************************************************/
	// Do not store inside temperature
	/*************************************************************/
	if (_data.outsideTemp != 32767)
		cass_statement_bind_float(statement, 7, from_Farenheight_to_Celsius(_data.outsideTemp/10.0));
	/*************************************************************/
	// Do not store inside humidity
	/*************************************************************/
	if (_data.outsideHum != 255)
		cass_statement_bind_int32(statement, 9, _data.outsideHum);
	/*************************************************************/
	for (int i=0 ; i<3 ; i++) {
		if (_data.extraTemp[1] != 255)
			cass_statement_bind_float(statement, 10+i, from_Farenheight_to_Celsius(_data.extraTemp[i] - 90));
	}
	/*************************************************************/
	for (int i=0 ; i<2 ; i++) {
		if (_data.soilTemp[i] != 255)
			cass_statement_bind_float(statement, 17+i, from_Farenheight_to_Celsius(_data.soilTemp[i] - 90));
		if (_data.leafTemp[i] != 255)
			cass_statement_bind_float(statement, 21+i, from_Farenheight_to_Celsius(_data.leafTemp[i] - 90));
		if (_data.extraHum[i] != 255)
			cass_statement_bind_int32(statement, 25+i, _data.extraHum[i]);
		if (_data.leafWetness[i] >= 0 && _data.leafWetness[i] <= 15)
			cass_statement_bind_int32(statement, 36+i, _data.leafWetness[i]);
	}
	/*************************************************************/
	for (int i=0 ; i<4 ; i++) {
		if (_data.soilMoisture[i] != 255)
			cass_statement_bind_int32(statement, 32+i, _data.soilMoisture[i]);
	}
	/*************************************************************/
	if (_data.avgWindSpeed != 255)
		cass_statement_bind_float(statement, 40, from_mph_to_kph(_data.avgWindSpeed));
	/*************************************************************/
	if (_data.prevailingWindDir != 255)
		cass_statement_bind_int32(statement, 41, static_cast<int>(_data.prevailingWindDir * 22.5));
	/*************************************************************/
	// No 10-min or 2-min average wind speed
	/*************************************************************/
	if (_data.maxWindSpeed != 255)
		cass_statement_bind_float(statement, 44, from_mph_to_kph(_data.maxWindSpeed));
	/*************************************************************/
	if (_data.maxWindSpeedDir != 255)
		cass_statement_bind_int32(statement, 45, static_cast<int>(_data.maxWindSpeedDir * 22.5));
	/*************************************************************/
	if (_data.maxRainRate != 65535)
		cass_statement_bind_float(statement, 46, from_rainrate_to_mm(_data.maxRainRate));
	/*************************************************************/
	// No avg rain rate over hour/day/...
	/*************************************************************/
	// No storm measurement
	/*************************************************************/
	if (_data.uv != 255)
		cass_statement_bind_int32(statement, 55, _data.uv);
	/*************************************************************/
	if (_data.solarRad != 32767)
		cass_statement_bind_int32(statement, 56, _data.solarRad);
	/*************************************************************/
	if (_data.outsideTemp != 32767 && _data.outsideHum != 255)
		cass_statement_bind_float(statement, 57,
			dew_point(
				from_Farenheight_to_Celsius(_data.outsideTemp / 10),
				_data.outsideHum
			)
		);
	/*************************************************************/
	if (_data.outsideTemp != 32767 && _data.outsideHum != 255)
		cass_statement_bind_float(statement, 58,
			heat_index(
				_data.outsideTemp / 10,
				_data.outsideHum
			)
		);
	/*************************************************************/
	if (_data.outsideTemp != 32767 && _data.avgWindSpeed != 255)
		cass_statement_bind_float(statement, 59,
			wind_chill(
				_data.outsideTemp / 10,
				_data.avgWindSpeed
			)
		);
	/*************************************************************/
	if (_data.outsideTemp != 32767 && _data.avgWindSpeed != 255 && _data.outsideHum != 255)
		cass_statement_bind_float(statement, 60,
			thsw_index(
				from_Farenheight_to_Celsius(_data.outsideTemp / 10),
				_data.outsideHum,
				from_mph_to_mps(_data.avgWindSpeed)
			)
		);
	/*************************************************************/
	// ET is not exploitable, it's given over the last hour
	/*************************************************************/
	std::string val = VantagePro2Message::from_forecast_to_diagnostic(_data.forecast);
	if (!val.empty())
		cass_statement_bind_string(statement, 64, val.c_str());
	/*************************************************************/
	// No forecast icons
	/*************************************************************/
	// No sunrise time
	/*************************************************************/
	// No sunset time
	/*************************************************************/
	cass_statement_bind_float(statement, 68, _data.rainfall);
	cass_statement_bind_float(statement, 69, _data.et);
}

void VantagePro2ArchiveMessage::populateV2DataPoint(const CassUuid station, CassStatement* const statement) const
{
	auto timestamp = _timeOffseter->convertFromLocalTime(
				_data.day,
				_data.month,
				_data.year + 2000,
				_data.time / 100,
				_data.time % 100
			);

	/*************************************************************/
	cass_statement_bind_uuid(statement, 0, station);
	/*************************************************************/
	cass_statement_bind_uint32(statement, 1,
		cass_date_from_epoch(
			date::floor<chrono::seconds>(
				timestamp
			).time_since_epoch().count()
		)
	);
	/*************************************************************/
	cass_statement_bind_int64(statement, 2,
		date::floor<chrono::milliseconds>(
			timestamp
		).time_since_epoch().count()
	);
	/*************************************************************/
	std::cerr << "barometer: " << from_inHg_to_bar(_data.barometer) << std::endl;
	cass_statement_bind_float(statement, 3, from_inHg_to_bar(_data.barometer));
	/*************************************************************/
	if (_data.outsideTemp != 32767 && _data.outsideHum != 255)
		cass_statement_bind_float(statement, 4,
			dew_point(
				from_Farenheight_to_Celsius(_data.outsideTemp / 10),
				_data.outsideHum
			)
		);
	/*************************************************************/
	for (int i=0 ; i<2 ; i++)
		if (_data.extraHum[i] != 255)
			cass_statement_bind_int32(statement, 5+i, _data.extraHum[i]);
	/*************************************************************/
	for (int i=0 ; i<3 ; i++) {
		if (_data.extraTemp[1] != 255)
			cass_statement_bind_float(statement, 7+i, from_Farenheight_to_Celsius(_data.extraTemp[i] - 90));
	}
	/*************************************************************/
	if (_data.outsideTemp != 32767 && _data.outsideHum != 255)
		cass_statement_bind_float(statement, 10,
			heat_index(
				_data.outsideTemp / 10,
				_data.outsideHum
			)
		);
	/*************************************************************/
	// Do not store inside humidity
	/*************************************************************/
	// Do not store inside temperature
	/*************************************************************/
	for (int i=0 ; i<2 ; i++) {
		if (_data.leafTemp[i] != 255)
			cass_statement_bind_float(statement, 13+i, from_Farenheight_to_Celsius(_data.leafTemp[i] - 90));
		if (_data.leafWetness[i] >= 0 && _data.leafWetness[i] <= 15)
			cass_statement_bind_int32(statement, 15+i, _data.leafWetness[i]);
	}
	/*************************************************************/
	if (_data.outsideHum != 255)
		cass_statement_bind_int32(statement, 17, _data.outsideHum);
	/*************************************************************/
	if (_data.outsideTemp != 32767)
		cass_statement_bind_float(statement, 18, from_Farenheight_to_Celsius(_data.outsideTemp/10.0));
	/*************************************************************/
	if (_data.maxRainRate != 65535)
		cass_statement_bind_float(statement, 19, from_rainrate_to_mm(_data.maxRainRate));
	/*************************************************************/
	cass_statement_bind_float(statement, 20, from_rainrate_to_mm(_data.rainfall));
	/*************************************************************/
	cass_statement_bind_float(statement, 21, from_in_to_mm(_data.et) / 1000);
	/*************************************************************/
	for (int i=0 ; i<4 ; i++) {
		if (_data.soilMoisture[i] != 255)
			cass_statement_bind_int32(statement, 22+i, _data.soilMoisture[i]);
	}
	/*************************************************************/
	for (int i=0 ; i<4 ; i++) {
		if (_data.soilTemp[i] != 255)
			cass_statement_bind_float(statement, 26+i, from_Farenheight_to_Celsius(_data.soilTemp[i] - 90));
	}
	/*************************************************************/
	if (_data.solarRad != 32767)
		cass_statement_bind_int32(statement, 30, _data.solarRad);
	/*************************************************************/
	if (_data.outsideTemp != 32767 && _data.avgWindSpeed != 255 && _data.outsideHum != 255)
		cass_statement_bind_float(statement, 31,
			thsw_index(
				from_Farenheight_to_Celsius(_data.outsideTemp / 10),
				_data.outsideHum,
				from_mph_to_mps(_data.avgWindSpeed)
			)
		);
	/*************************************************************/
	if (_data.uv != 255)
		cass_statement_bind_int32(statement, 32, _data.uv);
	/*************************************************************/
	if (_data.outsideTemp != 32767 && _data.avgWindSpeed != 255)
		cass_statement_bind_float(statement, 33,
			wind_chill(
				_data.outsideTemp / 10,
				_data.avgWindSpeed
			)
		);
	/*************************************************************/
	if (_data.prevailingWindDir != 255)
		cass_statement_bind_int32(statement, 34, static_cast<int>(_data.prevailingWindDir * 22.5));
	/*************************************************************/
	if (_data.maxWindSpeed != 255)
		cass_statement_bind_float(statement, 35, from_mph_to_kph(_data.maxWindSpeed));
	/*************************************************************/
	if (_data.avgWindSpeed != 255)
		cass_statement_bind_float(statement, 36, from_mph_to_kph(_data.avgWindSpeed));
	/*************************************************************/
	if (_data.solarRad != 32767) {
		bool ins = insolated(
			_data.solarRad,
			_timeOffseter->getLatitude(),
			_timeOffseter->getLongitude(),
			date::floor<chrono::seconds>(timestamp).time_since_epoch().count()
		);
		cass_statement_bind_int32(statement, 37, ins ? _timeOffseter->getMeasureStep() : 0);
	}
	/*************************************************************/
}

}
