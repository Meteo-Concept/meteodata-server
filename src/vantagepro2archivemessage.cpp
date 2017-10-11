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

#include "vantagepro2archivemessage.h"
#include "vantagepro2message.h"

namespace meteodata
{

VantagePro2ArchiveMessage::VantagePro2ArchiveMessage(ArchiveDataPoint data) :
	Message(),
	_data(data)
{}

void VantagePro2ArchiveMessage::populateDataPoint(const CassUuid station, CassStatement* const statement) const
{
	std::cerr << "Populating the new datapoint" << std::endl;
	/*************************************************************/
	cass_statement_bind_uuid(statement, 0, station);
	/*************************************************************/
	cass_statement_bind_int64(statement, 1,
			from_daymonthyearhourmin_to_CassandraTime(
				_data.day,
				_data.month,
				_data.year + 2000,
				_data.time / 100,
				_data.time % 100
			)
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
	std::cerr << "Inside temperature: " << _data.insideTemp << " " << from_Farenheight_to_Celsius(_data.insideTemp/10.0) << std::endl;
	if (_data.insideTemp != 32767)
		cass_statement_bind_float(statement, 6, from_Farenheight_to_Celsius(_data.insideTemp/10.0));
	/*************************************************************/
	if (_data.outsideTemp != 32767)
		cass_statement_bind_float(statement, 7, from_Farenheight_to_Celsius(_data.outsideTemp/10.0));
	/*************************************************************/
	std::cerr << "Inside humidity: " << (int)_data.insideHum << std::endl;
	if (_data.insideHum != 255)
		cass_statement_bind_int32(statement, 8, _data.insideHum);
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
		cass_statement_bind_int32(statement, 41, _data.prevailingWindDir);
	/*************************************************************/
	// No 10-min or 2-min average wind speed
	/*************************************************************/
	// No wind gust measurements
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
	// No dew point
	/*************************************************************/
	// No heat index
	/*************************************************************/
	// No wind chill
	/*************************************************************/
	// No THSW index
	/*************************************************************/
	// ET is not exploitable, it's given over the last hour
	/*************************************************************/
	std::string val = from_forecast_to_diagnostic(_data.forecast);
	if (!val.empty())
		cass_statement_bind_string(statement, 64, val.c_str());
	/*************************************************************/
	// No forecast icons
	/*************************************************************/
	// No sunrise time
	/*************************************************************/
	// No sunset time
}

};
