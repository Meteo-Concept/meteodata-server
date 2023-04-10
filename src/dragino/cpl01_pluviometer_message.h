/**
 * @file cpl01_pluviometer_message.h
 * @brief Definition of the Cpl01PluviometerMessage class
 * @author Laurent Georget
 * @date 2023-04-07
 */
/*
 * Copyright (C) 2023  SAS JD Environnement <contact@meteo-concept.fr>
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

#ifndef CPL01_PLUVIOMETER_MESSAGE_H
#define CPL01_PLUVIOMETER_MESSAGE_H

#include <string>
#include <vector>
#include <chrono>
#include <iterator>
#include <cmath>

#include <boost/property_tree/ptree.hpp>
#include <date.h>
#include <observation.h>
#include <dbconnection_observations.h>
#include <cassandra.h>

#include "liveobjects/liveobjects_message.h"

namespace meteodata
{
/**
 * @brief A Message able to receive and store the payload from Dragino CPL-01
 * configured for rainfall measurement
 */
class Cpl01PluviometerMessage : public LiveobjectsMessage
{
public:
	explicit Cpl01PluviometerMessage(DbConnectionObservations& db);

	void ingest(const CassUuid& station, const std::string& payload, const date::sys_seconds& datetime) override;

	void cacheValues(const CassUuid& station) override;

	inline bool looksValid() const override
	{
		return _obs.valid;
	}

	Observation getObservation(const CassUuid& station) const override;

	boost::property_tree::ptree getDecodedMessage() const override;

private:
	/**
	 * A reference to the database connection, to get or store cached values
	 */
	DbConnectionObservations& _db;

	/**
	 * @brief A struct used to store observation values to then populate the
	 * DB insertion query
	 */
	struct DataPoint
	{
		bool valid = false;
		date::sys_seconds time;
		int flag;
		bool alarm;
		bool currentlyOpen;
		uint16_t totalPulses;
		float rainfall = NAN;
	};

	/**
	 * @brief An observation object to store values as the API return value
	 * is getting parsed
	 */
	DataPoint _obs;

	/**
	 * The rain gauge scale in mm
	 */
	static constexpr float CPL01_RAIN_GAUGE_RESOLUTION = 0.2f;

	/**
	 * The cache key used to store the rainfall last number of clicks
	 */
	static constexpr char CPL01_RAINFALL_CACHE_KEY[] = "cpl01_rainfall_clicks";
};

}

#endif /* CPL01_PLUVIOMETER_MESSAGE_H */
