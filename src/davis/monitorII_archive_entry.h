/**
 * @file monitorII_archive_entry.h
 * @brief Definition of the MonitorIIArchiveEntry class
 * @author Laurent Georget
 * @date 2024-12-17
 */
/*
 * Copyright (C) 2024  SAS Météo Concept <contact@meteo-concept.fr>
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

#ifndef MONITORII_ARCHIVE_ENTRY_H
#define MONITORII_ARCHIVE_ENTRY_H

#include <cstdint>
#include <iterator>
#include <chrono>
#include <optional>

#include <boost/asio.hpp>
#include <date/date.h>
#include <cassandra.h>
#include <cassobs/observation.h>

#include "../time_offseter.h"

using std::uint8_t;
using std::uint16_t;
using std::uint32_t;
using std::uint64_t;

namespace meteodata
{

namespace asio = boost::asio;
namespace chrono = std::chrono;

class DbConnectionObservations;

/**
 * @brief A class able to store an archive page downloaded from a Monitor II (R)
 * station, by Davis Instruments (R)
 */
class MonitorIIArchiveEntry
{
public:
	/**
	 * @brief A type of buffer able to receive one entry of archive
	 * downloaded using commands SRD
	 */
	struct DataPoint
	{
		uint16_t barometer;
		uint8_t insideHumidity;
		uint8_t outsideHumidity;
		uint16_t rainfall;
		int16_t avgInsideTemperature;
		int16_t avgOutsideTemperature;
		uint8_t avgWindSpeed;
		uint8_t dominantWindDir;
		int16_t hiOutsideTemperature;
		uint8_t hiWindSpeed;
		uint32_t timestamp;
		int16_t lowOutsideTemperature;
		uint16_t crc; /*!< The CRC sent at the bottom of each archive page */
	} __attribute__((packed));

	explicit MonitorIIArchiveEntry(const DataPoint& data);

	Observation getObservation(CassUuid station) const;

	inline date::sys_seconds getTimestamp() const
	{
		return date::floor<chrono::seconds>(chrono::system_clock::from_time_t(_datapoint.timestamp));
	}

	/**
	 * @brief Do very basic checks on the consistency of the data point
	 *
	 * For now, this method checks that the date is not '0', nor in the future.
	 * It's not entirely foolproof but cover all known cases of uninitialized
	 * archive records.
	 */
	inline bool looksValid(std::optional<date::sys_seconds> notBefore = std::nullopt) const
	{
		auto t = getTimestamp();
		return  memcmp(&_datapoint, "\0\0\0\0", 4) != 0 &&
			t < chrono::system_clock::now() &&
			(!notBefore || t > *notBefore);
	}

private:
	/**
	 * @brief The archive page recently received from the station
	 */
	DataPoint _datapoint;
};

}

#endif /* MONITORII_ARCHIVE_ENTRY_H */
