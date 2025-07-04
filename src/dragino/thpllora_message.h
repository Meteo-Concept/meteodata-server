/**
 * @file thpllora_message.h
 * @brief Definition of the ThplloraMessage class
 * @author Laurent Georget
 * @date 2023-05-01
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

#ifndef THPLLORA_MESSAGE_H
#define THPLLORA_MESSAGE_H

#include <string>
#include <vector>
#include <chrono>
#include <iterator>
#include <cmath>
#include <optional>

#include <boost/json.hpp>
#include <date/date.h>
#include <cassobs/observation.h>
#include <cassobs/dbconnection_observations.h>
#include <cassandra.h>

#include "liveobjects/liveobjects_message.h"

namespace meteodata
{
/**
 * @brief A Message able to receive and store a Dragino LSN50v2 thermohygrometer
 * IoT payload from a low-power connection (LoRa, NB-IoT, etc.)
 */
class ThplloraMessage : public LiveobjectsMessage
{
public:
	ThplloraMessage(DbConnectionObservations& db, std::optional<int> forceRainfallCount = std::nullopt);

	/**
	 * @brief Parse the payload to build a specific datapoint for a given
	 * timestamp (not part of the payload itself)
	 *
	 * @param station The station identifier
	 * @param data The payload received by some mean, it's a ASCII-encoded
	 * hexadecimal string
	 * @param datetime The timestamp of the data message
	 */
	void ingest(const CassUuid& station, const std::string& payload, const date::sys_seconds& datetime) override;

	void cacheValues(const CassUuid& station) override;

	std::optional<float> getSingleCachedValue() override;

	inline bool looksValid() const override
	{
		return _obs.valid;
	}

	Observation getObservation(const CassUuid& station) const override;

	boost::json::object getDecodedMessage() const override;

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
		float battery = NAN;
		float temperature = NAN;
		float humidity = NAN;
		uint16_t totalPulses;
		float rainfall = NAN;
		float rainrate = NAN;
		float windSpeed = NAN;
		float gustSpeed = NAN;
		float windDir = NAN;
	};

	/**
	 * @brief An observation object to store values as the API return value
	 * is getting parsed
	 */
	DataPoint _obs;

	/**
	 * @brief A base count to compare the rainfall contained in the message,
	 * useful when recovering data in the past instead of relying on the
	 * last known value
	 */
	std::optional<int> _forcedRainfallCount;

	/**
	 * The rain gauge scale in mm
	 */
	static constexpr float THPLLORA_RAIN_GAUGE_RESOLUTION = 0.2f;

	/**
	 * The cache key used to store the rainfall last number of clicks
	 */
	static constexpr char THPLLORA_RAINFALL_CACHE_KEY[] = "thpllora_rainfall_clicks";
};

}

#endif /* THPLLORA_MESSAGE_H */
