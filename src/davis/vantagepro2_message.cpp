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

#include <dbconnection_observations.h>

#include "vantagepro2_message.h"

namespace meteodata
{
constexpr int VantagePro2Message::CRC_VALUES[];

/**
 * @brief Convert a forecast index to a human-readable description
 *
 * @param value the forecast rule
 *
 * @return a description of the forecast identified by a rule
 */
std::string VantagePro2Message::from_forecast_to_diagnostic(uint8_t value) {
	static const std::map<uint8_t, std::string> forecasts = {
		{0, "Mostly clear and cooler."},
		{1, "Mostly clear with little temperature change."},
		{2, "Mostly clear for 12 hours with little temperature change."},
		{3, "Mostly clear for 12 to 24 hours and cooler."},
		{4, "Mostly clear with little temperature change."},
		{5, "Partly cloudy and cooler."},
		{6, "Partly cloudy with little temperature change."},
		{7, "Partly cloudy with little temperature change."},
		{8, "Mostly clear and warmer."},
		{9, "Partly cloudy with little temperature change."},
		{10, "Partly cloudy with little temperature change."},
		{11, "Mostly clear with little temperature change."},
		{12, "Increasing clouds and warmer. Precipitation possible within 24 to 48 hours."},
		{13, "Partly cloudy with little temperature change."},
		{14, "Mostly clear with little temperature change."},
		{15, "Increasing clouds with little temperature change. Precipitation possible within 24 hours."},
		{16, "Mostly clear with little temperature change."},
		{17, "Partly cloudy with little temperature change."},
		{18, "Mostly clear with little temperature change."},
		{19, "Increasing clouds with little temperature change. Precipitation possible within 12 hours."},
		{20, "Mostly clear with little temperature change."},
		{21, "Partly cloudy with little temperature change."},
		{22, "Mostly clear with little temperature change."},
		{23, "Increasing clouds and warmer. Precipitation possible within 24 hours."},
		{24, "Mostly clear and warmer. Increasing winds."},
		{25, "Partly cloudy with little temperature change."},
		{26, "Mostly clear with little temperature change."},
		{27, "Increasing clouds and warmer. Precipitation possible within 12 hours. Increasing winds."},
		{28, "Mostly clear and warmer. Increasing winds."},
		{29, "Increasing clouds and warmer."},
		{30, "Partly cloudy with little temperature change."},
		{31, "Mostly clear with little temperature change."},
		{32, "Increasing clouds and warmer. Precipitation possible within 12 hours. Increasing winds."},
		{33, "Mostly clear and warmer. Increasing winds."},
		{34, "Increasing clouds and warmer."},
		{35, "Partly cloudy with little temperature change."},
		{36, "Mostly clear with little temperature change."},
		{37, "Increasing clouds and warmer. Precipitation possible within 12 hours. Increasing winds."},
		{38, "Partly cloudy with little temperature change."},
		{39, "Mostly clear with little temperature change."},
		{40, "Mostly clear and warmer. Precipitation possible within 48 hours."},
		{41, "Mostly clear and warmer."},
		{42, "Partly cloudy with little temperature change."},
		{43, "Mostly clear with little temperature change."},
		{44, "Increasing clouds with little temperature change. Precipitation possible within 24 to 48 hours."},
		{45, "Increasing clouds with little temperature change."},
		{46, "Partly cloudy with little temperature change."},
		{47, "Mostly clear with little temperature change."},
		{48, "Increasing clouds and warmer. Precipitation possible within 12 to 24 hours."},
		{49, "Partly cloudy with little temperature change."},
		{50, "Mostly clear with little temperature change."},
		{51, "Increasing clouds and warmer. Precipitation possible within 12 to 24 hours. Windy."},
		{52, "Partly cloudy with little temperature change."},
		{53, "Mostly clear with little temperature change."},
		{54, "Increasing clouds and warmer. Precipitation possible within 12 to 24 hours. Windy."},
		{55, "Partly cloudy with little temperature change."},
		{56, "Mostly clear with little temperature change."},
		{57, "Increasing clouds and warmer. Precipitation possible within 6 to 12 hours."},
		{58, "Partly cloudy with little temperature change."},
		{59, "Mostly clear with little temperature change."},
		{60, "Increasing clouds and warmer. Precipitation possible within 6 to 12 hours. Windy."},
		{61, "Partly cloudy with little temperature change."},
		{62, "Mostly clear with little temperature change."},
		{63, "Increasing clouds and warmer. Precipitation possible within 12 to 24 hours. Windy."},
		{64, "Partly cloudy with little temperature change."},
		{65, "Mostly clear with little temperature change."},
		{66, "Increasing clouds and warmer. Precipitation possible within 12 hours."},
		{67, "Partly cloudy with little temperature change."},
		{68, "Mostly clear with little temperature change."},
		{69, "Increasing clouds and warmer. Precipitation likley."},
		{70, "Clearing and cooler. Precipitation ending within 6 hours."},
		{71, "Partly cloudy with little temperature change."},
		{72, "Clearing and cooler. Precipitation ending within 6 hours."},
		{73, "Mostly clear with little temperature change."},
		{74, "Clearing and cooler. Precipitation ending within 6 hours."},
		{75, "Partly cloudy and cooler."},
		{76, "Partly cloudy with little temperature change."},
		{77, "Mostly clear and cooler."},
		{78, "Clearing and cooler. Precipitation ending within 6 hours."},
		{79, "Mostly clear with little temperature change."},
		{80, "Clearing and cooler. Precipitation ending within 6 hours."},
		{81, "Mostly clear and cooler."},
		{82, "Partly cloudy with little temperature change."},
		{83, "Mostly clear with little temperature change."},
		{84, "Increasing clouds with little temperature change. Precipitation possible within 24 hours."},
		{85, "Mostly cloudy and cooler. Precipitation continuing."},
		{86, "Partly cloudy with little temperature change."},
		{87, "Mostly clear with little temperature change."},
		{88, "Mostly cloudy and cooler. Precipitation likely."},
		{89, "Mostly cloudy with little temperature change. Precipitation continuing."},
		{90, "Mostly cloudy with little temperature change. Precipitation likely."},
		{91, "Partly cloudy with little temperature change."},
		{92, "Mostly clear with little temperature change."},
		{93, "Increasing clouds and cooler. Precipitation possible and windy within 6 hours."},
		{94, "Increasing clouds with little temperature change. Precipitation possible and windy within 6 hours."},
		{95, "Mostly cloudy and cooler. Precipitation continuing. Increasing winds."},
		{96, "Partly cloudy with little temperature change."},
		{97, "Mostly clear with little temperature change."},
		{98, "Mostly cloudy and cooler. Precipitation likely. Increasing winds."},
		{99, "Mostly cloudy with little temperature change. Precipitation continuing. Increasing winds."},
		{100, "Mostly cloudy with little temperature change. Precipitation likely. Increasing winds."},
		{101, "Partly cloudy with little temperature change."},
		{102, "Mostly clear with little temperature change."},
		{103, "Increasing clouds and cooler. Precipitation possible within 12 to 24 hours possible wind shift to the W, NW, or N."},
		{104, "Increasing clouds with little temperature change. Precipitation possible within 12 to 24 hours possible wind shift to the W, NW, or N."},
		{105, "Partly cloudy with little temperature change."},
		{106, "Mostly clear with little temperature change."},
		{107, "Increasing clouds and cooler. Precipitation possible within 6 hours possible wind shift to the W, NW, or N."},
		{108, "Increasing clouds with little temperature change. Precipitation possible within 6 hours possible wind shift to the W, NW, or N."},
		{109, "Mostly cloudy and cooler. Precipitation ending within 12 hours possible wind shift to the W, NW, or N."},
		{110, "Mostly cloudy and cooler. Possible wind shift to the W, NW, or N."},
		{111, "Mostly cloudy with little temperature change. Precipitation ending within 12 hours possible wind shift to the W, NW, or N."},
		{112, "Mostly cloudy with little temperature change. Possible wind shift to the W, NW, or N."},
		{113, "Mostly cloudy and cooler. Precipitation ending within 12 hours possible wind shift to the W, NW, or N."},
		{114, "Partly cloudy with little temperature change."},
		{115, "Mostly clear with little temperature change."},
		{116, "Mostly cloudy and cooler. Precipitation possible within 24 hours possible wind shift to the W, NW, or N."},
		{117, "Mostly cloudy with little temperature change. Precipitation ending within 12 hours possible wind shift to the W, NW, or N."},
		{118, "Mostly cloudy with little temperature change. Precipitation possible within 24 hours possible wind shift to the W, NW, or N."},
		{119, "Clearing, cooler and windy. Precipitation ending within 6 hours."},
		{120, "Clearing, cooler and windy."},
		{121, "Mostly cloudy and cooler. Precipitation ending within 6 hours. Windy with possible wind shift to the W, NW, or N."},
		{122, "Mostly cloudy and cooler. Windy with possible wind shift o the W, NW, or N."},
		{123, "Clearing, cooler and windy."},
		{124, "Partly cloudy with little temperature change."},
		{125, "Mostly clear with little temperature change."},
		{126, "Mostly cloudy with little temperature change. Precipitation possible within 12 hours. Windy."},
		{127, "Partly cloudy with little temperature change."},
		{128, "Mostly clear with little temperature change."},
		{129, "Increasing clouds and cooler. Precipitation possible within 12 hours, possibly heavy at times. Windy."},
		{130, "Mostly cloudy and cooler. Precipitation ending within 6 hours. Windy."},
		{131, "Partly cloudy with little temperature change."},
		{132, "Mostly clear with little temperature change."},
		{133, "Mostly cloudy and cooler. Precipitation possible within 12 hours. Windy."},
		{134, "Mostly cloudy and cooler. Precipitation ending in 12 to 24 hours."},
		{135, "Mostly cloudy and cooler."},
		{136, "Mostly cloudy and cooler. Precipitation continuing, possible heavy at times. Windy."},
		{137, "Partly cloudy with little temperature change."},
		{138, "Mostly clear with little temperature change."},
		{139, "Mostly cloudy and cooler. Precipitation possible within 6 to 12 hours. Windy."},
		{140, "Mostly cloudy with little temperature change. Precipitation continuing, possibly heavy at times. Windy."},
		{141, "Partly cloudy with little temperature change."},
		{142, "Mostly clear with little temperature change."},
		{143, "Mostly cloudy with little temperature change. Precipitation possible within 6 to 12 hours. Windy."},
		{144, "Partly cloudy with little temperature change."},
		{145, "Mostly clear with little temperature change."},
		{146, "Increasing clouds with little temperature change. Precipitation possible within 12 hours, possibly heavy at times. Windy."},
		{147, "Mostly cloudy and cooler. Windy."},
		{148, "Mostly cloudy and cooler. Precipitation continuing, possibly heavy at times. Windy."},
		{149, "Partly cloudy with little temperature change."},
		{150, "Mostly clear with little temperature change."},
		{151, "Mostly cloudy and cooler. Precipitation likely, possibly heavy at times. Windy."},
		{152, "Mostly cloudy with little temperature change. Precipitation continuing, possibly heavy at times. Windy."},
		{153, "Mostly cloudy with little temperature change. Precipitation likely, possibly heavy at times. Windy."},
		{154, "Partly cloudy with little temperature change."},
		{155, "Mostly clear with little temperature change."},
		{156, "Increasing clouds and cooler. Precipitation possible within 6 hours. Windy."},
		{157, "Increasing clouds with little temperature change. Precipitation possible within 6 hours. Windy"},
		{158, "Increasing clouds and cooler. Precipitation continuing. Windy with possible wind shift to the W, NW, or N."},
		{159, "Partly cloudy with little temperature change."},
		{160, "Mostly clear with little temperature change."},
		{161, "Mostly cloudy and cooler. Precipitation likely. Windy with possible wind shift to the W, NW, or N."},
		{162, "Mostly cloudy with little temperature change. Precipitation continuing. Windy with possible wind shift to the W, NW, or N."},
		{163, "Mostly cloudy with little temperature change. Precipitation likely. Windy with possible wind shift to the W, NW, or N."},
		{164, "Increasing clouds and cooler. Precipitation possible within 6 hours. Windy with possible wind shift to the W, NW, or N."},
		{165, "Partly cloudy with little temperature change."},
		{166, "Mostly clear with little temperature change."},
		{167, "Increasing clouds and cooler. Precipitation possible within 6 hours possible wind shift to the W, NW, or N."},
		{168, "Increasing clouds with little temperature change. Precipitation possible within 6 hours. Windy with possible wind shift to the W, NW, or N."},
		{169, "Increasing clouds with little temperature change. Precipitation possible within 6 hours possible wind shift to the W, NW, or N."},
		{170, "Partly cloudy with little temperature change."},
		{171, "Mostly clear with little temperature change."},
		{172, "Increasing clouds and cooler. Precipitation possible within 6 hours. Windy with possible wind shift to the W, NW, or N."},
		{173, "Increasing clouds with little temperature change. Precipitation possible within 6 hours. Windy with possible wind shift to the W, NW, or N."},
		{174, "Partly cloudy with little temperature change."},
		{175, "Mostly clear with little temperature change."},
		{176, "Increasing clouds and cooler. Precipitation possible within 12 to 24 hours. Windy with possible wind shift to the W, NW, or N."},
		{177, "Increasing clouds with little temperature change. Precipitation possible within 12 to 24 hours. Windy with possible wind shift to the W, NW, or N."},
		{178, "Mostly cloudy and cooler. Precipitation possibly heavy at times and ending within 12 hours. Windy with possible wind shift to the W, NW, or N."},
		{179, "Partly cloudy with little temperature change."},
		{180, "Mostly clear with little temperature change."},
		{181, "Mostly cloudy and cooler. Precipitation possible within 6 to 12 hours, possibly heavy at times. Windy with possible wind shift to the W, NW, or N."},
		{182, "Mostly cloudy with little temperature change. Precipitation ending within 12 hours. Windy with possible wind shift to the W, NW, or N."},
		{183, "Mostly cloudy with little temperature change. Precipitation possible within 6 to 12 hours, possibly heavy at times. Windy with possible wind shift to the W, NW, or N."},
		{184, "Mostly cloudy and cooler. Precipitation continuing."},
		{185, "Partly cloudy with little temperature change."},
		{186, "Mostly clear with little temperature change."},
		{187, "Mostly cloudy and cooler. Precipitation likely. Windy with possible wind shift to the W, NW, or N."},
		{188, "Mostly cloudy with little temperature change. Precipitation continuing."},
		{189, "Mostly cloudy with little temperature change. Precipitation likely."},
		{190, "Partly cloudy with little temperature change."},
		{191, "Mostly clear with little temperature change."},
		{192, "Mostly cloudy and cooler. Precipitation possible within 12 hours, possibly heavy at times. Windy."},
		{193, "FORECAST REQUIRES 3 HOURS OF RECENT DATA"},
		{194, "Mostly clear and cooler."},
		{195, "Mostly clear and cooler."},
		{196, "Mostly clear and cooler."}
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
std::string VantagePro2Message::from_bartrend_to_diagnostic(uint8_t value) {
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
	std::cerr << "Inside temperature: " << _l1.insideTemperature << " " << from_Farenheit_to_Celsius(_l1.insideTemperature/10.0) << std::endl;
	if (_l1.insideTemperature != 32767)
		cass_statement_bind_float(statement, 6, from_Farenheit_to_Celsius(_l1.insideTemperature/10.0));
	/*************************************************************/
	if (_l1.outsideTemperature != 32767)
		cass_statement_bind_float(statement, 7, from_Farenheit_to_Celsius(_l1.outsideTemperature/10.0));
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
			cass_statement_bind_float(statement, 10+i, from_Farenheit_to_Celsius(_l1.extraTemp[i] - 90));
	}
	/*************************************************************/
	for (int i=0 ; i<4 ; i++) {
		if (_l1.soilTemp[i] != 255)
			cass_statement_bind_float(statement, 17+i, from_Farenheit_to_Celsius(_l1.soilTemp[i] - 90));
		if (_l1.leafTemp[i] != 255)
			cass_statement_bind_float(statement, 21+i, from_Farenheit_to_Celsius(_l1.leafTemp[i] - 90));
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
		if (_l1.leafWetnesses[i] <= 15)
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
		cass_statement_bind_float(statement, 57, from_Farenheit_to_Celsius(_l2.dewPoint));
	/*************************************************************/
	if (_l2.heatIndex != 255)
		cass_statement_bind_float(statement, 58, from_Farenheit_to_Celsius(_l2.heatIndex));
	/*************************************************************/
	if (_l2.windChill != 255)
		cass_statement_bind_float(statement, 59, from_Farenheit_to_Celsius(_l2.windChill));
	/*************************************************************/
	if (_l2.thswIndex != 255)
		cass_statement_bind_float(statement, 60, from_Farenheit_to_Celsius(_l2.thswIndex));
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
	/*************************************************************/
	// No rain archive
	// No ETP archive
}

void VantagePro2Message::populateV2DataPoint(const CassUuid stationId, CassStatement* const statement) const
{
	std::cerr << "Populating the new datapoint" << std::endl;
	/*************************************************************/
	cass_statement_bind_uuid(statement, 0, stationId);
	/*************************************************************/
	cass_statement_bind_uint32(statement, 1, cass_date_from_epoch(time(NULL)));
	cass_statement_bind_int64(statement, 2, 1000*time(NULL));
	/*************************************************************/
	std::cerr << "barometer: " << from_inHg_to_bar(_l2.barometer) << std::endl;
	cass_statement_bind_float(statement, 3, from_inHg_to_bar(_l2.barometer));
	/*************************************************************/
	if (_l2.dewPoint != 255)
		cass_statement_bind_float(statement, 4, from_Farenheit_to_Celsius(_l2.dewPoint));
	/*************************************************************/
	for (int i=0 ; i<2 ; i++) {
		if (_l1.extraHum[i] != 255)
			cass_statement_bind_int32(statement, 5+i, _l1.extraHum[i]);
	}
	/*************************************************************/
	for (int i=0 ; i<3 ; i++) {
		if (_l1.extraTemp[1] != 255)
			cass_statement_bind_float(statement, 7+i, from_Farenheit_to_Celsius(_l1.extraTemp[i] - 90));
	}
	/*************************************************************/
	if (_l2.heatIndex != 255)
		cass_statement_bind_float(statement, 10, from_Farenheit_to_Celsius(_l2.heatIndex));
	/*************************************************************/
	std::cerr << "Inside insideHumidity: " << (int)_l1.insideHumidity << std::endl;
	if (_l1.insideHumidity != 255)
		cass_statement_bind_int32(statement, 11, _l1.insideHumidity);
	/*************************************************************/
	std::cerr << "Inside temperature: " << _l1.insideTemperature << " " << from_Farenheit_to_Celsius(_l1.insideTemperature/10.0) << std::endl;
	if (_l1.insideTemperature != 32767)
		cass_statement_bind_float(statement, 12, from_Farenheit_to_Celsius(_l1.insideTemperature/10.0));
	/*************************************************************/
	for (int i=0 ; i<2 ; i++) {
		if (_l1.leafTemp[i] != 255)
			cass_statement_bind_float(statement, 13+i, from_Farenheit_to_Celsius(_l1.leafTemp[i] - 90));
		if (_l1.leafWetnesses[i] <= 15)
			cass_statement_bind_int32(statement, 15+i, _l1.leafWetnesses[i]);
	}
	/*************************************************************/
	if (_l1.outsideHumidity != 255)
		cass_statement_bind_int32(statement, 17, _l1.outsideHumidity);
	/*************************************************************/
	if (_l1.outsideTemperature != 32767)
		cass_statement_bind_float(statement, 18, from_Farenheit_to_Celsius(_l1.outsideTemperature/10.0));
	/*************************************************************/
	if (_l1.rainRate != 65535)
		cass_statement_bind_float(statement, 19, from_rainrate_to_mm(_l1.rainRate));
	/*************************************************************/
	// No rainfall
	/*************************************************************/
	// No evapotranspiration
	/*************************************************************/
	for (int i=0 ; i<4 ; i++) {
		if (_l1.soilMoistures[i] != 255)
			cass_statement_bind_int32(statement, 22+i, _l1.soilMoistures[i]);
	}
	/*************************************************************/
	for (int i=0 ; i<4 ; i++) {
		if (_l1.soilTemp[i] != 255)
			cass_statement_bind_float(statement, 26+i, from_Farenheit_to_Celsius(_l1.soilTemp[i] - 90));
	}
	/*************************************************************/
	if (_l2.solarRad != 32767)
		cass_statement_bind_int32(statement, 30, _l2.solarRad);
	/*************************************************************/
	if (_l2.thswIndex != 255)
		cass_statement_bind_float(statement, 31, from_Farenheit_to_Celsius(_l2.thswIndex));
	/*************************************************************/
	if (_l2.uv != 255)
		cass_statement_bind_int32(statement, 32, _l2.uv);
	/*************************************************************/
	if (_l2.windChill != 255)
		cass_statement_bind_float(statement, 33, from_Farenheit_to_Celsius(_l2.windChill));
	/*************************************************************/
	if (_l1.windDir != 32767)
		cass_statement_bind_int32(statement, 34, _l1.windDir);
	/*************************************************************/
	if (_l2.tenMinWindGust != 255)
		cass_statement_bind_float(statement, 35, from_mph_to_kph(_l2.tenMinWindGust));
	/*************************************************************/
	if (_l2.twoMinAvgWindSpeed != 32767)
		cass_statement_bind_float(statement, 36, from_mph_to_kph(_l2.twoMinAvgWindSpeed) / 10);
	/*************************************************************/
	// No insolation
	/*************************************************************/
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
}
