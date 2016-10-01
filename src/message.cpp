#include <ctime>
#include <cstring>
#include <map>

#include "message.h"
#include "dbconnection.h"

namespace {
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

	uint32_t from_daymonthyear_to_CassandraDate(int day, int month, int year)
	{
		struct tm date;
		memset(&date, 0, sizeof(date));
		date.tm_mday = day;
		date.tm_mon = month;
		date.tm_year = year;
		return cass_date_from_epoch(mktime(&date));
	}

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

	inline float from_inHg_to_bar(int bar)
	{
		return bar * 0.03386;
	}

	inline float from_Farenheight_to_Celsius(int f)
	{
		return (f - 32.0) / 1.80;
	}

	inline float from_mph_to_mps(int mph)
	{
		return mph * 0.44704;
	}

	inline float from_mph_to_kph(int mph)
	{
		return mph * 1.609;
	}

	inline float from_in_to_mm(int in)
	{
		return in * 25.4;
	}

	inline float from_rainrate_to_mm(int rr)
	{
		//assume the raw value is in 0.2mm/hour
		return rr * 0.2;
	}
}

namespace meteodata
{
	void populateDataPoint(const CassUuid stationId, const Loop1& l1, const Loop2& l2, CassStatement* const statement)
	{
		std::cerr << "Populating the new datapoint" << std::endl;
		/*************************************************************/
		cass_statement_bind_uuid(statement, 0, stationId);
		/*************************************************************/
		cass_statement_bind_int64(statement, 1, 1000*time(NULL));
		/*************************************************************/
		std::string val = from_bartrend_to_diagnostic(l1.barTrend);
		if (val.empty())
			cass_statement_bind_null(statement, 2);
		else
			cass_statement_bind_string(statement, 2, val.c_str());
		/*************************************************************/
		std::cerr << "barometer: " << from_inHg_to_bar(l2.barometer) << std::endl;
		cass_statement_bind_float(statement, 3, from_inHg_to_bar(l2.barometer));
		/*************************************************************/
		std::cerr << "absBarPressure: " << from_inHg_to_bar(l2.absBarPressure) << std::endl;
		cass_statement_bind_float(statement, 4, from_inHg_to_bar(l2.absBarPressure));
		/*************************************************************/
		cass_statement_bind_float(statement, 5, from_inHg_to_bar(l2.barSensorRaw));
		/*************************************************************/
		std::cerr << "Inside temperature: " << l1.insideTemperature << " " << from_Farenheight_to_Celsius(l1.insideTemperature/10) << std::endl;
		cass_statement_bind_float(statement, 6, from_Farenheight_to_Celsius(l1.insideTemperature/10));
		/*************************************************************/
		if (l1.outsideTemperature == 32767)
			cass_statement_bind_null(statement, 7);
		else
			cass_statement_bind_float(statement, 7, from_Farenheight_to_Celsius(l1.outsideTemperature/10));
		/*************************************************************/
		std::cerr << "Inside insideHumidity: " << (int)l1.insideHumidity << std::endl;
		cass_statement_bind_float(statement, 8, l1.insideHumidity);
		/*************************************************************/
		if (l1.outsideHumidity == 255)
			cass_statement_bind_null(statement, 9);
		else
			cass_statement_bind_float(statement, 9, l1.outsideHumidity);
		/*************************************************************/
		for (int i=0 ; i<7 ; i++) {
			if (l1.extraTemp[1] == 255)
				cass_statement_bind_null(statement, 10+i);
			else
				cass_statement_bind_float(statement, 10+i, from_Farenheight_to_Celsius(l1.extraTemp[i] - 90));
		}
		/*************************************************************/
		for (int i=0 ; i<4 ; i++) {
			if (l1.soilTemp[i] == 255)
				cass_statement_bind_null(statement, 17+i);
			else
				cass_statement_bind_float(statement, 17+i, from_Farenheight_to_Celsius(l1.soilTemp[i] - 90));
			if (l1.leafTemp[i] == 255)
				cass_statement_bind_null(statement, 21+i);
			else
				cass_statement_bind_float(statement, 21+i, from_Farenheight_to_Celsius(l1.leafTemp[i] - 90));
		}
		/*************************************************************/
		for (int i=0 ; i<7 ; i++) {
			if (l1.extraHum[i] == 255)
				cass_statement_bind_null(statement, 25+i);
			else
				cass_statement_bind_float(statement, 25+i, l1.extraHum[i]);
		}
		/*************************************************************/
		for (int i=0 ; i<4 ; i++) {
			if (l1.soilMoistures[i] == 255)
				cass_statement_bind_float(statement, 32+i, l1.soilMoistures[i]);
			else
				cass_statement_bind_null(statement, 32+i);
			if (l1.leafWetnesses[i] >= 0 && l1.leafWetnesses[i] <= 15)
				cass_statement_bind_float(statement, 36+i, l1.leafWetnesses[i]);
			else
				cass_statement_bind_null(statement, 36+i);
		}
		/*************************************************************/
		if (l1.windSpeed == 255)
			cass_statement_bind_null(statement, 40);
		else
			cass_statement_bind_float(statement, 40, from_mph_to_kph(l1.windSpeed));
		/*************************************************************/
		if (l1.windDir == 32767)
			cass_statement_bind_null(statement, 41);
		else
			cass_statement_bind_float(statement, 41, l1.windDir);
		/*************************************************************/
		if (l2.tenMinAvgWindSpeed == 32767)
			cass_statement_bind_null(statement, 42);
		else
			cass_statement_bind_float(statement, 42, from_mph_to_kph(l2.tenMinAvgWindSpeed / 10));
		/*************************************************************/
		if (l2.twoMinAvgWindSpeed == 32767)
			cass_statement_bind_null(statement, 43);
		else
			cass_statement_bind_float(statement, 43, from_mph_to_kph(l2.twoMinAvgWindSpeed / 10));
		/*************************************************************/
		if (l2.tenMinWindGust == 255)
			cass_statement_bind_null(statement, 44);
		else
			cass_statement_bind_float(statement, 44, from_mph_to_kph(l2.tenMinWindGust / 10));
		/*************************************************************/
		if (l2.windGustDir == 65535)
			cass_statement_bind_null(statement, 45);
		else
			cass_statement_bind_float(statement, 45, l2.windGustDir);
		/*************************************************************/
		if (l1.rainRate == 65535)
			cass_statement_bind_null(statement, 46);
		else
			cass_statement_bind_float(statement, 46, from_rainrate_to_mm(l1.rainRate));
		/*************************************************************/
		cass_statement_bind_float(statement, 47, from_rainrate_to_mm(l2.last15MinRain));
		/*************************************************************/
		cass_statement_bind_float(statement, 48, from_rainrate_to_mm(l2.lastHourRain));
		/*************************************************************/
		cass_statement_bind_float(statement, 49, from_rainrate_to_mm(l2.last24HoursRain));
		/*************************************************************/
		cass_statement_bind_float(statement, 50, from_rainrate_to_mm(l1.dayRain));
		/*************************************************************/
		cass_statement_bind_float(statement, 51, from_rainrate_to_mm(l1.monthRain));
		/*************************************************************/
		cass_statement_bind_float(statement, 52, from_rainrate_to_mm(l1.yearRain));
		/*************************************************************/
		cass_statement_bind_float(statement, 53, from_in_to_mm(l2.stormRain / 100));
		/*************************************************************/
		if (l2.monthStartDateCurrentStorm >= 1 &&
		    l2.monthStartDateCurrentStorm <= 12 &&
		    l2.dayStartDateCurrentStorm >= 1 &&
		    l2.dayStartDateCurrentStorm <= 31)
			cass_statement_bind_uint32(statement, 54,
				from_daymonthyear_to_CassandraDate(
					l2.dayStartDateCurrentStorm + 2000,
					l2.monthStartDateCurrentStorm,
					l2.yearStartDateCurrentStorm));
		else
			cass_statement_bind_null(statement, 54);
		/*************************************************************/
		if (l2.uv == 255)
			cass_statement_bind_null(statement, 55);
		else
			cass_statement_bind_int32(statement, 55, l2.uv);
		/*************************************************************/
		if (l2.solarRad == 32767)
			cass_statement_bind_null(statement,56);
		else
			cass_statement_bind_float(statement, 56, l2.solarRad);
		/*************************************************************/
		if (l2.dewPoint == 255)
			cass_statement_bind_null(statement, 57);
		else
			cass_statement_bind_float(statement, 57, from_Farenheight_to_Celsius(l2.dewPoint));
		/*************************************************************/
		if (l2.heatIndex == 255)
			cass_statement_bind_null(statement, 58);
		else
			cass_statement_bind_float(statement, 58, from_Farenheight_to_Celsius(l2.heatIndex));
		/*************************************************************/
		if (l2.windChill == 255)
			cass_statement_bind_null(statement, 59);
		else
			cass_statement_bind_float(statement, 59, from_Farenheight_to_Celsius(l2.windChill));
		/*************************************************************/
		if (l2.thswIndex == 255)
			cass_statement_bind_null(statement, 60);
		else
			cass_statement_bind_float(statement, 60, from_Farenheight_to_Celsius(l2.thswIndex));
		/*************************************************************/
		if (l1.dayET == 65535)
			cass_statement_bind_null(statement, 61);
		else
			cass_statement_bind_float(statement, 61, from_in_to_mm(l1.dayET) / 1000);
		/*************************************************************/
		if (l1.monthET == 65535)
			cass_statement_bind_null(statement, 62);
		else
			cass_statement_bind_float(statement, 62, from_in_to_mm(l1.monthET) / 100);
		/*************************************************************/
		if (l1.yearET == 65535)
			cass_statement_bind_null(statement, 63);
		else
			cass_statement_bind_float(statement, 63, from_in_to_mm(l1.yearET) / 100);
		/*************************************************************/
		val = from_forecast_to_diagnostic(l1.forecastIcons);
		if (val.empty())
			cass_statement_bind_null(statement, 64);
		else
			cass_statement_bind_string(statement, 64, val.c_str());
		/*************************************************************/
		cass_statement_bind_int32(statement, 65, l1.forecastIcons);
		/*************************************************************/
		cass_statement_bind_int64(statement, 66,
			from_hourmin_to_CassandraTime(
				l1.timeOfSunrise / 100,
				l1.timeOfSunrise % 100
			));
		/*************************************************************/
		cass_statement_bind_int64(statement, 67,
			from_hourmin_to_CassandraTime(
				l1.timeOfSunset / 100,
				l1.timeOfSunset % 100
			));
	}

};
