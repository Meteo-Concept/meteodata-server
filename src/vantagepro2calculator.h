/**
 * @file vantagepro2calculator.h
 * @brief Definition of the VantagePro2Calculator class
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

#ifndef VANTAGEPRO2CALCULATOR_H
#define VANTAGEPRO2CALCULATOR_H

#include <cstdint>
#include <ctime>
#include <cmath>
#include <array>
#include <chrono>

#include <date/date.h>
#include <cassandra.h>

using std::uint8_t;
using std::uint16_t;
using std::uint32_t;
using std::uint64_t;

namespace chrono = std::chrono;

namespace meteodata {

/**
 * @brief A Message able to receive and store one raw data point from a
 * VantagePro2 (R) station, by Davis Instruments (R)
 */
class VantagePro2Calculator
{
public:

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

private:
	/**
	 * @brief The constants necessary to compute the VantagePro2 CRCs
	 */
	static constexpr int CRC_VALUES[] =
	{
		0x0, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5, 0x60c6, 0x70e7,
		0x8108, 0x9129, 0xa14a, 0xb16b, 0xc18c, 0xd1ad, 0xe1ce, 0xf1ef,
		0x1231, 0x210, 0x3273, 0x2252, 0x52b5, 0x4294, 0x72f7, 0x62d6,
		0x9339, 0x8318, 0xb37b, 0xa35a, 0xd3bd, 0xc39c, 0xf3ff, 0xe3de,
		0x2462, 0x3443, 0x420, 0x1401, 0x64e6, 0x74c7, 0x44a4, 0x5485,
		0xa56a, 0xb54b, 0x8528, 0x9509, 0xe5ee, 0xf5cf, 0xc5ac, 0xd58d,
		0x3653, 0x2672, 0x1611, 0x630, 0x76d7, 0x66f6, 0x5695, 0x46b4,
		0xb75b, 0xa77a, 0x9719, 0x8738, 0xf7df, 0xe7fe, 0xd79d, 0xc7bc,
		0x48c4, 0x58e5, 0x6886, 0x78a7, 0x840, 0x1861, 0x2802, 0x3823,
		0xc9cc, 0xd9ed, 0xe98e, 0xf9af, 0x8948, 0x9969, 0xa90a, 0xb92b,
		0x5af5, 0x4ad4, 0x7ab7, 0x6a96, 0x1a71, 0xa50, 0x3a33, 0x2a12,
		0xdbfd, 0xcbdc, 0xfbbf, 0xeb9e, 0x9b79, 0x8b58, 0xbb3b, 0xab1a,
		0x6ca6, 0x7c87, 0x4ce4, 0x5cc5, 0x2c22, 0x3c03, 0xc60, 0x1c41,
		0xedae, 0xfd8f, 0xcdec, 0xddcd, 0xad2a, 0xbd0b, 0x8d68, 0x9d49,
		0x7e97, 0x6eb6, 0x5ed5, 0x4ef4, 0x3e13, 0x2e32, 0x1e51, 0xe70,
		0xff9f, 0xefbe, 0xdfdd, 0xcffc, 0xbf1b, 0xaf3a, 0x9f59, 0x8f78,
		0x9188, 0x81a9, 0xb1ca, 0xa1eb, 0xd10c, 0xc12d, 0xf14e, 0xe16f,
		0x1080, 0xa1, 0x30c2, 0x20e3, 0x5004, 0x4025, 0x7046, 0x6067,
		0x83b9, 0x9398, 0xa3fb, 0xb3da, 0xc33d, 0xd31c, 0xe37f, 0xf35e,
		0x2b1, 0x1290, 0x22f3, 0x32d2, 0x4235, 0x5214, 0x6277, 0x7256,
		0xb5ea, 0xa5cb, 0x95a8, 0x8589, 0xf56e, 0xe54f, 0xd52c, 0xc50d,
		0x34e2, 0x24c3, 0x14a0, 0x481, 0x7466, 0x6447, 0x5424, 0x4405,
		0xa7db, 0xb7fa, 0x8799, 0x97b8, 0xe75f, 0xf77e, 0xc71d, 0xd73c,
		0x26d3, 0x36f2, 0x691, 0x16b0, 0x6657, 0x7676, 0x4615, 0x5634,
		0xd94c, 0xc96d, 0xf90e, 0xe92f, 0x99c8, 0x89e9, 0xb98a, 0xa9ab,
		0x5844, 0x4865, 0x7806, 0x6827, 0x18c0, 0x8e1, 0x3882, 0x28a3,
		0xcb7d, 0xdb5c, 0xeb3f, 0xfb1e, 0x8bf9, 0x9bd8, 0xabbb, 0xbb9a,
		0x4a75, 0x5a54, 0x6a37, 0x7a16, 0xaf1, 0x1ad0, 0x2ab3, 0x3a92,
		0xfd2e, 0xed0f, 0xdd6c, 0xcd4d, 0xbdaa, 0xad8b, 0x9de8, 0x8dc9,
		0x7c26, 0x6c07, 0x5c64, 0x4c45, 0x3ca2, 0x2c83, 0x1ce0, 0xcc1,
		0xef1f, 0xff3e, 0xcf5d, 0xdf7c, 0xaf9b, 0xbfba, 0x8fd9, 0x9ff8,
		0x6e17, 0x7e36, 0x4e55, 0x5e74, 0x2e93, 0x3eb2, 0xed1, 0x1ef0
	};
};

}

namespace {

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
	date::sys_time<chrono::seconds> tp = date::sys_days(date::day(d)/m/y);
	return cass_date_from_epoch(tp.time_since_epoch().count());
}

/**
 * @brief Convert the date of today to a value that can be entered in a
 * Cassandra column of type "date"
 *
 * @return a value corresponding to the date given as parameter suitable for
 * insertion in a Cassandra database
 */
inline uint32_t today_to_CassandraDate()
{
	date::sys_time<chrono::seconds> tp = date::sys_days();
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
	float hi_farenheight =
		0.5 * (t_farenheight + 61.0 +
			(t_farenheight - 68.0) * 1.2 +
			hum * 0.094);

	if ((hi_farenheight + t_farenheight) / 2 > 80.0f) {
		hi_farenheight =
			-42.379 +
			2.04901523 * t_farenheight +
			10.14333127 * hum +
			-0.22475541 * t_farenheight * hum +
			-0.00683783 * std::pow(t_farenheight, 2) +
			-0.05481717 * std::pow(hum, 2) +
			0.00122874  * std::pow(t_farenheight, 2) * hum +
			0.00085282  * t_farenheight * std::pow(hum, 2) +
			-0.00000199 * std::pow(t_farenheight, 2) * std::pow(hum, 2);

		if (hum < 13 && hi_farenheight >= 80.0f && hi_farenheight <= 112.0f)
			hi_farenheight -=
				((13 - hum) / 4.0) *
					std::sqrt(17.0 - std::abs(t_farenheight - 95.0) / 17.0);
		else if (hum > 85 && hi_farenheight >= 80.0f && hi_farenheight <= 87.0f)
			hi_farenheight +=
				((hum - 85) / 10.0) *
					((87.0 - hi_farenheight) / 5.0);
	}
	return from_Farenheight_to_Celsius(hi_farenheight);
}

// Formula from Davis Instruments
inline float wind_chill(float t_farenheight, float wind_mph)
{
	float rc;
	if (wind_mph < 5.0 || t_farenheight >= 91.4)
		rc = t_farenheight;
	else
		rc = 35.74 + 0.6215 * t_farenheight
			 - 35.75 * std::pow(wind_mph, 0.16)
			 + 0.4275 * t_farenheight * std::pow(wind_mph, 0.16);

	return from_Farenheight_to_Celsius(std::min(rc, t_farenheight));
}

// Formula from Norms of apparent temperature in Australia, Aust. Met. Mag., 1994, Vol 43, 1-16 (see http://www.bom.gov.au/info/thermal_stress/#atapproximation))
inline float thsw_index(float t_celsius, int hum, float wind_ms, float solarRad)
{
	float waterVaporPressure = (hum / 100.0f) * 6.105
		* std::exp(17.27 * t_celsius / (237.7 + t_celsius));
	return t_celsius
	     + 0.348 * waterVaporPressure
	     - 0.70 * wind_ms
	     + 0.70 * solarRad / (wind_ms + 10.0)
	     - 4.25;
}

inline float thsw_index(float t_celsius, int hum, float wind_ms)
{
	float waterVaporPressure = (hum / 100.0f) * 6.105
		* std::exp(17.27 * t_celsius / (237.7 + t_celsius));
	return t_celsius
	     + 0.33 * waterVaporPressure
	     - 0.70 * wind_ms
	     - 4.0;
}

}
#endif /* VANTAGEPRO2CALCULATOR_H */
