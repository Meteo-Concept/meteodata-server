/**
 * @file liveobjects_message.h
 * @brief Definition of the LiveobjectsMessage class
 * @author Laurent Georget
 * @date 2022-04-28
 */
/*
 * Copyright (C) 2022  SAS JD Environnement <contact@meteo-concept.fr>
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

#ifndef LIVEOBJECTS_MESSAGE_H
#define LIVEOBJECTS_MESSAGE_H

#include <memory>

#include <dbconnection_observations.h>
#include <cassandra.h>
#include <observation.h>
#include <boost/property_tree/ptree.hpp>
#include <boost/json.hpp>
#include <date.h>

namespace meteodata {

class LiveobjectsMessage
{
public:
	virtual ~LiveobjectsMessage() = default;

	/**
	 * Validate that the payload looks valid (only characters in the correct
	 * character set, correct length, etc.)
	 * @param payload The LoRa message payload
	 * @param expectedSize The length the payload should have
	 * @return True if, and only if, the payload looks correct, before parsing
	 */
	virtual bool validateInput(const std::string& payload, int expectedSize);

	/**
	 * Get the observation built from the message
	 * @param station The station which will receive the observation in the
	 * database
	 * @return The observation built from the message
	 */
	virtual Observation getObservation(const CassUuid& station) const = 0;

	/**
	 * Whether the observation can be inserted in the database
	 * @return True if, and only if, the message looks good enough to be inserted
	 * in the database
	 */
	virtual inline bool looksValid() const = 0;

	virtual boost::json::object getDecodedMessage() const = 0;

	/**
	 * @brief Parse the payload to build a specific datapoint for a given
	 * timestamp (not part of the payload itself)
	 *
	 * @param station The station/sensor identifier
	 * @param data The payload received by some mean, it's a ASCII-encoded
	 * hexadecimal string
	 * @param datetime The timestamp of the data message
	 */
	virtual void ingest(const CassUuid& station, const std::string& payload, const date::sys_seconds& timestamp) = 0;

	/**
	 * @brief Store values in the cache database for later message building
	 *
	 * @param station The station/sensor identifier
	 */
	virtual void cacheValues(const CassUuid& station)
	{
		// no-op
	};

	static std::unique_ptr<LiveobjectsMessage> parseMessage(
		DbConnectionObservations& db,
		const boost::property_tree::ptree& json,
		const CassUuid& station,
		date::sys_seconds& timestamp
	);
};

}


#endif //LIVEOBJECTS_MESSAGE_H
