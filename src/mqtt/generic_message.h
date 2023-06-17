/**
 * @file generic_message.h
 * @brief Definition of the GenericMessage class
 * @author Laurent Georget
 * @date 2023-06-16
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

#ifndef GENERIC_MESSAGE_H
#define GENERIC_MESSAGE_H

#include <memory>

#include <dbconnection_observations.h>
#include <cassandra.h>
#include <observation.h>
#include <boost/property_tree/ptree.hpp>
#include <boost/json.hpp>
#include <date.h>

namespace meteodata {

class GenericMessage
{
public:
	/**
	 * Get the observation built from the message
	 * @param station The station which will receive the observation in the
	 * database
	 * @return The observation built from the message
	 */
	virtual Observation getObservation(const CassUuid& station) const;

	/**
	 * Whether the observation can be inserted in the database
	 * @return True if, and only if, the message looks good enough to be inserted
	 * in the database
	 */
	virtual bool looksValid() const;

	/**
	 * @brief Store values in the cache database for later message building
	 *
	 * @param station The station/sensor identifier
	 */
	virtual void cacheValues(const CassUuid& station)
	{
		// no-op
	};

	static GenericMessage buildMessage(
		DbConnectionObservations& db,
		const boost::property_tree::ptree& json,
		date::sys_seconds& timestamp
	);

private:
	/**
	 * @brief A struct used to store observation values to then populate the
	 * DB insertion query
	 */
	struct DataPoint
	{
		bool valid = false;
		date::sys_seconds time;
		float atmosphericPressure = NAN;
		float windAvg = NAN;
		float windMax = NAN;
		float temperature = NAN;
		float temperature_min = NAN;
		float temperature_max = NAN;
		float humidity = NAN;
		float windDir = NAN;
		float dewPoint = NAN;
		float rainfall = NAN;
		float rainrate = NAN;
		float solarrad = NAN;
		float uv = NAN;
	};

	/**
	 * @brief An observation object to store values as the API return value
	 * is getting parsed
	 */
	DataPoint _obs;

	bool _valid;
	boost::property_tree::ptree _content;
};

}


#endif //GENERIC_MESSAGE_H
