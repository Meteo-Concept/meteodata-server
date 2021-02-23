/**
 * @file objenious_api_archive_message_collection.h
 * @brief Definition of the ObjeniousApiArchiveMessageCollection class
 * @author Laurent Georget
 * @date 2021-02-23
 */
/*
 * Copyright (C) 2021  SAS JD Environnement <contact@meteo-concept.fr>
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

#ifndef OBJENIOUS_API_ARCHIVE_MESSAGE_COLLECTION_H
#define OBJENIOUS_API_ARCHIVE_MESSAGE_COLLECTION_H

#include <iostream>
#include <vector>
#include <map>

#include <date.h>

#include "./objenious_archive_message.h"

namespace meteodata {

/**
 * @brief A parser able to receive and store a JSON file resulting from a call
 * to https://api.objenious.com/v2/data/.../raw/last/1
 *
 * The collection will eventually hold several instances of
 * ObjeniousApiArchiveMessage.
 */
class ObjeniousApiArchiveMessageCollection
{
private:
	/**
	 * @brief The sensors known for this station
	 *
	 * This is a map from meteorological variables like "humidity", "wind
	 * direction" to Objenious ids such as "humidity" which are keys in
	 * the JSON data objects returned by the API.
	 */
	const std::map<std::string, std::string>* _variables;

	/**
	 * @brief All the individual messages parsable from the JSON returned by
	 * the Objenious API
	 *
	 * If the API returns several datapoints, there will be arranged in an
	 * array containing a datetime and several variables, depending on the
	 * sensors availables. The collection will create a message for each
	 * datapoint.
	 */
	std::vector<ObjeniousApiArchiveMessage> _messages;

	bool _mayHaveMore = false;

	std::string _cursor;

public:
	/**
	 * @brief Instantiate the collection with the data specific to a station
	 *
	 * @param timeOffseter The TimeOffseter instance necessary to convert
	 * datetimes between the station local timezone and the UTC timezone
	 * @param sensors The map of sensors known for this station (a map from
	 * meteorological variables such as "temperature" and Objenious API
	 * id such as "temperature" found in the JSON output)
	 */
	ObjeniousApiArchiveMessageCollection(const std::map<std::string, std::string>* variables);

	/**
	 * @brief Parse the body of a Objenious API data response to create
	 * the corresponding messages (instances of
	 * ObjeniousApiArchiveMessage)
	 *
	 * @param input An iterator to the body of the API response (a JSON
	 * string)
	 */
	void parse(std::istream& input);

	/**
	 * @brief Gets an iterator to the beginning of the messages
	 *
	 * Calling this method before parse() will not yield much...
	 *
	 * @return The begin() iterator of the list of messages succesfully
	 * parsed
	 */
	inline auto begin() const {
		return _messages.cbegin();
	}
	/**
	 * @brief Gets an iterator past-the-end of the list of messages
	 *
	 * @return The end() iterator of the list of messages succesfully
	 * parsed
	 */
	inline auto end() const {
		return _messages.cend();
	}

	/**
	 * @brief Get the greatest timestamp (i.e. most recent datetime)
	 * available among the parsed messages
	 *
	 * Calling this method before parse() or if the list of messages will
	 * result in undefined behaviour.
	 *
	 * @return The most recent datetime which will be inserted in the
	 * database thanks to this messages collection
	 */
	inline date::sys_seconds getNewestMessageTime() const {
		return _messages.back()._obs.time;
	}

	inline bool mayHaveMore() const {
		return _mayHaveMore;
	}

	inline std::string getPaginationCursor() const {
		return _cursor;
	}
};

}

#endif /* OBJENIOUS_API_ARCHIVE_MESSAGE_COLLECTION_H */
