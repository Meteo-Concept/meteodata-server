/**
 * @file thlora_thermohygrometer_message.h
 * @brief Definition of the ThloraThermohygrometerMessage class
 * @author Laurent Georget
 * @date 2022-10-04
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

#ifndef THLORA_THERMOHYGROMETER_MESSAGE_H
#define THLORA_THERMOHYGROMETER_MESSAGE_H

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

#include "liveobjects/liveobjects_message.h"

namespace meteodata
{

namespace pt = boost::property_tree;

/**
 * @brief A Message able to receive and store a Barani rain gauge IoT payload from a
 * low-power connection (LoRa, NB-IoT, etc.)
 */
class ThloraThermohygrometerMessage : public LiveobjectsMessage
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
	void ingest(const CassUuid& station, const std::string& payload, const date::sys_seconds& datetime);

	inline bool looksValid() const override { return _obs.valid; }

	boost::property_tree::ptree getDecodedMessage() const override;

private:
	/**
	 * @brief A struct used to store observation values to then populate the
	 * DB insertion query
	 */
	struct DataPoint
	{
		bool valid = false;
		date::sys_seconds time;
		int header;
		float temperature;
		float humidity;
		int period;
		int rssi;
		float snr;
		float battery;
	};

	/**
	 * @brief An observation object to store values as the API return value
	 * is getting parsed
	 */
	DataPoint _obs;
};

}

#endif /* THLORA_THERMOHYGROMETER_MESSAGE_H */
