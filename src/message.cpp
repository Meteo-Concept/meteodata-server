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
		return (it == forecasts.cend()) ? "Unknown" : it->second;
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
		return (it == bartrends.cend()) ? "Unknown" : it->second;
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
		memset(&date, 0, sizeof(date));
		date.tm_hour = hour;
		date.tm_min = min;
		return cass_time_from_epoch(mktime(&date));
	}

	inline int32_t from_inHg_to_bar(int bar)
	{
		return bar * 0.03386;
	}

	inline int32_t from_Farenheight_to_Celsius(int f)
	{
		return (f - 32.0) / 1.80;
	}

	inline int32_t from_mph_to_mps(int mph)
	{
		return mph * 0.44704;
	}

	inline int32_t from_mph_to_kph(int mph)
	{
		return mph * 1.609;
	}

	inline int32_t from_in_to_mm(int in)
	{
		return in * 25.4;
	}

	inline int32_t from_rainrate_to_mm(int rr)
	{
		//assume the raw value is in 0.2mm/hour
		return rr * 0.2;
	}
}

namespace meteodata
{
	DataPoint makeDataPoint(Loop1 l1, Loop2 l2)
	{
		DataPoint dp;
		dp.time = time(NULL);
		dp.bartrend = from_bartrend_to_diagnostic(l1.barTrend);
		dp.barometer = (l2.barometer >= 20 && l2.barometer <= 32.5) ?
				from_inHg_to_bar(1000 * l2.barometer) :
				255;
		dp.barometer_abs = from_inHg_to_bar(1000 * l2.absBarPressure);
		dp.barometer_raw = from_inHg_to_bar(1000 * l2.barSensorRaw);
		dp.insidetemp = from_Farenheight_to_Celsius(l1.insideTemperature);
		dp.outsidetemp = (l1.outsideTemperature == 255) ?
			255 :
			from_Farenheight_to_Celsius(l1.outsideTemperature);
		dp.insidehum = l1.insideHumidity;
		dp.outsidehum = l1.outsideHumidity;
		for (int i=0 ; i<7 ; i++) {
			dp.extratemp[i] = (l1.extraTemp[1] == 255) ?
				255 :
				from_Farenheight_to_Celsius(l1.extraTemp[i] - 90);
		}
		for (int i=0 ; i<4 ; i++) {
			dp.soiltemp[i] = (l1.soilTemp[i] == 255) ?
				255 :
				from_Farenheight_to_Celsius(l1.soilTemp[i] - 90);
			dp.leaftemp[i] = (l1.leafTemp[i] == 255) ?
				255 :
				from_Farenheight_to_Celsius(l1.leafTemp[i] - 90);
		}
		for (int i=0 ; i<7 ; i++)
			dp.extrahum[i] = l1.extraHum[i];
		for (int i=0 ; i<4 ; i++) {
			dp.soilmoistures[i] = l1.soilMoistures[i];
			dp.leafwetnesses[i] = (l1.leafWetnesses[i] >= 0 &&
					       l1.leafWetnesses[i] <= 15) ?
				l1.leafWetnesses[i] :
				255;
		}
		dp.windspeed = from_mph_to_kph(l1.windSpeed);
		dp.winddir = l1.windDir;
		dp.avgwindspeed_10min = (l2.tenMinAvgWindSpeed == 32767) ?
			32767 :
			from_mph_to_kph(l2.tenMinAvgWindSpeed * 10);
		dp.avgwindspeed_2min = (l2.twoMinAvgWindSpeed == 32767) ?
			32767 :
			from_mph_to_kph(l2.twoMinAvgWindSpeed * 10);
		dp.windgust_10min = (l2.tenMinWindGust == 32767) ?
			32767 :
			from_mph_to_kph(l2.tenMinWindGust * 10);
		dp.windgustdir = l2.windGustDir;
		dp.rainrate = from_rainrate_to_mm(l1.rainRate);
		dp.rain_15min = from_rainrate_to_mm(l2.last15MinRain);
		dp.rain_1h = from_rainrate_to_mm(l2.lastHourRain);
		dp.rain_24h = from_rainrate_to_mm(l2.lastHourRain);
		dp.dayrain = from_rainrate_to_mm(l1.dayRain);
		dp.monthrain = from_rainrate_to_mm(l1.monthRain);
		dp.yearrain = from_rainrate_to_mm(l1.yearRain);
		dp.stormrain = from_in_to_mm(l2.stormRain * 100);
		dp.stormstartdate =
			(l2.monthStartDateCurrentStorm >= 1 &&
			 l2.monthStartDateCurrentStorm <= 12 &&
			 l2.dayStartDateCurrentStorm >= 1 &&
			 l2.dayStartDateCurrentStorm <= 31) ?
			from_daymonthyear_to_CassandraDate(
					l2.dayStartDateCurrentStorm + 2000,
					l2.monthStartDateCurrentStorm,
					l2.yearStartDateCurrentStorm
				    ) :
			255;
		dp.UV = l2.uv;
		dp.solarrad = l2.solarRad;
		dp.dewpoint = (l2.dewPoint == 255) ?
			255 :
			from_Farenheight_to_Celsius(l2.dewPoint);
		dp.heatindex = (l2.heatIndex == 255) ?
			255 :
			from_Farenheight_to_Celsius(l2.heatIndex);
		dp.windchill = (l2.windChill == 255) ?
			255 :
			from_Farenheight_to_Celsius(l2.windChill);
		dp.thswindex = (l2.thswIndex == 255) ?
			255 :
			from_Farenheight_to_Celsius(l2.thswIndex);
		dp.dayET = from_in_to_mm(l1.dayET * 1000);
		dp.monthET = from_in_to_mm(l1.monthET * 100);
		dp.yearET = from_in_to_mm(l1.yearET * 100);
		dp.forecast = from_forecast_to_diagnostic(l1.forecastIcons);
		dp.forecast_icons = l1.forecastIcons;
		dp.sunrise = from_hourmin_to_CassandraTime(
				l1.timeOfSunrise / 100,
				l1.timeOfSunrise % 100
			     );
		dp.sunset = from_hourmin_to_CassandraTime(
				l1.timeOfSunset / 100,
				l1.timeOfSunset % 100
			    );

		return dp;
	}

};
