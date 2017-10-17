/**
 * @file vantagepro2archivepage.h
 * @brief Definition of the VantagePro2ArchivePage class
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
#include <iterator>
#include <chrono>

#include <boost/asio.hpp>

#include "vantagepro2archivemessage.h"
#include "timeoffseter.h"

using std::uint8_t;
using std::uint16_t;
using std::uint32_t;
using std::uint64_t;

namespace meteodata {

namespace asio = boost::asio;
namespace chrono = std::chrono;

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
	std::array<asio::mutable_buffer,1>& getBuffer() {
		return _pageBuffer;
	}

	/**
	 * @brief Check the integrity of the received data by computing its CRC
	 * @see VantagePro2Message::validateCRC
	 *
	 * @return true if, and only if, the data has been correctly received
	 */
	bool isValid() const;

	/**
	 * @brief Store the archive data in instances of
	 * \a VantagePro2ArchiveMessage and clear to buffer
	 *
	 * \a storeToMessages must be called once data has been received and
	 * validated in order to commit the data points and prepare the
	 * \a ArchivePage for more incoming data.
	 */
	void storeToMessages();

	/**
	 * @brief Provide an iterator on the collection of
	 * \a VantagePro2ArchiveMessage built from the archive data
	 *
	 * @return An iterator pointing onto the first archive message in the
	 * archive
	 */
	auto cbegin() const { return _archiveMessages.cbegin(); }

	/**
	 * @brief Provide an iterator on the collection of
	 * \a VantagePro2ArchiveMessage built from the archive data
	 *
	 * @return An iterator pointing past the last archive message in the
	 * archive
	 */
	auto cend() const { return _archiveMessages.cend(); }

	date::local_seconds lastArchiveRecordDateTime() const;

	/**
	 * @brief Clear the archive page and make it ready for any future
	 * download
	 */
	void clear();

	void prepare(const date::sys_seconds& beginning, const TimeOffseter* timeOffseter);

private:

	static constexpr int NUMBER_OF_DATA_POINTS_PER_PAGE = 5;

	struct ArchivePage
	{
		uint8_t sequenceNumber; /*!< The sequence number sent at the beginning of each archive page */
		VantagePro2ArchiveMessage::ArchiveDataPoint points[NUMBER_OF_DATA_POINTS_PER_PAGE]; /*!< The data points this page contains */
		int : 32;
		uint16_t crc; /*!< The CRC sent at the bottom of each archive page */
	} __attribute__((packed));

	/**
	 * @brief The archive page recently received from the station
	 */
	ArchivePage _page;

	date::sys_seconds _beginning;
	date::sys_seconds _now;
	date::sys_seconds _mostRecent;

	const TimeOffseter* _timeOffseter;

	/**
	 * @brief A collection of ArchiveMessage, constructed on-the-fly as archive data is received
	 */
	std::vector<VantagePro2ArchiveMessage> _archiveMessages;

	/**
	 * @brief The Boost::Asio buffer in which the raw data point from the
	 * station is to be received
	 */
	std::array<asio::mutable_buffer,1> _pageBuffer = { {asio::buffer(&_page, sizeof(ArchivePage))} };

	bool isRelevant(const VantagePro2ArchiveMessage::ArchiveDataPoint& point);
};

}

#endif /* VANTAGEPRO2ARCHIVEPAGE_H */
