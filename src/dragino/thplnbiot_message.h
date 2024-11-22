/**
 * @file thplnbiot_message.h
 * @brief Definition of the ThplnbiotMessage class
 * @author Laurent Georget
 * @date 2024-06-19
 */
/*
 * Copyright (C) 2024  SAS JD Environnement <contact@meteo-concept.fr>
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

#ifndef THPLNBIOT_MESSAGE_H
#define THPLNBIOT_MESSAGE_H

#include <string>
#include <vector>
#include <chrono>
#include <iterator>
#include <cmath>

#include <date/date.h>
#include <cassobs/observation.h>
#include <cassandra.h>

#include "cassobs/dbconnection_observations.h"


namespace meteodata
{
/**
 * @brief A Message able to receive and store a Dragino SN50v3 thermohygrometer
 * IoT payload from a low-power connection (LoRa, NB-IoT, etc.)
 */
class ThplnbiotMessage
{
public:
	explicit ThplnbiotMessage(DbConnectionObservations& db);

	/**
	 * @brief Parse the payload to build a specific datapoint for a given
	 * timestamp (not part of the payload itself)
	 *
	 * @param station The station identifier
	 * @param data The payload received by some mean, it's a ASCII-encoded
	 * hexadecimal string
	 */
	void ingest(const CassUuid& station, const std::string& payload);

	void cacheValues(const CassUuid& station);

	std::vector<Observation> getObservations(const CassUuid& station) const;

private:
	DbConnectionObservations& _db;

	bool validateInput(const std::string& payload);

	/**
	 * @brief A struct used to store observation values to then populate the
	 * DB insertion query
	 */
	struct DataPoint
	{
		bool valid = false;
		date::sys_seconds time;
		float temperature = NAN;
		float humidity = NAN;
		uint32_t count = 0;
		float rainfall = NAN;
		float intensity = NAN;

	};

	/**
	 * @brief A collection of observation objects to store values
	 */
	std::vector<DataPoint> _obs;

	bool _valid = false;

	static constexpr size_t HEADER_LENGTH = 28;
	static constexpr size_t FOOTER_LENGTH = 64;
	static constexpr size_t DATA_POINT_LENGTH = 28;
	static constexpr char THPLNBIOT_RAINFALL_CACHE_KEY[] = "thplnbiot_rainfall_clicks";
	static constexpr float THPLNBIOT_RAIN_GAUGE_RESOLUTION = 0.2f;
};

}

#endif /* THPLNBIOT_MESSAGE_H */
