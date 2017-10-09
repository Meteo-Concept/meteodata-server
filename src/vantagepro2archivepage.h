/**
 * @file vantagepro2archivepage.h
 * @brief Definition of the VantagePro2Message class
 * @author Laurent Georget
 * @date 2017-10-10
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

#ifndef VANTAGEPRO2ARCHIVEPAGE_H
#define VANTAGEPRO2ARCHIVEPAGE_H

#include <cstdint>

#include <boost/asio.hpp>

using std::uint8_t;
using std::uint16_t;
using std::uint32_t;
using std::uint64_t;

namespace meteodata {

namespace asio = boost::asio;

/**
 * @brief A class able to store an archive page downloaded from a VantagePro2 (R)
 * station, by Davis Instruments (R)
 *
 * This class does not inherit \a Message because it does not represent an
 * individual data point, ready for insertion into the database.
 */
class VantagePro2ArchivePage
{
public:
	/**
	 * @brief Get a reference to the Boost::Asio buffer to receive the raw
	 * data from  the station
	 *
	 * @return a reference to the buffer in which the
	 * VantagePro2ArchivePage can store data from the station
	 */
	asio::mutable_buffer& getBuffer() {
		return _pageBuffer;
	}

	/**
	 * @brief Check the integrity of the received data by computing its CRC
	 * @see VantagePro2Message::validateCRC
	 *
	 * @return true if, and only if, the data has been correctly received
	 */
	bool isValid() const;

private:
	/**
	 * @brief An archive page, used by VantagePro2 (R) stations, and
	 * documented by Davis Instruments (R)
	 */
	struct ArchiveDataPoint
	{
		uint16_t date;
		uint16_t time;
		uint16_t outsideTemp;
		uint16_t maxOutsideTemp;
		uint16_t minOutsideTemp;
		uint16_t rainfall;
		uint16_t maxRainRate;
		uint16_t barometer;
		uint16_t solarRad;
		uint16_t nbWindSamples;
		uint16_t insideTemp;
		uint8_t  insideHum;
		uint8_t  outsideHum;
		uint8_t  avgWindSpeed;
		uint8_t  maxWindSpeed;
		uint8_t  maxWindSpeedDir;
		uint8_t  prevailingWindDir;
		uint8_t  uv;
		uint8_t  et;
		uint16_t maxSolarRad;
		uint8_t  maxUV;
		uint8_t  forecast;
		uint8_t  leafTemp[2];
		uint8_t  leafWetness[2];
		uint8_t  soilTemp[4];
		uint8_t  recordType;
		uint8_t  extraHum[2];
		uint8_t  extraTemp[3];
		uint8_t  soilMoisture[4];
	} __attribute__((packed));

	struct ArchivePage
	{
		uint8_t _sequenceNumber; /*!< The sequence number sent at the beginning of each archive page */
		ArchiveDataPoint _point[5]; /*!< The five data points this page contains */
		int : 4;
		uint16_t _crc; /*!< The CRC sent at the bottom of each archive page */
	} __attribute__((packed));

	/**
	 * @brief The archive page received from the station
	 */
	ArchivePage _page;

	/**
	 * @brief The Boost::Asio buffer in which the raw data point from the
	 * station is to be received
	 */
	asio::mutable_buffer _pageBuffer = asio::buffer(&_page, sizeof(_page));
};

}

#endif /* VANTAGEPRO2ARCHIVEPAGE_H */
