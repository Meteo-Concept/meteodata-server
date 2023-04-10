/**
 * @file barani_rain_gauge_message.h
 * @brief Definition of the BaraniRainGaugeMessage class
 * @author Laurent Georget
 * @date 2022-03-24
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

#ifndef BARANI_RAIN_GAUGE_MESSAGE_H
#define BARANI_RAIN_GAUGE_MESSAGE_H

#include <limits>
#include <iostream>
#include <string>
#include <map>
#include <optional>
#include <cmath>

#include <boost/property_tree/ptree.hpp>
#include <date.h>
#include <observation.h>
#include <dbconnection_observations.h>
#include <cassandra.h>

#include "liveobjects/liveobjects_message.h"

namespace meteodata
{

namespace pt = boost::property_tree;

/**
 * @brief A Message able to receive and store a Barani rain gauge IoT payload from a
 * low-power connection (LoRa, NB-IoT, etc.)
 */
class BaraniRainGaugeMessage : public LiveobjectsMessage
{
public:
	explicit BaraniRainGaugeMessage(DbConnectionObservations& db);

	Observation getObservation(const CassUuid& station) const override;

	void ingest(const CassUuid& station, const std::string& payload, const date::sys_seconds& datetime) override;

	void cacheValues(const CassUuid& station) override;

	/**
	 * @brief Parse the payload to build a specific datapoint for a given
	 * timestamp (not part of the payload itself)
	 *
	 * @param data The payload received by some mean, it's a ASCII-encoded
	 * hexadecimal string
	 * @param datetime The timestamp of the data message
	 * @param previousClicks The previous state of the rain gauge revolving counter
	 * @param previousCorrectionClicks The previous state of the rain gauge correction revolving counter
	 */
	void ingest(const std::string& payload, const date::sys_seconds& datetime,
				float rainGaugeResolution,
				std::optional<int> previousClicks, std::optional<int> previousCorrectionClicks);


	inline int getRainfallClicks() const { return _obs.rainfallClicks; }

	inline int getRainfallCorrectionClicks() const { return _obs.correction; }

	inline bool looksValid() const override { return _obs.valid; }

private:
	DbConnectionObservations& _db;

	/**
	 * @brief A struct used to store observation values to then populate the
	 * DB insertion query
	 */
	struct DataPoint
	{
		bool valid = false;
		int index = -1;
		date::sys_seconds time;
		float batteryVoltage;
		int rainfallClicks;
		float rainfall;
		float minTimeBetweenClicks;
		float maxRainrate;
		bool tempOver2C;
		bool heaterSwitchedOn;
		int correction;
	};

	/**
	 * @brief An observation object to store values as the API return value
	 * is getting parsed
	 */
	DataPoint _obs;

	static constexpr float BARANI_RAIN_GAUGE_RESOLUTION = 0.2f;
	static constexpr char BARANI_RAINFALL_CACHE_KEY[] = "barani_rainfall_clicks";
	static constexpr char BARANI_RAINFALL_CORRECTION_CACHE_KEY[] = "barani_raincorr_clicks";
};

}

#endif /* BARANI_RAIN_GAUGE_MESSAGE_H */
