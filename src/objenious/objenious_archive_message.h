/**
 * @file objenious_api_archive_message.h
 * @brief Definition of the ObjeniousApiArchiveMessage class
 * @author Laurent Georget
 * @date 2020-09-02
 */
/*
 * Copyright (C) 2020  SAS JD Environnement <contact@meteo-concept.fr>
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

#ifndef OBJENIOUS_API_ARCHIVE_MESSAGE_H
#define OBJENIOUS_API_ARCHIVE_MESSAGE_H

#include <limits>
#include <iostream>
#include <string>
#include <map>
#include <cmath>

#include <boost/property_tree/ptree.hpp>
#include <date/date.h>
#include <observation.h>
#include <cassandra.h>

#include "../message.h"
#include "../time_offseter.h"

namespace meteodata
{

namespace pt = boost::property_tree;

class ObjeniousApiArchiveMessageCollection;

class ObjeniousMqttSubscriber;

/**
 * @brief A Message able to receive and store a JSON file resulting from a call to
 * https://api.objenious.com/v2/...
 */
class ObjeniousApiArchiveMessage
{
public:
	/**
	 * @brief Construct an invalid message
	 */
	ObjeniousApiArchiveMessage() = default;

	/**
	 * @brief Destruct the message
	 */
	~ObjeniousApiArchiveMessage() = default;

	inline date::sys_seconds getTimestamp() const
	{
		return _obs.time;
	}

	Observation getObservation(const CassUuid station) const;

private:
	/**
	 * @brief A struct used to store observation values to then populate the
	 * DB insertion query
	 *
	 * All fields are parsed as floats from the API messages (even
	 * integers). Given that the resolution is usually the unit or 0.1, this
	 * shouldn't cause any major rounding issue.
	 */
	struct DataPoint
	{
		date::sys_seconds time;
		float pressure = INVALID_FLOAT; // hPa
		float humidity = INVALID_FLOAT;     // %
		float temperature = INVALID_FLOAT; // °C
		float windDir = INVALID_FLOAT; // °
		float windSpeed = INVALID_FLOAT; // km/h
		float windGustSpeed = INVALID_FLOAT; // km/h
		float rainRate = INVALID_FLOAT; // mm/h
		float rainFall = INVALID_FLOAT; // mm
		float solarRad = INVALID_FLOAT; // W/m2
		float uvIndex = INVALID_FLOAT; // no unit
	};

	/**
	 * @brief An invalid integer to detect invalid sensored values
	 */
	constexpr static int INVALID_INT = std::numeric_limits<int>::min();

	/**
	 * @brief An invalid float to detect invalid sensored values
	 */
	constexpr static float INVALID_FLOAT = std::numeric_limits<float>::quiet_NaN();

	const std::map<std::string, std::string>* _variables;

	static const std::map<std::string, float DataPoint::*> FIELDS;

	/**
	 * @brief Whether a floating point sensored value is invalid
	 *
	 * @param v The value
	 *
	 * @return True if, and only if, the value is invalid (i.e. a missing
	 * or bad sensor reading)
	 */
	constexpr static bool isInvalid(float v)
	{
		return std::isnan(v); // /!\ NaN never compares equal to itself
	}

	/**
	 * @brief Whether an integer sensored value is invalid
	 *
	 * @param v The value
	 *
	 * @return True if, and only if, the value is invalid (i.e. a missing
	 * or bad sensor reading)
	 */
	constexpr static bool isInvalid(int v)
	{
		return v == INVALID_INT;
	}

	/**
	 * @brief Create an invalid value of the integer type
	 *
	 * @param int A parameter that is ignored but used for overload
	 * resolution
	 *
	 * @return An integer default invalid value
	 */
	constexpr static int invalidDefault(const int&)
	{
		return INVALID_INT;
	}

	/**
	 * @brief Create an invalid value of the float type
	 *
	 * @param float A parameter that is ignored but used for overload
	 * resolution
	 *
	 * @return A float default invalid value
	 */
	constexpr static float invalidDefault(const float&)
	{
		return INVALID_FLOAT;
	}

	/**
	 * @brief An observation object to store values as the API return value
	 * is getting parsed
	 */
	DataPoint _obs;

	/**
	 * @brief The real constructor used by the
	 * ObjeniousApiArchiveMessageCollection class to instantiate this
	 * class
	 *
	 * @param variables The variables registered for the station (also
	 * shared by many messages)
	 */
	ObjeniousApiArchiveMessage(const std::map<std::string, std::string>* variables);

	/**
	 * @brief Parse the data output by the Objenious API to extract one
	 * datapoint (for a specific datetime)
	 *
	 * The API may answer with several datapoints. The second parameter (the
	 * index) indicates which datapoint has to be parsed.
	 *
	 * @param data The entire JSON object handed over by the API
	 * @param index The specific data index to consider
	 */
	void ingest(const pt::ptree& data);

	friend class ObjeniousApiArchiveMessageCollection;

	friend class ObjeniousMqttSubscriber;
};

}

#endif /* OBJENIOUS_ARCHIVE_MESSAGE_H */
