/**
 * @file vantagepro2message.h
 * @brief Definition of the VantagePro2Message class
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

#ifndef VANTAGEPRO2MESSAGE_H
#define VANTAGEPRO2MESSAGE_H

#include <cstdint>
#include <ctime>
#include <cmath>
#include <array>
#include <chrono>

#include <boost/asio.hpp>

#include <date.h>
#include <cassandra.h>
#include <message.h>

using std::uint8_t;
using std::uint16_t;
using std::uint32_t;
using std::uint64_t;

namespace chrono = std::chrono;

namespace meteodata
{

namespace asio = boost::asio;

/**
 * @brief A Message able to receive and store one raw data point from a
 * VantagePro2 (R) station, by Davis Instruments (R)
 */
class VantagePro2Message : public Message
{
public:
	/**
	 * @brief Get a reference to the Boost::Asio buffer to receive the raw
	 * data from  the station
	 *
	 * @return a reference to the buffer in which the VantagePro2Message can
	 * store data from the station
	 */
	std::array<asio::mutable_buffer, 2>& getBuffer()
	{
		return _messageBuffer;
	}

	/**
	 * @brief Check the integrity of the received data by computing its CRC
	 * @see validateCRC
	 *
	 * @return true if, and only if, the data has been correclty received
	 */
	bool isValid() const;

	/**
	 * @brief Check the VantagePro2 CRC of any sequence of bytes
	 *
	 * This method assumes the CRC is contained in the last two bytes of the
	 * sequence.
	 * The computation of the CRC is done as described in the documentation
	 * by Davis Instruments.
	 *
	 * @param msg the sequence of bytes (presumably received from the
	 * station) whose last two bytes is a CRC to verify
	 * @param len the length of the sequence
	 *
	 * @return true if, and only if, the sequence is valid according to its
	 * CRC
	 */
	static bool validateCRC(const void* msg, size_t len);

	/**
	 * @brief Compute the VantagePro2 CRC of any sequence of bytes
	 *
	 * This method assumes the CRC is to be written in the last two bytes
	 * of the sequence.
	 * The computation of the CRC is done as described in the documentation
	 * by Davis Instruments.
	 *
	 * @param msg the sequence of bytes (presumably received from the
	 * station) whose last two bytes is where the CRC should be written
	 * @param len the length of the sequence
	 */
	static void computeCRC(void* msg, size_t len);

	static std::string from_bartrend_to_diagnostic(uint8_t value);
	static std::string from_forecast_to_diagnostic(uint8_t value);

	void populateDataPoint(const CassUuid station, CassStatement* const statement) const override;
	void populateV2DataPoint(const CassUuid station, CassStatement* const statement) const override;

private:
	/**
	 * @brief A Loop1 message, used by VantagePro2 (R) stations, and
	 * documented by Davis Instruments (R)
	 */
	struct Loop1
	{
		char header[3]; /**< Identifies the Loop packets: LOO */
		uint8_t barTrend; /**< The barometric trend */
		uint8_t packetType; /**< The packet type: 0 for Loop 1 */
		uint16_t nextRecord; /**< The next record position in the
				       archive, to monitor the creation of new
				       measure points */
		uint16_t barometer; /**< The barometric value */
		uint16_t insideTemperature; /**< The temperature inside */
		uint8_t insideHumidity; /**< The humidity inside */
		uint16_t outsideTemperature; /**< The temperature outside */
		uint8_t windSpeed; /**< The current wind speed */
		uint8_t tenMinAvgWindSpeed; /**< The wind speed averaged over the
					      last ten minutes */
		uint16_t windDir; /**< The wind direction */
		uint8_t extraTemp[7]; /**< The temperature measured by extra
					sensors */
		uint8_t soilTemp[4]; /**< The soil temperature measured by extra
				       sensors */
		uint8_t leafTemp[4]; /**< The leaf temperatur measured by extra
				       sensors */
		uint8_t outsideHumidity; /**< The outside humidity */
		uint8_t extraHum[7]; /**< The humidity measured by extra
				       sensors */
		uint16_t rainRate; /**< The current rain rate */
		uint8_t uv; /**< The UV index */
		uint16_t solarRad; /**< The solar radiation */
		uint16_t stormRain; /**< The storm rain volume */
		/** The month the current storm started */
		unsigned int monthStartDateCurrentStorm : 4;
		/** The day the current storm started */
		unsigned int dayStartDateCurrentStorm : 5;
		/** The year the current storm started */
		unsigned int yearStartDateCurrentStorm : 7;
		uint16_t dayRain; /**< Today's rain volume */
		uint16_t monthRain; /**< The current month's rain volume */
		uint16_t yearRain; /**< The current year's rain volume */
		uint16_t dayET; /**< Today's evapotranspiration */
		uint16_t monthET; /**< The current month's evapotranspiration */
		uint16_t yearET; /**< The current year's evapotranspiration */
		uint8_t soilMoistures[4]; /**< The soil moisture measured by
					    extra sensors */
		uint8_t leafWetnesses[4]; /**< The leaf wetnesses measured by
					    extra sensors */
		uint8_t insideAlarm; /**< The active alarms for the "inside"
				       value */
		uint8_t rainAlarm; /**< The active alarms on rain values */
		uint16_t outsideAlarms; /**< The active alarms for the
					 "outside" values */
		uint64_t extraTempHumAlarms; /**< The active alarms on extra
					       temerature and humidity values */
		uint32_t soilLeafAlarms; /**< The active alarms on the soil
					   moisture and leaf wetness values */
		uint8_t transmitterBatteryStatus; /**< The meteo station radio
						    battery status */
		uint16_t consoleBatteryVoltage; /**< The meteo station's main
						  battery status */
		uint8_t forecastIcons; /**< The forecast icons displayed */
		uint8_t forecastRuleNumber; /**< The forecast */
		uint16_t timeOfSunrise; /**< The hour at which the sun rises
					  today */
		uint16_t timeOfSunset; /**< The hour at which the sun sets
					 today */
		char lf; /**< A linefeed (0x0a, \\n) character */
		char cr; /**< A carriage return (0x0d, \\r) character */
		uint16_t crcLoop1; /**< The CRC value of the message to
				     validate the transmission */
	} __attribute__((packed));

	/**
	 * @brief A Loop2 message, used by VantagePro2 (R) stations, and
	 * documented by Davis Instruments (R)
	 */
	struct Loop2
	{
		char header[3]; /**< Identifies the Loop packets: LOO */
		uint8_t barTrend; /**< The barometric trend (an index in a table
				       of trend descritions) */
		uint8_t packetType; /**< The packet type: 1 for Loop2 */
		unsigned int : 16;
		uint16_t barometer; /**< The barometric value */
		uint16_t insideTemperature; /**< The inside temperature */
		uint8_t insideHumidity; /**< The inside humidity percentage */
		uint16_t outsideTemperature; /**< The outside temperature */
		uint8_t windSpeed; /**< The current wind speed */
		unsigned int : 8;
		uint16_t windDir; /**< The angular direction of the wind */
		uint16_t tenMinAvgWindSpeed; /**< The wind speed, averaged over
					          the last ten minutes */
		uint16_t twoMinAvgWindSpeed; /**< The wind speed, averaged over
					          the last two minutes */
		uint16_t tenMinWindGust; /**< The wind gust speed, averaged over
					      the last ten minutes */
		uint16_t windGustDir; /**< The angular direction of the wind
					gust */
		unsigned int : 16;
		unsigned int : 16;
		uint16_t dewPoint; /**< The dew point */
		unsigned int : 8;
		uint8_t outsideHumidity; /**< The outside humidity percentage */
		unsigned int : 8;
		uint16_t heatIndex; /**< The heat index */
		uint16_t windChill; /**< The wind chill */
		uint16_t thswIndex; /**< The THSW index */
		uint16_t rainRate; /**< The icurrent rain rate */
		uint8_t uv; /**< The UV index */
		uint16_t solarRad; /**< The solar radiation */
		uint16_t stormRain; /**< The storm rain volume */
		/** The month the current storm started */
		unsigned int monthStartDateCurrentStorm : 4;
		/** The day the current storm started */
		unsigned int dayStartDateCurrentStorm : 5;
		/** The year the current storm started */
		unsigned int yearStartDateCurrentStorm : 7;
		uint16_t dayRain; /**< Today's amount of rain */
		uint16_t last15MinRain; /**< The amount of rain over the last
					     fifteen minutes */
		uint16_t lastHourRain; /**< The amount of rain in the last
					    hour */
		uint16_t dayET; /** Today's evapotranspiration */
		uint16_t last24HoursRain; /**< The amount of rain over the last
					       twenty-four hour */
		uint8_t barReducMethod; /**< The barometric reduction method */
		uint16_t userBarOffset; /**< The barometric manually specified
					     offset */
		uint16_t barCalibNumber; /**< The barometric calibration */
		uint16_t barSensorRaw; /**< The raw reading from the barometer,
					    before reduction */
		uint16_t absBarPressure; /**< The absolute barometric
					      pressure */
		uint16_t altimeterSetting; /**< The manual setting of the
					        altimeter */
		unsigned int : 8;
		unsigned int : 8;
		/** The abscissa in the graph of the 10-minutes wind speed at
		 * which the next point will be placed */
		uint8_t next10MinWindSpeedGraphPtr;
		/** The abscissa in the graph of the 15-minutes wind speed at
		 * which the next point will be placed */
		uint8_t next15MinWindSpeedGraphPtr;
		/** The abscissa in the graph of the last hour wind speed at
		 * which the next point will be placed */
		uint8_t nextHourWindSpeedGraphPtr;
		/** The abscissa in the graph of the last day wind speed at
		 * which the next point will be placed */
		uint8_t nextDayWindSpeedGraphPtr;
		/** The abscissa in the graph of the last minute rain amount at
		 * which the next point will be placed */
		uint8_t nextMinRainGraphPtr;
		/** The abscissa in the graph of the rain storm at which the
		 * next point will be placed */
		uint8_t nextRainStormGraphPtr;
		/** The "MM" such that the rain amount per hour is computed
		 * between XhMM and (x+1)hMM for all x (manual setting) */
		uint8_t minuteInHourForRainCalculation;
		/** The abscissa in the graph of the amount of rain in the
		 * month at which the next point will be placed */
		uint8_t nextMonthRainGraphPtr;
		/** The abscissa in the graph of the amount of rain in the
		 * year at which the next point will be placed */
		uint8_t nextYearRainGraphPtr;
		/** The abscissa in the graph of the seasonal rain at which the
		 * next point will be placed */
		uint8_t nextSeasonRainGraphPtr;
		unsigned int : 16;
		unsigned int : 16;
		unsigned int : 16;
		unsigned int : 16;
		unsigned int : 16;
		unsigned int : 16;
		char lf; /**< A linefeed (0x0a, \\n) character */
		char cr; /**< A carriage return (0x0d, \\r) character */
		uint16_t crcLoop2; /**< The CRC value of the message to
				     validate the transmission */
	} __attribute__((packed));

	/**
	 * @brief The constants necessary to compute the VantagePro2 CRCs
	 */
	static constexpr int CRC_VALUES[] = {0x0, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5, 0x60c6, 0x70e7, 0x8108, 0x9129,
										 0xa14a, 0xb16b, 0xc18c, 0xd1ad, 0xe1ce, 0xf1ef, 0x1231, 0x210, 0x3273, 0x2252,
										 0x52b5, 0x4294, 0x72f7, 0x62d6, 0x9339, 0x8318, 0xb37b, 0xa35a, 0xd3bd, 0xc39c,
										 0xf3ff, 0xe3de, 0x2462, 0x3443, 0x420, 0x1401, 0x64e6, 0x74c7, 0x44a4, 0x5485,
										 0xa56a, 0xb54b, 0x8528, 0x9509, 0xe5ee, 0xf5cf, 0xc5ac, 0xd58d, 0x3653, 0x2672,
										 0x1611, 0x630, 0x76d7, 0x66f6, 0x5695, 0x46b4, 0xb75b, 0xa77a, 0x9719, 0x8738,
										 0xf7df, 0xe7fe, 0xd79d, 0xc7bc, 0x48c4, 0x58e5, 0x6886, 0x78a7, 0x840, 0x1861,
										 0x2802, 0x3823, 0xc9cc, 0xd9ed, 0xe98e, 0xf9af, 0x8948, 0x9969, 0xa90a, 0xb92b,
										 0x5af5, 0x4ad4, 0x7ab7, 0x6a96, 0x1a71, 0xa50, 0x3a33, 0x2a12, 0xdbfd, 0xcbdc,
										 0xfbbf, 0xeb9e, 0x9b79, 0x8b58, 0xbb3b, 0xab1a, 0x6ca6, 0x7c87, 0x4ce4, 0x5cc5,
										 0x2c22, 0x3c03, 0xc60, 0x1c41, 0xedae, 0xfd8f, 0xcdec, 0xddcd, 0xad2a, 0xbd0b,
										 0x8d68, 0x9d49, 0x7e97, 0x6eb6, 0x5ed5, 0x4ef4, 0x3e13, 0x2e32, 0x1e51, 0xe70,
										 0xff9f, 0xefbe, 0xdfdd, 0xcffc, 0xbf1b, 0xaf3a, 0x9f59, 0x8f78, 0x9188, 0x81a9,
										 0xb1ca, 0xa1eb, 0xd10c, 0xc12d, 0xf14e, 0xe16f, 0x1080, 0xa1, 0x30c2, 0x20e3,
										 0x5004, 0x4025, 0x7046, 0x6067, 0x83b9, 0x9398, 0xa3fb, 0xb3da, 0xc33d, 0xd31c,
										 0xe37f, 0xf35e, 0x2b1, 0x1290, 0x22f3, 0x32d2, 0x4235, 0x5214, 0x6277, 0x7256,
										 0xb5ea, 0xa5cb, 0x95a8, 0x8589, 0xf56e, 0xe54f, 0xd52c, 0xc50d, 0x34e2, 0x24c3,
										 0x14a0, 0x481, 0x7466, 0x6447, 0x5424, 0x4405, 0xa7db, 0xb7fa, 0x8799, 0x97b8,
										 0xe75f, 0xf77e, 0xc71d, 0xd73c, 0x26d3, 0x36f2, 0x691, 0x16b0, 0x6657, 0x7676,
										 0x4615, 0x5634, 0xd94c, 0xc96d, 0xf90e, 0xe92f, 0x99c8, 0x89e9, 0xb98a, 0xa9ab,
										 0x5844, 0x4865, 0x7806, 0x6827, 0x18c0, 0x8e1, 0x3882, 0x28a3, 0xcb7d, 0xdb5c,
										 0xeb3f, 0xfb1e, 0x8bf9, 0x9bd8, 0xabbb, 0xbb9a, 0x4a75, 0x5a54, 0x6a37, 0x7a16,
										 0xaf1, 0x1ad0, 0x2ab3, 0x3a92, 0xfd2e, 0xed0f, 0xdd6c, 0xcd4d, 0xbdaa, 0xad8b,
										 0x9de8, 0x8dc9, 0x7c26, 0x6c07, 0x5c64, 0x4c45, 0x3ca2, 0x2c83, 0x1ce0, 0xcc1,
										 0xef1f, 0xff3e, 0xcf5d, 0xdf7c, 0xaf9b, 0xbfba, 0x8fd9, 0x9ff8, 0x6e17, 0x7e36,
										 0x4e55, 0x5e74, 0x2e93, 0x3eb2, 0xed1, 0x1ef0};

	/**
	 * @brief The first half of the data point
	 */
	Loop1 _l1;
	/**
	 * @brief The second half of the data point
	 */
	Loop2 _l2;
	/**
	 * @brief The Boost::Asio buffer in which the raw data point from the
	 * station is to be received
	 */
	std::array<asio::mutable_buffer, 2> _messageBuffer = {
			{asio::buffer(&_l1, sizeof(Loop1)), asio::buffer(&_l2, sizeof(Loop2))}};
};

}

namespace
{

/**
 * @brief Convert a date to a value that can be entered in a Cassandra column
 * of type "date"
 *
 * @param d the day
 * @param m the month
 * @param y the year
 *
 * @return a value corresponding to the date given as parameter suitable for
 * insertion in a Cassandra database
 */
inline uint32_t from_daymonthyear_to_CassandraDate(int d, int m, int y)
{
	date::sys_time<chrono::seconds> tp = date::sys_days(date::day(d) / m / y);
	return cass_date_from_epoch(tp.time_since_epoch().count());
}

/** @brief Convert an hour and minute value to a value that can be entered in a
 * Cassandra column of type "time"
 *
 * @param h the hour
 * @param m the minutes
 *
 * @return a value corresponding to the time given as parameter suitable for
 * insertion in a Cassandra database
 */
inline int64_t from_hourmin_to_CassandraTime(int h, int m)
{
	date::sys_time<chrono::seconds> tp = date::sys_days() + chrono::hours(h) + chrono::minutes(m);
	return cass_time_from_epoch(tp.time_since_epoch().count());
}

/**
 * @brief Convert a pression given in inches of mercury to bar (or, perhaps
 * more commonly, milli-inches of mercury to millibars)
 *
 * @param inHg the value to convert
 *
 * @return the parameter value converted to bar
 */
template<typename T>
inline float from_inHg_to_bar(T inHg)
{
	return static_cast<float>(inHg * 0.03386);
}

/**
 * @brief Convert a temperature given in Farenheit degrees to Celsius degrees
 *
 * @param f the value to convert
 *
 * @return the parameter value converted to Celsius degrees
 */
inline float from_Farenheit_to_Celsius(float f)
{
	return (f - 32.0) / 1.80;
}

/**
 * @brief Convert a temperature given in Kelvin to Celsius degrees
 *
 * @param f the value to convert
 *
 * @return the parameter value converted to Celsius degrees
 */
inline float from_Kelvin_to_Celsius(float k)
{
	return k - 273.15;
}

/**
 * @brief Convert a temperature given in Celsius degrees to Farenheit degrees
 *
 * @param f the value to convert
 *
 * @return the parameter value converted to Farenheit degrees
 */
inline float from_Celsius_to_Farenheit(float c)
{
	return (c * 1.80) + 32.0;
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
 * @brief Convert a velocity from kilometers per hour to meters per second
 *
 * @param kph the value to convert. Contrarily to other similar functions, this
 * one takes a float.
 *
 * @return the parameter value converted to meters per second
 */
inline float from_kph_to_mps(float kph)
{
	return kph / 3.6;
}

/**
 * @brief Convert a velocity from meters per second to kilometers per hour
 *
 * @param mps the value to convert
 *
 * @return the parameter value converted to kilometers per hour
 */
inline float from_mps_to_kph(int mps)
{
	return mps * 3.6;
}

/**
 * @brief Convert a distance from inches to millimeters,
 * when the the value in inches is given as an integer
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
 * @brief Convert a distance from inches to millimeters,
 * when the the value in inches is given as a float
 *
 * @param in the value to convert
 *
 * @return the parameter value converted to millimeters
 */
inline float from_in_to_mm(float in)
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


// Formula of Magnus-Tetens
inline float dew_point(float t_celsius, int hum)
{
	float rh = hum / 100.0f - 1;
	float alpha = (17.27 * t_celsius) / (237.7 + t_celsius) + std::log1p(rh);
	float tr = (237.7 * alpha) / (17.27 - alpha);
	return tr;
}

// Formula of NWS (See http://www.wpc.ncep.noaa.gov/html/heatindex_equation.shtml)
inline float heat_index(float t_farenheight, int hum)
{
	float hi_farenheight = 0.5 * (t_farenheight + 61.0 + (t_farenheight - 68.0) * 1.2 + hum * 0.094);

	if ((hi_farenheight + t_farenheight) / 2 > 80.0f) {
		hi_farenheight = -42.379 + 2.04901523 * t_farenheight + 10.14333127 * hum + -0.22475541 * t_farenheight * hum +
						 -0.00683783 * std::pow(t_farenheight, 2) + -0.05481717 * std::pow(hum, 2) +
						 0.00122874 * std::pow(t_farenheight, 2) * hum + 0.00085282 * t_farenheight * std::pow(hum, 2) +
						 -0.00000199 * std::pow(t_farenheight, 2) * std::pow(hum, 2);

		if (hum < 13 && hi_farenheight >= 80.0f && hi_farenheight <= 112.0f)
			hi_farenheight -= ((13 - hum) / 4.0) * std::sqrt(17.0 - std::abs(t_farenheight - 95.0) / 17.0);
		else if (hum > 85 && hi_farenheight >= 80.0f && hi_farenheight <= 87.0f)
			hi_farenheight += ((hum - 85) / 10.0) * ((87.0 - hi_farenheight) / 5.0);
	}
	return from_Farenheit_to_Celsius(hi_farenheight);
}

// Formula from Davis Instruments
inline float wind_chill(float t_farenheight, float wind_mph)
{
	float rc;
	if (wind_mph < 5.0 || t_farenheight >= 91.4)
		rc = t_farenheight;
	else
		rc = 35.74 + 0.6215 * t_farenheight - 35.75 * std::pow(wind_mph, 0.16) +
			 0.4275 * t_farenheight * std::pow(wind_mph, 0.16);

	return from_Farenheit_to_Celsius(std::min(rc, t_farenheight));
}

// Formula from Norms of apparent temperature in Australia, Aust. Met. Mag., 1994, Vol 43, 1-16 (see http://www.bom.gov.au/info/thermal_stress/#atapproximation))
inline float thsw_index(float t_celsius, int hum, float wind_ms, float netRad)
{
	float waterVaporPressure = (hum / 100.0f) * 6.105 * std::exp(17.27 * t_celsius / (237.7 + t_celsius));
	return t_celsius + 0.348 * waterVaporPressure - 0.70 * wind_ms + 0.70 * netRad / (wind_ms + 10.0) - 4.25;
}

inline float thsw_index(float t_celsius, int hum, float wind_ms)
{
	float waterVaporPressure = (hum / 100.0f) * 6.105 * std::exp(17.27 * t_celsius / (237.7 + t_celsius));
	return t_celsius + 0.33 * waterVaporPressure - 0.70 * wind_ms - 4.0;
}

inline bool insolated(float solarRad, float latitude, float longitude, time_t timestamp)
{
	using namespace date;
	using namespace chrono;

	constexpr double PI = 3.14159265358979;
	double raddeg = PI / 180.;
	latitude *= raddeg;
	longitude *= raddeg;

	auto time = sys_seconds{seconds{timestamp}};
	double fDays = duration<double, days::period>(time - sys_days{year{2000} / 1 / 1} - 12h).count();
	int secondsSinceMidnight = floor<seconds>(time - floor<days>(time)).count();

	// Mean longitude of the sun
	double l = (280.46646 + 0.98564736 * fDays) * raddeg;
	// Mean anomaly of the sun
	double m = (357.52911 + 0.985600281 * fDays) * raddeg;
	// Difference between the mean and true longitude of the sun
	double c =
			((1.914602 - 0.00000013188 * fDays) * std::sin(m) + (0.019993 - 0.000000002765 * fDays) * std::sin(2 * m)) *
			raddeg;
	// Obliquity of the Earth (approximation correct for the next century)
	double epsilon = 23.43929 * raddeg;
	// Sine of tqe solar declination angle
	double sin_delta = std::sin(l + c) * std::sin(epsilon);

	// y -- no particular meaning but facilitates the expression of the equation of time
	double y = std::pow(std::tan(epsilon / 2.), 2.);
	// Excentricity of the Earth's orbit
	double e = 0.016708634 - 0.0000000011509 * fDays;
	// Equation of the time -- angular difference between apparent solar time and mean time
	double eq = y * std::sin(2 * l) - 2 * e * std::sin(m) + 4 * e * y * std::sin(m) * std::cos(2 * l);

	// True solar time of the UTC timestamp in parameter
	// pi / (12. * 3600.) is the coefficient to convert a timestamp in seconds to a value in radians
	double h = secondsSinceMidnight * PI / (12. * 3600.) + eq + longitude;
	// Sine of the solar altitude
	double sin_alpha =
			std::cos(PI - h) * std::cos(latitude) * std::cos(std::asin(sin_delta)) + std::sin(latitude) * sin_delta;

	if (sin_alpha >= -1. && sin_alpha <= 1.) {
		double alpha = std::asin(sin_alpha);
		if (alpha < 3. * raddeg)
			return false;

		double threshold = (0.73 + 0.06 * std::cos(2 * PI * fDays / 365)) * 1080 * std::pow(sin_alpha, 1.25);
		return solarRad > threshold;
	}
	return false;
}

// Formula of Penman-Monteith, from the methodology by the FAO http://www.fao.org/3/X0490E/x0490e04.htm
inline float
evapotranspiration(float t_celsius, int hum, float wind_ms, float solar_radiation, float latitude, float longitude,
				   int elevation, time_t timestamp, int polling_period)
{
	using namespace date;
	using namespace chrono;

	auto time = sys_seconds{seconds{timestamp}};
	constexpr double PI = 3.14159265358979;
	double raddeg = PI / 180.;
	latitude *= raddeg;
	longitude *= raddeg;


	// Slope of the saturation pressure curve
	double Delta =
			4098 * (0.6108 * std::exp((17.27 * t_celsius) / (t_celsius + 237.3))) / std::pow(t_celsius + 237.3, 2.);
	// Average atmospheric pressure at the altitude of the station
	double P = 101.3 * std::pow((293. - 0.0065 * elevation) / 293., 5.26);
	// Psychometric constant
	double gamma = 6.65e-4 * P;
	// Saturation vapour pressure
	double e_s = 0.6108 * std::exp((17.27 * t_celsius) / (t_celsius + 237.3));
	// Vapour pressure
	double e_a = e_s * hum / 100;

	// day of the year
	double J = duration<double, days::period>(floor<days>(time) - floor<years>(time)).count();
	// Inverse relative distance Earth-Sun
	double d_r = 1 + 0.033 * cos(2 * PI * J / 365);
	// Solar declination
	double delta = 0.409 * sin(2 * PI * J / 365 - 1.39);
	// Equation of time
	double b = 2 * PI * (J - 81) / 364;
	double S_c = 0.1645 * sin(2 * b) - 0.1255 * cos(b) - 0.025 * sin(b);
	// Fractional hours since midnight (UTC)
	double t = duration<double, hours::period>(time - floor<days>(time)).count();
	// sunset hour angle
	//double omega_s = std::acos(-std::tan(latitude) * std::tan(delta));
	// true solar angle at half the polling period
	double omega = (t - (polling_period / 120) + S_c) * PI / 12 - longitude - PI;
	double omega_2 = omega + (PI / 12) * (polling_period / 120.);
	double omega_1 = omega - (PI / 12) * (polling_period / 120.);
	//bool daytime = omega > - omega_s && omega < omega_s;

	// Extraterrestrial radiation
	double G_sc = 0.0820; // solar constant
	double R_a = (12 / PI) * G_sc * d_r * ((omega_2 - omega_1) * std::sin(latitude) * std::sin(delta) +
										   std::cos(latitude) * std::cos(delta) *
										   (std::sin(omega_2) - std::sin(omega_1)));
	if (R_a < 0)
		R_a = 0;

	// solar radiation over the measurement period
	double R = solar_radiation * 60e-6; // conversion from W.m^2 to MJ.m^2.min^-1
	// Clear-sky solar radiation (Angstrom formula with no calibrated parameters)
	double R_so = (0.75 + 2e-5 * elevation) * R_a;
	// Net shortwave radiation
	double a = 0.23; // standard albedo for the grass
	double R_ns = (1 - a) * R;
	// Stefan-Boltzmann constant, by min
	double sigma = 4.903e-9 / (24 * 60);
	// Net longwave radiation
	double ratio = R_so == 0 ? 0.6 : R > R_so ? 1 : R / R_so;
	double R_nl = sigma * std::pow(t_celsius + 273.16, 4.) * (0.34 - 0.14 * std::sqrt(e_a)) * (1.35 * ratio - 0.35);
	// Net radiation
	double R_n = R_ns - R_nl;

	// Soil heat flux, seems to be ignored by Vantage stations
	double G = 0.; //R_n * (daytime ? 0.1 : 0.5);

	// Evapotranspiration
	double ET_0 = (0.408 * Delta * (R_n - G) + gamma * (37 / (t_celsius + 273.16)) * wind_ms * (e_s - e_a)) /
				  (Delta + gamma * (1 + 0.34 * wind_ms));
	if (ET_0 < 0)
		ET_0 = 0;

	return ET_0;
}

}

#endif /* VANTAGEPRO2MESSAGE_H */
