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

class DbConnectionObservations;

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
	inline std::array<asio::mutable_buffer,1>& getBuffer()
	{
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
	 */
	bool store(DbConnectionObservations& db, const CassUuid& station);

	/**
	 * @brief Give the timestamp of the most recent relevant archive entry
	 *
	 * @return The timestamp of the last archive entry which should be
	 * inserted into the database
	 */
	inline date::sys_seconds lastArchiveRecordDateTime() const
	{
		return _mostRecent;
	}

	/**
	 * @brief Prepare an archive page so that the archive download may
	 * start
	 *
	 * This function essentially initialize some internal state so that
	 * archive entries downloaded from the station are correctly parsed.
	 * It is especially important to inform the \a VantagePro2ArchivePage
	 * of the time setting of the station.
	 *
	 * @param beginning The timestamp of the last data entry from the
	 * station stored into the database
	 * @param timeOffseter The \a TimeOffseter able to convert timestamps
	 * between the server POSIX time and the station local time
	 */
	void prepare(const date::sys_seconds& beginning, const TimeOffseter* timeOffseter);

private:

	/**
	 * @brief The number of archive entries in each downloaded page
	 */
	static constexpr int NUMBER_OF_DATA_POINTS_PER_PAGE = 5;

	/**
	 * @brief A type of buffer able to receive one page of archive
	 * downloaded using commands DMP or DMPAFT
	 */
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

	/**
	 * @brief The time since which archived data must be collected
	 */
	date::sys_seconds _beginning;
	/**
	 * @brief The timestamp of the beginning of the archive retrieval
	 */
	date::sys_seconds _now;
	/**
	 * @brief The timestamp of the most recent archive entry fetched
	 * so far from the station
	 *
	 * This value is used to update the database. Next time archive
	 * have to be retrieved from the station, it will be possible to
	 * pass this value to the station to limit the archive download
	 * to more recent entries, in order to avoid processing already
	 * outdated entries.
	 */
	date::sys_seconds _mostRecent;

	/**
	 * @brief The time converter that is to be used to parse the
	 * station's timestamps
	 */
	const TimeOffseter* _timeOffseter;

	/**
	 * @brief The Boost::Asio buffer in which the raw data point from the
	 * station is to be received
	 */
	std::array<asio::mutable_buffer,1> _pageBuffer = { {asio::buffer(&_page, sizeof(ArchivePage))} };

	/**
	 * @brief Tell whether an archive entry should be inserted into the
	 * database
	 *
	 * Criteria for entering the database are essentially fitting a hole in
	 * the timeseries of data for this station and having a consistent
	 * timestamp (not newer than current time).
	 *
	 * @param point The archive entry
	 * @param v2 Whether we are considering the v1 or v2 database
	 *
	 * @return True if, and only if, \a point should be inserted in the
	 * database
	 */
	bool isRelevant(const VantagePro2ArchiveMessage::ArchiveDataPoint& point, bool v2);
};

}

#endif /* VANTAGEPRO2ARCHIVEPAGE_H */
