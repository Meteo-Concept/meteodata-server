/**
 * @file vantagepro2message.cpp
 * @brief Implementation of the VantagePro2Message class
 * @author Laurent Georget
 * @date 2016-10-05
 */
/*
 * Copyright (C) 2016  SAS Météo Concept <contact@meteo-concept.fr>
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

#include <ctime>
#include <cstring>
#include <iostream>
#include <map>
#include <iterator>
#include <algorithm>

#include "vantagepro2message.h"
#include "dbconnection.h"

namespace meteodata
{
constexpr int VantagePro2Message::CRC_VALUES[];

void VantagePro2Message::populateDataPoint(const CassUuid stationId, CassStatement* const statement) const
{
	std::cerr << "Populating the new datapoint" << std::endl;
	/*************************************************************/
	cass_statement_bind_uuid(statement, 0, stationId);
	/*************************************************************/
	cass_statement_bind_int64(statement, 1, 1000*time(NULL));
	/*************************************************************/
	std::string val = from_bartrend_to_diagnostic(_l1.barTrend);
	if (!val.empty())
		cass_statement_bind_string(statement, 2, val.c_str());
	/*************************************************************/
	std::cerr << "barometer: " << from_inHg_to_bar(_l2.barometer) << std::endl;
	cass_statement_bind_float(statement, 3, from_inHg_to_bar(_l2.barometer));
	/*************************************************************/
	std::cerr << "absBarPressure: " << from_inHg_to_bar(_l2.absBarPressure) << std::endl;
	cass_statement_bind_float(statement, 4, from_inHg_to_bar(_l2.absBarPressure));
	/*************************************************************/
	cass_statement_bind_float(statement, 5, from_inHg_to_bar(_l2.barSensorRaw));
	/*************************************************************/
	std::cerr << "Inside temperature: " << _l1.insideTemperature << " " << from_Farenheight_to_Celsius(_l1.insideTemperature/10.0) << std::endl;
	if (_l1.insideTemperature != 32767)
		cass_statement_bind_float(statement, 6, from_Farenheight_to_Celsius(_l1.insideTemperature/10.0));
	/*************************************************************/
	if (_l1.outsideTemperature != 32767)
		cass_statement_bind_float(statement, 7, from_Farenheight_to_Celsius(_l1.outsideTemperature/10.0));
	/*************************************************************/
	std::cerr << "Inside insideHumidity: " << (int)_l1.insideHumidity << std::endl;
	if (_l1.insideHumidity != 255)
		cass_statement_bind_int32(statement, 8, _l1.insideHumidity);
	/*************************************************************/
	if (_l1.outsideHumidity != 255)
		cass_statement_bind_int32(statement, 9, _l1.outsideHumidity);
	/*************************************************************/
	for (int i=0 ; i<7 ; i++) {
		if (_l1.extraTemp[1] != 255)
			cass_statement_bind_float(statement, 10+i, from_Farenheight_to_Celsius(_l1.extraTemp[i] - 90));
	}
	/*************************************************************/
	for (int i=0 ; i<4 ; i++) {
		if (_l1.soilTemp[i] != 255)
			cass_statement_bind_float(statement, 17+i, from_Farenheight_to_Celsius(_l1.soilTemp[i] - 90));
		if (_l1.leafTemp[i] != 255)
			cass_statement_bind_float(statement, 21+i, from_Farenheight_to_Celsius(_l1.leafTemp[i] - 90));
	}
	/*************************************************************/
	for (int i=0 ; i<7 ; i++) {
		if (_l1.extraHum[i] != 255)
			cass_statement_bind_int32(statement, 25+i, _l1.extraHum[i]);
	}
	/*************************************************************/
	for (int i=0 ; i<4 ; i++) {
		if (_l1.soilMoistures[i] != 255)
			cass_statement_bind_int32(statement, 32+i, _l1.soilMoistures[i]);
		if (_l1.leafWetnesses[i] >= 0 && _l1.leafWetnesses[i] <= 15)
			cass_statement_bind_int32(statement, 36+i, _l1.leafWetnesses[i]);
	}
	/*************************************************************/
	if (_l1.windSpeed != 255)
		cass_statement_bind_float(statement, 40, from_mph_to_kph(_l1.windSpeed));
	/*************************************************************/
	if (_l1.windDir != 32767)
		cass_statement_bind_int32(statement, 41, _l1.windDir);
	/*************************************************************/
	if (_l2.tenMinAvgWindSpeed != 32767)
		cass_statement_bind_float(statement, 42, from_mph_to_kph(_l2.tenMinAvgWindSpeed) / 10);
	/*************************************************************/
	if (_l2.twoMinAvgWindSpeed != 32767)
		cass_statement_bind_float(statement, 43, from_mph_to_kph(_l2.twoMinAvgWindSpeed) / 10);
	/*************************************************************/
	if (_l2.tenMinWindGust != 255)
		cass_statement_bind_float(statement, 44, from_mph_to_kph(_l2.tenMinWindGust));
	/*************************************************************/
	if (_l2.windGustDir != 65535)
		cass_statement_bind_float(statement, 45, _l2.windGustDir);
	/*************************************************************/
	if (_l1.rainRate != 65535)
		cass_statement_bind_float(statement, 46, from_rainrate_to_mm(_l1.rainRate));
	/*************************************************************/
	cass_statement_bind_float(statement, 47, from_rainrate_to_mm(_l2.last15MinRain));
	/*************************************************************/
	cass_statement_bind_float(statement, 48, from_rainrate_to_mm(_l2.lastHourRain));
	/*************************************************************/
	cass_statement_bind_float(statement, 49, from_rainrate_to_mm(_l2.last24HoursRain));
	/*************************************************************/
	cass_statement_bind_float(statement, 50, from_rainrate_to_mm(_l1.dayRain));
	/*************************************************************/
	cass_statement_bind_float(statement, 51, from_rainrate_to_mm(_l1.monthRain));
	/*************************************************************/
	cass_statement_bind_float(statement, 52, from_rainrate_to_mm(_l1.yearRain));
	/*************************************************************/
	cass_statement_bind_float(statement, 53, from_in_to_mm(_l2.stormRain) / 100);
	/*************************************************************/
	if (_l2.monthStartDateCurrentStorm >= 1 &&
	    _l2.monthStartDateCurrentStorm <= 12 &&
	    _l2.dayStartDateCurrentStorm >= 1 &&
	    _l2.dayStartDateCurrentStorm <= 31)
		cass_statement_bind_uint32(statement, 54,
			from_daymonthyear_to_CassandraDate(
				_l2.dayStartDateCurrentStorm + 2000,
				_l2.monthStartDateCurrentStorm,
				_l2.yearStartDateCurrentStorm));
	/*************************************************************/
	if (_l2.uv != 255)
		cass_statement_bind_int32(statement, 55, _l2.uv);
	/*************************************************************/
	if (_l2.solarRad != 32767)
		cass_statement_bind_int32(statement, 56, _l2.solarRad);
	/*************************************************************/
	if (_l2.dewPoint != 255)
		cass_statement_bind_float(statement, 57, from_Farenheight_to_Celsius(_l2.dewPoint));
	/*************************************************************/
	if (_l2.heatIndex != 255)
		cass_statement_bind_float(statement, 58, from_Farenheight_to_Celsius(_l2.heatIndex));
	/*************************************************************/
	if (_l2.windChill != 255)
		cass_statement_bind_float(statement, 59, from_Farenheight_to_Celsius(_l2.windChill));
	/*************************************************************/
	if (_l2.thswIndex != 255)
		cass_statement_bind_float(statement, 60, from_Farenheight_to_Celsius(_l2.thswIndex));
	/*************************************************************/
	if (_l1.dayET != 65535)
		cass_statement_bind_float(statement, 61, from_in_to_mm(_l1.dayET) / 1000);
	/*************************************************************/
	if (_l1.monthET != 65535)
		cass_statement_bind_float(statement, 62, from_in_to_mm(_l1.monthET) / 100);
	/*************************************************************/
	if (_l1.yearET != 65535)
		cass_statement_bind_float(statement, 63, from_in_to_mm(_l1.yearET) / 100);
	/*************************************************************/
	val = from_forecast_to_diagnostic(_l1.forecastRuleNumber);
	if (!val.empty())
		cass_statement_bind_string(statement, 64, val.c_str());
	/*************************************************************/
	cass_statement_bind_int32(statement, 65, _l1.forecastIcons);
	/*************************************************************/
	cass_statement_bind_int64(statement, 66,
		from_hourmin_to_CassandraTime(
			_l1.timeOfSunrise / 100,
			_l1.timeOfSunrise % 100
		));
	/*************************************************************/
	cass_statement_bind_int64(statement, 67,
		from_hourmin_to_CassandraTime(
			_l1.timeOfSunset / 100,
			_l1.timeOfSunset % 100
		));
}

bool VantagePro2Message::validateCRC(const void* msg, size_t len)
{
	//byte-wise reading
	const uint8_t* bytes = reinterpret_cast<const uint8_t*>(msg);
	unsigned int crc = 0;
	for (unsigned int i=0 ; i<len ; i++) {
		uint8_t index = (crc >> 8) ^ bytes[i];
		crc = CRC_VALUES[index] ^ ((crc << 8) & 0xFFFF);
		if (i == len - 3)
			std::cerr << "CRC should be equal to " << std::hex << crc << std::dec << std::endl;
	}

	return crc == 0;
}

void VantagePro2Message::computeCRC(void* msg, size_t len)
{
	uint8_t* bytes = reinterpret_cast<uint8_t*>(msg);
	unsigned int crc = 0;
	unsigned int i;
	for (i=0 ; i<len-2 ; i++) {
		uint8_t index = (crc >> 8) ^ bytes[i];
		crc = CRC_VALUES[index] ^ ((crc << 8) & 0xFFFF);
	}

	std::cerr << "CRC computed: " << std::hex << crc << std::dec << std::endl;

	bytes[i]   = (crc & 0xFF00) >> 8;
	bytes[i+1] = (crc & 0x00FF);
}

bool VantagePro2Message::isValid() const
{
	return validateCRC(&_l1, sizeof(Loop1)) &&
	       validateCRC(&_l2, sizeof(Loop2));
}
};
