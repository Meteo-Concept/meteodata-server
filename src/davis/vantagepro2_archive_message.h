/**
 * @file vantagepro2archivemessage.h
 * @brief Definition of the VantagePro2ArchiveMessage class
 * @author Laurent Georget
 * @date 2017-10-11
 */
/*
 * Copyright (C) 2017  SAS Météo Concept <contact@meteo-concept.fr>
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

#ifndef VANTAGEPRO2ARCHIVEMESSAGE_H
#define VANTAGEPRO2ARCHIVEMESSAGE_H

#include <cstdint>
#include <array>
#include <chrono>

#include <boost/asio.hpp>

#include <cassandra.h>
#include <message.h>

#include "../time_offseter.h"

using std::uint8_t;
using std::uint16_t;
using std::uint32_t;
using std::uint64_t;

namespace meteodata {

namespace asio = boost::asio;
namespace chrono = std::chrono;

/**
 * @brief A Message able to receive and store one raw data point from the
 * archive of a VantagePro2 (R) station, by Davis Instruments (R)
 */
class VantagePro2ArchiveMessage : public Message
{
public:
	/**
	 * @brief An archive data point, used by VantagePro2 (R) stations, and
	 * documented by Davis Instruments (R)
	 */
	struct ArchiveDataPoint
	{
		unsigned int day   : 5;     /*!< The day this entry has been written                            */
		unsigned int month : 4;     /*!< The month this entry has been written                          */
		unsigned int year  : 7;     /*!< The year this entry has been written                           */
		uint16_t time;              /*!< The hour, minutes, and seconds this entry has been written     */
		uint16_t outsideTemp;       /*!< The average outside temperature over the duration of the entry */
		uint16_t maxOutsideTemp;    /*!< The maximal outside temperature over the duration of the entry */
		uint16_t minOutsideTemp;    /*!< The minimal outside temperature over the duration of the entry */
		uint16_t rainfall;          /*!< The quantity of rain over the duration of the entry            */
		uint16_t maxRainRate;       /*!< The maximal rain rate over the duration of the entry           */
		uint16_t barometer;         /*!< The average barometric pressure over the duration of the entry */
		uint16_t solarRad;          /*!< The average solar radiation over the duration of the entry     */
		/**
		 * @brief The number of wind samples collected from the sensors over the duration
		 * of the archive
		 *
		 * This value can be used to estimate the quality of the link between the sensors
		 * and the station.
                 */
		uint16_t nbWindSamples;
		uint16_t insideTemp;        /*!< The average inside temperature over the duration of the entry */
		uint8_t  insideHum;         /*!< The average inside humidity over the duration of the entry    */
		uint8_t  outsideHum;        /*!< The average outside humidity over the duration of the entry   */
		uint8_t  avgWindSpeed;      /*!< The average wind speed over the duration of the entry         */
		uint8_t  maxWindSpeed;      /*!< The maximal wind speed over the duration of the entry         */
		uint8_t  maxWindSpeedDir;   /*!< The direction of the wind of maximal velocity                 */
		uint8_t  prevailingWindDir; /*!< The prevailing wind direction over the duration of the entry  */
		uint8_t  uv;                /*!< The average UV index over the duration of the entry           */
		uint8_t  et;                /*!< The total evapotranspriation measured over the duration of the entry */
		uint16_t maxSolarRad;       /*!< The maximal solar radiation over the duration of the entry    */
		uint8_t  maxUV;             /*!< The maximal UV index measured over the duration of the entry  */
		uint8_t  forecast;          /*!< The forecast at the end of the entry period                   */
		uint8_t  leafTemp[2];       /*!< Additional leaf temperatures values                           */
		uint8_t  leafWetness[2];    /*!< Additional leaf wetness values                                */
		uint8_t  soilTemp[4];       /*!< Additional soil temperature values                            */
		uint8_t  recordType;        /*!< A special value indicating the format of this entry           */
		uint8_t  extraHum[2];       /*!< Additional humidity values                                    */
		uint8_t  extraTemp[3];      /*!< Additional temperature values                                 */
		uint8_t  soilMoisture[4];   /*!< Additional soil moistures values                              */
	} __attribute__((packed));

	/**
	 * @brief Construct a \a VantagePro2ArchiveMessage from an archive entry
	 * and a \a TimeOffseter
	 *
	 * @param data A raw buffer originating from a VantagePro2 station obtained
	 * via a DMP or DMPAFT command
	 * @param timeOffseter The \a TimeOffseter having all the clues to correctly
	 * convert timestamps from and to the station local time
	 */
	VantagePro2ArchiveMessage(const ArchiveDataPoint& data, const TimeOffseter* timeOffseter);
	virtual void populateDataPoint(const CassUuid station, CassStatement* const statement) const override;
	virtual void populateV2DataPoint(const CassUuid station, CassStatement* const statement) const override;

	inline date::sys_seconds getTimestamp() const {
		return date::floor<chrono::seconds>(
			_timeOffseter->convertFromLocalTime(
				_data.day, _data.month, _data.year + 2000,
				_data.time / 100, _data.time % 100)
			);
	}

	/**
	 * @brief Do very basic checks on the consistency of the data point
	 *
	 * For now, this method checks that the date is not '0', nor in the future.
	 * It's not entirely foolproof but cover all known cases of uninitialized
	 * archive records.
	 */
	inline bool looksValid() const {
		return memcmp(&_data, "\0\0\0\0", 4) != 0 &&
		       getTimestamp() < chrono::system_clock::now();
	}

private:
	/**
	 * @brief The data point, an individual archive entry received from the station
	 */
	ArchiveDataPoint _data;

	/**
	 * @brief The \a TimeOffseter able to convert the archive entries' timestamps to
	 * POSIX time
	 */
	const TimeOffseter* _timeOffseter;
};

}

#endif /* VANTAGEPRO2ARCHIVEMESSAGE_H */
