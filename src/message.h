#ifndef MESSAGE_H
#define MESSAGE_H

#include <cstdint>
#include <iostream>
#include <vector>
#include <algorithm>
#include <cassandra.h>

using std::uint8_t;
using std::uint16_t;
using std::uint32_t;
using std::uint64_t;

namespace meteodata {

	struct Loop1
	{
		char header[3];
		uint8_t barTrend;
		uint8_t packetType;
		uint16_t nextRecord;
		uint16_t barometer;
		uint16_t insideTemperature;
		uint8_t insideHumidity;
		uint16_t outsideTemperature;
		uint8_t windSpeed;
		uint8_t tenMinAvgWindSpeed;
		uint16_t windDir;
		uint8_t extraTemp[7];
		uint8_t soilTemp[4];
		uint8_t leafTemp[4];
		uint8_t outsideHumidity;
		uint8_t extraHum[7];
		uint16_t rainRate;
		uint8_t uv;
		uint16_t solarRad;
		uint16_t stormRain;
		unsigned int monthStartDateCurrentStorm : 4;
		unsigned int dayStartDateCurrentStorm   : 5;
		unsigned int yearStartDateCurrentStorm  : 7;
		uint16_t dayRain;
		uint16_t monthRain;
		uint16_t yearRain;
		uint16_t dayET;
		uint16_t monthET;
		uint16_t yearET;
		uint8_t soilMoistures[4];
		uint8_t leafWetnesses[4];
		uint8_t insideAlarm;
		uint8_t rainAlarm;
		uint16_t outsideAlarms;
		uint64_t extraTempHumAlarms;
		uint32_t soilLeafAlarms;
		uint8_t transmitterBatteryStatus;
		uint16_t consoleBatteryVoltage;
		uint8_t forecastIcons;
		uint8_t forecastRuleNumber;
		uint16_t timeOfSunrise;
		uint16_t timeOfSunset;
		char lf;
		char cr;
		uint16_t crcLoop1;
	} __attribute__((packed));

	struct Loop2
	{
		char header[3];
		uint8_t barTrend;
		uint8_t packetType;
		unsigned int : 16;
		uint16_t barometer;
		uint16_t insideTemperature;
		uint8_t insideHumidity;
		uint16_t outsideTemperature;
		uint8_t windSpeed;
		unsigned int : 8;
		uint16_t windDir;
		uint16_t tenMinAvgWindSpeed;
		uint16_t twoMinAvgWindSpeed;
		uint16_t tenMinWindGust;
		uint16_t windGustDir;
		unsigned int : 16;
		unsigned int : 16;
		uint16_t dewPoint;
		unsigned int : 8;
		uint8_t outsideHumidity;
		unsigned int : 8;
		uint16_t heatIndex;
		uint16_t windChill;
		uint16_t thswIndex;
		uint16_t rainRate;
		uint8_t uv;
		uint16_t solarRad;
		uint16_t stormRain;
		unsigned int monthStartDateCurrentStorm : 4;
		unsigned int dayStartDateCurrentStorm   : 5;
		unsigned int yearStartDateCurrentStorm  : 7;
		uint16_t dayRain;
		uint16_t last15MinRain;
		uint16_t lastHourRain;
		uint16_t dayET;
		uint16_t last24HoursRain;
		uint8_t barReducMethod;
		uint16_t userBarOffset;
		uint16_t barCalibNumber;
		uint16_t barSensorRaw;
		uint16_t absBarPressure;
		uint16_t altimeterSetting;
		unsigned int : 8;
		unsigned int : 8;
		uint8_t next10MinWindSpeedGraphPtr;
		uint8_t next15MinWindSpeedGraphPtr;
		uint8_t nextHourWindSpeedGraphPtr;
		uint8_t nextDayWindSpeedGraphPtr;
		uint8_t nextMinRainGraphPtr;
		uint8_t nextRainStormGraphPtr;
		uint8_t minuteInHourForRainCalculation;
		uint8_t nextMonthRainGraphPtr;
		uint8_t nextYearRainGraphPtr;
		uint8_t nextSeasonRainGraphPtr;
		unsigned int : 16;
		unsigned int : 16;
		unsigned int : 16;
		unsigned int : 16;
		unsigned int : 16;
		unsigned int : 16;
		char lf;
		char cr;
		uint16_t crc;
	} __attribute__((packed));

	void populateDataPoint(const Loop1& l1, const Loop2& l2, CassStatement* const statement);
}

#endif
