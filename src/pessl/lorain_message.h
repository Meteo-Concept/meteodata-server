/**
 * @file lorain_message.h
 * @brief Definition of the LorainMessage class
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

#ifndef LORAIN_MESSAGE_H
#define LORAIN_MESSAGE_H

#include <limits>
#include <iostream>
#include <string>
#include <map>
#include <optional>
#include <cmath>

#include <boost/property_tree/ptree.hpp>
#include <date.h>
#include <observation.h>
#include <cassandra.h>

#include "../message.h"

namespace meteodata
{

namespace pt = boost::property_tree;

/**
 * @brief A Message able to receive and store a Lorain IoT payload from a
 * low-power connection (LoRa, NB-IoT, etc.)
 */
class LorainMessage
{
public:
	/**
	 * @brief Construct a message
	 */
	LorainMessage() = default;

	/**
	 * @brief Destruct the message
	 */
	~LorainMessage() = default;

	Observation getObservation(const CassUuid station) const;

	/**
	 * @brief Parse the payload to build a specific datapoint for a given
	 * timestamp (not part of the payload itself)
	 *
	 * @param data The payload received by some mean, it's a ASCII-encoded
	 * 45-bytes hexadecimal string
	 * @param datetime The timestamp of the data message
	 */
	void ingest(const std::string& payload, const date::sys_seconds& datetime, std::optional<int> lastRainfallClicks);

	inline int getRainfallClicks() const { return _obs.rainfallClicks; }

	inline bool looksValid() const { return _obs.valid; }

private:
	/**
	 * @brief A struct used to store observation values to then populate the
	 * DB insertion query
	 */
	struct DataPoint
	{
		bool valid = false;
		date::sys_seconds time;
		int batteryVoltage;    // mV
		int solarPanelVoltage; // mV
		int rainfallClicks;
		float rainfall;       // mm
		float temperature;    // °C
		float minTemperature; // °C
		float maxTemperature; // °C
		float humidity;    // %
		float minHumidity; // %
		float maxHumidity; // %
		float deltaT;    // °c
		float minDeltaT; // °C
		float maxDeltaT; // °C
		float dewPoint; // °C
		float minDewPoint; // °C
		float vaporPressureDeficit; // kPa
		float minVaporPressureDeficit; // kPa
		int leafWetnessTimeRatio; // min
	};

	/**
	 * @brief An observation object to store values as the API return value
	 * is getting parsed
	 */
	DataPoint _obs;
};

}

#endif /* LORAIN_MESSAGE_H */