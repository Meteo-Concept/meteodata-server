/**
 * @file oseren_soil_station_message.h
 * @brief Definition of the OserenSoilStationMessage class
 * @author Laurent Georget
 * @date 2025-07-09
 */
/*
 * Copyright (C) 2025  SAS JD Environnement <contact@meteo-concept.fr>
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

#ifndef OSEREN_SOIL_STATION_MESSAGE_H
#define OSEREN_SOIL_STATION_MESSAGE_H

#include <limits>
#include <iostream>
#include <string>
#include <map>
#include <optional>
#include <cmath>

#include <boost/json.hpp>
#include <date/date.h>
#include <cassobs/observation.h>
#include <cassandra.h>

#include "liveobjects/liveobjects_message.h"

namespace meteodata
{

/**
 * @brief A Message able to receive and store a Barani rain gauge IoT payload from a
 * low-power connection (LoRa, NB-IoT, etc.)
 */
class OserenSoilStationMessage : public LiveobjectsMessage
{
public:
	Observation getObservation(const CassUuid& station) const override;

	/**
	 * @brief Parse the payload to build a specific datapoint for a given
	 * timestamp (not part of the payload itself)
	 *
	 * @param data The payload received by some mean, it's a ASCII-encoded
	 * hexadecimal string
	 * @param datetime The timestamp of the data message
	 */
	void ingest(const CassUuid& station, const std::string& payload, const date::sys_seconds& datetime) override;

	inline bool looksValid() const override { return _obs.valid; }

	boost::json::object getDecodedMessage() const override;

private:
	/**
	 * @brief A struct used to store observation values to then populate the
	 * DB insertion query
	 */
	struct DataPoint
	{
		bool valid = false;
		int header;
		date::sys_seconds time;
		float temperature;
		int humidity;
		int pressure;
		float rainfall;
		float windspeed;
		int winddir;
		float soilTemp10;
		float soilVWC10;
		float soilTemp50;
		float soilVWC50;
		float soilTemp100;
		float soilVWC100;
		float enclosureTemp;
		int enclosureHum;
		int battery;
	};

	/**
	 * @brief An observation object to store values as the API return value
	 * is getting parsed
	 */
	DataPoint _obs;
};

}

#endif /* OSEREN_SOIL_STATION_MESSAGE_H */
