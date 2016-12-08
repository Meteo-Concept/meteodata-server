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

#include "vantagepro2message.h"
#include "dbconnection.h"

namespace {
/**
 * @brief Convert a forecast index to a human-readable description
 *
 * @param value the forecast index
 *
 * @return a description of the forecast identified by index
 */
std::string from_forecast_to_diagnostic(uint8_t value) {
	static const std::map<uint8_t, std::string> forecasts = {
		{8,  "Mostly Clear"},
		{6,  "Partly Cloudy"},
		{2,  "Mostly Cloudy"},
		{3,  "Mostly Cloudy, Rain within 12 hours"},
		{18, "Mostly Cloudy, Snow within 12 hours"},
		{19, "Mostly Cloudy, Rain or snow within 12 hours"},
		{7,  "Partly Cloudy, Rain within 12 hours"},
		{22, "Partly Cloudy, Snow within 12 hours"},
		{23, "Partly Cloudy, Rain or Snow within 12 hours"}
	};
	auto it = forecasts.find(value);
	return (it == forecasts.cend()) ? std::string() : it->second;
}

/**
 * @brief Convert a barometric trend value to a human-readable description
 *
 * @param value the barometric trend value
 *
 * @return the textual description corresponding to the barometric trend value
 */
std::string from_bartrend_to_diagnostic(uint8_t value) {
	static const std::map<uint8_t, std::string> bartrends = {
		{196, "Falling rapidly"},
		{236, "Falling slowly"},
		{0,   "Steady"},
		{20,  "Raising slowly"},
		{60,  "Raising rapidly"}
	};
	auto it = bartrends.find(value);
	return (it == bartrends.cend()) ? std::string() : it->second;
}

/**
 * @brief Convert a date to a value that can be entered in a Cassandra column
 * of type "date"
 *
 * @param day the day
 * @param month the month
 * @param year the year
 *
 * @return a value corresponding to the date given as parameter suitable for
 * insertion in a Cassandra database
 */
uint32_t from_daymonthyear_to_CassandraDate(int day, int month, int year)
{
	struct tm date;
	memset(&date, 0, sizeof(date));
	date.tm_mday = day;
	date.tm_mon = month;
	date.tm_year = year;
	return cass_date_from_epoch(mktime(&date));
}

/** @brief Convert an hour and minute value to a value that can be entered in a
 * Cassandra column of type "time"
 *
 * @param hour the hour
 * @param min the minutes
 *
 * @return a value corresponding to the time given as parameter suitable for
 * insertion in a Cassandra database
 */
int64_t from_hourmin_to_CassandraTime(int hour, int min)
{
	struct tm date;
	time_t currentTime = time(NULL);
	localtime_r(&currentTime, &date);
	date.tm_hour = hour;
	date.tm_min = min;
	date.tm_sec = 0;
	return cass_time_from_epoch(mktime(&date));
}

/**
 * @brief Convert a pression given in inches of mercury to bar
 *
 * @param inHg the value to convert
 *
 * @return the parameter value converted to bar
 */
inline float from_inHg_to_bar(int inHg)
{
	return inHg * 0.03386;
}

/**
 * @brief Convert a temperature given in Farenheight degrees to Celsius degrees
 *
 * @param f the value to convert
 *
 * @return the parameter value converted to Celsius degrees
 */
inline float from_Farenheight_to_Celsius(float f)
{
	return (f - 32.0) / 1.80;
}

/**
 * @brief Convert a velocity from miles per hour to meters per second
 *
 * @param mph the value to convert
 *
 * @return the parameter value converted to meters per second
 */
inline float from_mph_to_mps(int mph)
{
	return mph * 0.44704;
}

/**
 * @brief Convert a velocity from miles per hour to kilometers per hour
 *
 * @param mph the value to convert
 *
 * @return the parameter value converted to kilometers per hour
 */
inline float from_mph_to_kph(int mph)
{
	return mph * 1.609;
}

/**
 * @brief Convert a distance from inches to millimeters
 *
 * @param in the value to convert
 *
 * @return the parameter value converted to millimeters
 */
inline float from_in_to_mm(int in)
{
	return in * 25.4;
}

/**
 * @brief Convert a number of rain clicks to millimeters of rain
 *
 * @param rr the value to convert
 *
 * @return the parameter value converted to millimeters of rain
 */
inline float from_rainrate_to_mm(int rr)
{
	//assume the raw value is in 0.2mm/hour, this is configurable
	return rr * 0.2;
}
}

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
	if (val.empty())
		cass_statement_bind_null(statement, 2);
	else
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
	cass_statement_bind_float(statement, 6, from_Farenheight_to_Celsius(_l1.insideTemperature/10.0));
	/*************************************************************/
	if (_l1.outsideTemperature == 32767)
		cass_statement_bind_null(statement, 7);
	else
		cass_statement_bind_float(statement, 7, from_Farenheight_to_Celsius(_l1.outsideTemperature/10.0));
	/*************************************************************/
	std::cerr << "Inside insideHumidity: " << (int)_l1.insideHumidity << std::endl;
	cass_statement_bind_int32(statement, 8, _l1.insideHumidity);
	/*************************************************************/
	if (_l1.outsideHumidity == 255)
		cass_statement_bind_null(statement, 9);
	else
		cass_statement_bind_int32(statement, 9, _l1.outsideHumidity);
	/*************************************************************/
	for (int i=0 ; i<7 ; i++) {
		if (_l1.extraTemp[1] == 255)
			cass_statement_bind_null(statement, 10+i);
		else
			cass_statement_bind_float(statement, 10+i, from_Farenheight_to_Celsius(_l1.extraTemp[i] - 90));
	}
	/*************************************************************/
	for (int i=0 ; i<4 ; i++) {
		if (_l1.soilTemp[i] == 255)
			cass_statement_bind_null(statement, 17+i);
		else
			cass_statement_bind_float(statement, 17+i, from_Farenheight_to_Celsius(_l1.soilTemp[i] - 90));
		if (_l1.leafTemp[i] == 255)
			cass_statement_bind_null(statement, 21+i);
		else
			cass_statement_bind_float(statement, 21+i, from_Farenheight_to_Celsius(_l1.leafTemp[i] - 90));
	}
	/*************************************************************/
	for (int i=0 ; i<7 ; i++) {
		if (_l1.extraHum[i] == 255)
			cass_statement_bind_null(statement, 25+i);
		else
			cass_statement_bind_int32(statement, 25+i, _l1.extraHum[i]);
	}
	/*************************************************************/
	for (int i=0 ; i<4 ; i++) {
		if (_l1.soilMoistures[i] == 255)
			cass_statement_bind_null(statement, 32+i);
		else
			cass_statement_bind_int32(statement, 32+i, _l1.soilMoistures[i]);
		if (_l1.leafWetnesses[i] >= 0 && _l1.leafWetnesses[i] <= 15)
			cass_statement_bind_int32(statement, 36+i, _l1.leafWetnesses[i]);
		else
			cass_statement_bind_null(statement, 36+i);
	}
	/*************************************************************/
	if (_l1.windSpeed == 255)
		cass_statement_bind_null(statement, 40);
	else
		cass_statement_bind_float(statement, 40, from_mph_to_kph(_l1.windSpeed));
	/*************************************************************/
	if (_l1.windDir == 32767)
		cass_statement_bind_null(statement, 41);
	else
		cass_statement_bind_int32(statement, 41, _l1.windDir);
	/*************************************************************/
	if (_l2.tenMinAvgWindSpeed == 32767)
		cass_statement_bind_null(statement, 42);
	else
		cass_statement_bind_float(statement, 42, from_mph_to_kph(_l2.tenMinAvgWindSpeed) / 10);
	/*************************************************************/
	if (_l2.twoMinAvgWindSpeed == 32767)
		cass_statement_bind_null(statement, 43);
	else
		cass_statement_bind_float(statement, 43, from_mph_to_kph(_l2.twoMinAvgWindSpeed) / 10);
	/*************************************************************/
	if (_l2.tenMinWindGust == 255)
		cass_statement_bind_null(statement, 44);
	else
		cass_statement_bind_float(statement, 44, from_mph_to_kph(_l2.tenMinWindGust));
	/*************************************************************/
	if (_l2.windGustDir == 65535)
		cass_statement_bind_null(statement, 45);
	else
		cass_statement_bind_float(statement, 45, _l2.windGustDir);
	/*************************************************************/
	if (_l1.rainRate == 65535)
		cass_statement_bind_null(statement, 46);
	else
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
	else
		cass_statement_bind_null(statement, 54);
	/*************************************************************/
	if (_l2.uv == 255)
		cass_statement_bind_null(statement, 55);
	else
		cass_statement_bind_int32(statement, 55, _l2.uv);
	/*************************************************************/
	if (_l2.solarRad == 32767)
		cass_statement_bind_null(statement,56);
	else
		cass_statement_bind_int32(statement, 56, _l2.solarRad);
	/*************************************************************/
	if (_l2.dewPoint == 255)
		cass_statement_bind_null(statement, 57);
	else
		cass_statement_bind_float(statement, 57, from_Farenheight_to_Celsius(_l2.dewPoint));
	/*************************************************************/
	if (_l2.heatIndex == 255)
		cass_statement_bind_null(statement, 58);
	else
		cass_statement_bind_float(statement, 58, from_Farenheight_to_Celsius(_l2.heatIndex));
	/*************************************************************/
	if (_l2.windChill == 255)
		cass_statement_bind_null(statement, 59);
	else
		cass_statement_bind_float(statement, 59, from_Farenheight_to_Celsius(_l2.windChill));
	/*************************************************************/
	if (_l2.thswIndex == 255)
		cass_statement_bind_null(statement, 60);
	else
		cass_statement_bind_float(statement, 60, from_Farenheight_to_Celsius(_l2.thswIndex));
	/*************************************************************/
	if (_l1.dayET == 65535)
		cass_statement_bind_null(statement, 61);
	else
		cass_statement_bind_float(statement, 61, from_in_to_mm(_l1.dayET) / 1000);
	/*************************************************************/
	if (_l1.monthET == 65535)
		cass_statement_bind_null(statement, 62);
	else
		cass_statement_bind_float(statement, 62, from_in_to_mm(_l1.monthET) / 100);
	/*************************************************************/
	if (_l1.yearET == 65535)
		cass_statement_bind_null(statement, 63);
	else
		cass_statement_bind_float(statement, 63, from_in_to_mm(_l1.yearET) / 100);
	/*************************************************************/
	val = from_forecast_to_diagnostic(_l1.forecastIcons);
	if (val.empty())
		cass_statement_bind_null(statement, 64);
	else
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
	}

	return crc == 0;
}

bool VantagePro2Message::isValid() const
{
	return validateCRC(&_l1, sizeof(Loop1)) &&
	       validateCRC(&_l2, sizeof(Loop2));
}
};
