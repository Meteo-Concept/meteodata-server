/**
 * @file barani_meteoag_2022_message.h
 * @brief Definition of the BaraniMeteoAg2022Message class
 * @author Laurent Georget
 * @date 2024-03-11
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

#ifndef BARANI_METEOAG_2022_MESSAGE_H
#define BARANI_METEOAG_2022_MESSAGE_H

#include <string>

#include <boost/json.hpp>
#include <date/date.h>
#include <observation.h>
#include <dbconnection_observations.h>
#include <cassandra.h>

#include "liveobjects/liveobjects_message.h"

namespace meteodata
{

/**
 * @brief A Message able to receive and store a Barani MeteoAg (multi-probe
 * generic device) IoT payload from a low-power connection (LoRa, NB-IoT, etc.)
 */
class BaraniMeteoAg2022Message : public LiveobjectsMessage
{
public:
	explicit BaraniMeteoAg2022Message(DbConnectionObservations& db);

	Observation getObservation(const CassUuid& station) const override;

	void ingest(const CassUuid& station, const std::string& payload, const date::sys_seconds& datetime) override;

	/**
	 * @brief Parse the payload to build a specific datapoint for a given
	 * timestamp (not part of the payload itself)
	 *
	 * @param data The payload received by some mean, it's a ASCII-encoded
	 * hexadecimal string
	 * @param datetime The timestamp of the data message
	 */
	void ingest(
		const std::string& payload,
		const date::sys_seconds& datetime
	);

	inline bool looksValid() const override { return _obs.valid; }

	boost::json::object getDecodedMessage() const override;

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
		int selectorE;
		int selectorF;
		int selectorG;
		float sensorE1;
		float sensorE2;
		float sensorE3;
		float sensorF1;
		float sensorF2;
		float sensorF3;
		float sensorG1;
	};

	/**
	 * @brief An observation object to store values as the API return value
	 * is getting parsed
	 */
	DataPoint _obs;

	static std::pair<bool, float> parseSS200(float v, float temp = 24.f);
	static std::pair<bool, float> parse6470(float v);
};

}

#endif /* BARANI_METEOAG_2022_MESSAGE_H */
