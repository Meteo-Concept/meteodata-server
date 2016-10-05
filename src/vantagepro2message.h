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
#include <array>

#include <boost/asio.hpp>

#include <cassandra.h>

#include "message.h"

using std::uint8_t;
using std::uint16_t;
using std::uint32_t;
using std::uint64_t;

namespace meteodata {

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
	std::array<asio::mutable_buffer, 2>& getBuffer() {
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

	virtual void populateDataPoint(const CassUuid station, CassStatement* const statement) const override;

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
		unsigned int dayStartDateCurrentStorm   : 5;
		/** The year the current storm started */
		unsigned int yearStartDateCurrentStorm  : 7;
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
		unsigned int dayStartDateCurrentStorm   : 5;
		/** The year the current storm started */
		unsigned int yearStartDateCurrentStorm  : 7;
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
		{ asio::buffer(&_l1, sizeof(Loop1)),
		  asio::buffer(&_l2, sizeof(Loop2)) }
	};
};

}

#endif /* VANTAGEPRO2MESSAGE_H */
