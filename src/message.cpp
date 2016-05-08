#include <ctime>
#include <cstring>
#include <map>

#include "message.h"
#include "dbconnection.h"

namespace {
	const std::map<int32_t, std::string> forecasts = {
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
}

namespace meteodata
{
	DataPoint makeDataPoint(Loop1 l1, Loop2 l2)
	{
		DataPoint dp;
		dp.time = time(NULL);
		dp.bartrend = l1.barTrend;
		dp.barometer = l2.barometer;
		dp.barometer_abs = l2.absBarPressure;
		dp.barometer_raw = l2.barSensorRaw;
		dp.insidetemp = l1.insideTemperature;
		dp.outsidetemp = l1.outsideTemperature;
		dp.insidehum = l1.insideHumidity;
		dp.outsidehum = l1.outsideHumidity;
		for (int i=0 ; i<7 ; i++)
			dp.extratemp[i] = l1.extraTemp[i];
		for (int i=0 ; i<4 ; i++) {
			dp.soiltemp[i] = l1.soilTemp[i];
			dp.leaftemp[i] = l1.leafTemp[i];
		}
		for (int i=0 ; i<7 ; i++)
			dp.extrahum[i] = l1.extraHum[i];
		for (int i=0 ; i<4 ; i++) {
			dp.soilmoistures[i] = l1.soilMoistures[i];
			dp.leafwetnesses[i] = l1.leafWetnesses[i];
		}
		dp.windspeed = l1.windSpeed;
		dp.winddir = l1.windDir;
		dp.avgwindspeed_10min = l1.tenMinAvgWindSpeed;
		dp.avgwindspeed_2min = l2.twoMinAvgWindSpeed;
		dp.windgust_10min = l2.tenMinWindGust;
		dp.windgustdir = l2.windGustDir;
		dp.rainrate = l1.rainRate;
		dp.rain_15min = l2.last15MinRain;
		dp.rain_1h = l2.lastHourRain;
		dp.rain_24h = l2.lastHourRain;
		dp.dayrain = l1.dayRain;
		dp.monthrain = l1.monthRain;
		dp.yearrain = l1.yearRain;
		dp.stormrain = l2.stormRain;
		dp.stormstartdate = from_daymonthyear_to_CassandraDate(
					l2.dayStartDateCurrentStorm,
					l2.monthStartDateCurrentStorm,
					l2.yearStartDateCurrentStorm
				    );
		dp.UV = l2.uv;
		dp.solarrad = l2.solarRad;
		dp.dewpoint = l2.dewPoint;
		dp.heatindex = l2.heatIndex;
		dp.windchill = l2.windChill;
		dp.thswindex = l2.thswIndex;
		dp.dayET = l1.dayET;
		dp.monthET = l1.monthET;
		dp.yearET = l1.yearET;
		dp.forecast = forecasts.at(l1.forecastIcons);
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
