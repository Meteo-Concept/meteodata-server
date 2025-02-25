/**
 * @file liveobjects_message.cpp
 * @brief Implementation of the LiveobjectsMessage class
 * @author Laurent Georget
 * @date 2022-04-29
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

#include <algorithm>
#include <initializer_list>
#include <iostream>
#include <string>
#include <systemd/sd-daemon.h>

#include "liveobjects_message.h"
#include "dragino/cpl01_pluviometer_message.h"
#include "dragino/lsn50v2_thermohygrometer_message.h"
#include "dragino/lsn50v2_probe6470_message.h"
#include "dragino/sn50v3_probe6470_message.h"
#include "dragino/llms01_leaf_sensor_message.h"
#include "dragino/lse01_soil_sensor_message.h"
#include "dragino/thpllora_message.h"
#include "barani/barani_anemometer_message.h"
#include "barani/barani_anemometer_2023_message.h"
#include "barani/barani_rain_gauge_message.h"
#include "barani/barani_thermohygro_message.h"
#include "barani/barani_meteoag_2022_message.h"
#include "pessl/lorain_message.h"
#include "thlora/thlora_thermohygrometer_message.h"
#include "talkpool/oy1110_thermohygrometer_message.h"
#include "cassandra_utils.h"

namespace meteodata
{

bool LiveobjectsMessage::validateInput(const std::string& payload, std::initializer_list<int> expectedSize)
{
	if (std::none_of(expectedSize.begin(), expectedSize.end(), [&payload](int s) { return payload.length() == s; })) {
		std::cerr << SD_ERR << "[MQTT Liveobjects] protocol: " << "Invalid size " << payload.length()
			  << " for payload " << payload << std::endl;
		return false;
	}

	if (!std::all_of(payload.cbegin(), payload.cend(), [](char c) {
		return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
	})) {
		std::cerr << SD_ERR << "[MQTT Liveobjects] protocol: " << "Payload " << payload
			  << " contains invalid characters" << std::endl;
		return false;
	}

	return true;
}

std::unique_ptr<LiveobjectsMessage> LiveobjectsMessage::instantiateMessage(DbConnectionObservations& db,
	const std::string& sensor, int port, const CassUuid& station)
{
	if (sensor == "dragino-cpl01-pluviometer" && port == 2) {
		return std::make_unique<Cpl01PluviometerMessage>(db);
	} else if ((sensor == "dragino-lsn50v2" || sensor == "dragino_lsn50v2") && port == 2) {
		return std::make_unique<Lsn50v2ThermohygrometerMessage>();
	} else if (sensor == "dragino-thpllora" && port == 2) {
		return std::make_unique<ThplloraMessage>(db);
	} else if (sensor == "dragino-llms01" && port == 2) {
		return std::make_unique<Llms01LeafSensorMessage>();
	} else if (sensor == "dragino-lse01" && port == 2) {
		return std::make_unique<Lse01SoilSensorMessage>();
	} else if (sensor == "dragino-probe6470" && port == 2) {
		return std::make_unique<Lsn50v2Probe6470Message>();
	} else if (sensor == "dragino-sn50v3-probe6470" && port == 2) {
		return std::make_unique<Sn50v3Probe6470Message>();
	} else if (sensor == "barani-meteowind" && port == 1) {
		return std::make_unique<BaraniAnemometerMessage>();
	} else if (sensor == "barani-meteowind-v2023" && port == 1) {
		return std::make_unique<BaraniAnemometer2023Message>();
	} else if (sensor == "barani-meteorain" && port == 1) {
		return std::make_unique<BaraniRainGaugeMessage>(db);
	} else if (sensor == "barani-meteohelix" && port == 1) {
		return std::make_unique<BaraniThermohygroMessage>(db);
	} else if (sensor == "barani-meteoag-2022" && port == 1) {
		return std::make_unique<BaraniMeteoAg2022Message>(db);
	} else if (sensor == "lorain-pluviometer") {
		return std::make_unique<LorainMessage>(db);
	} else if (sensor == "thlora-thermohygrometer") {
		return std::make_unique<ThloraThermohygrometerMessage>();
	} else if (sensor == "talkpool-oy1110") {
		return std::make_unique<Oy1110ThermohygrometerMessage>(station);
	}
	return {};
}

std::unique_ptr<LiveobjectsMessage> LiveobjectsMessage::parseMessage(DbConnectionObservations& db,
	const boost::property_tree::ptree& json, const CassUuid& station, date::sys_seconds& timestamp)
{
	auto sensor = json.get<std::string>("extra.sensors", "");
	auto payload = json.get<std::string>("value.payload");
	auto port = json.get<int>("metadata.network.lora.port", -1);

	std::unique_ptr<LiveobjectsMessage> m = instantiateMessage(db, sensor, port, station);

	if (!m) {
		std::cerr << SD_ERR << "[Liveobjects " << station << "] protocol: "
				  << "Misconfigured sensor, unknown sensor type! Aborting." << std::endl;
		return {};
	}

	auto t = json.get<std::string>("timestamp");
	std::istringstream is{t};
	// don't bother parsing the subseconds
	is >> date::parse("%Y-%m-%dT%H:%M:%S", timestamp);

	using namespace date;
	std::cout << SD_DEBUG << "Parsing message with timestamp " << timestamp << std::endl;

	m->ingest(station, payload, timestamp);
	return m;
}

}
