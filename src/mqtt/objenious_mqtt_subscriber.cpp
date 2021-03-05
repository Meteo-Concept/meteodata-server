/**
 * @file objenious_mqttsubscriber.cpp
 * @brief Implementation of the ObjeniousMqttSubscriber class
 * @author Laurent Georget
 * @date 2021-02-23
 */
/*
 * Copyright (C) 2021  SAS JD Environnement <contact@meteo-concept.fr>
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

#include <iostream>
#include <memory>
#include <functional>
#include <iterator>
#include <chrono>
#include <sstream>
#include <exception>
#include <systemd/sd-daemon.h>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <cassandra.h>
#include <dbconnection_observations.h>
#include <mqtt_client_cpp.hpp>

#include "../time_offseter.h"
#include "mqtt_subscriber.h"
#include "objenious_mqtt_subscriber.h"
#include "../objenious/objenious_archive_message_collection.h"

namespace sys = boost::system;

namespace meteodata {

using namespace date;

constexpr char ObjeniousMqttSubscriber::ARCHIVES_TOPIC[];

ObjeniousMqttSubscriber::ObjeniousMqttSubscriber(
		const CassUuid& station, MqttSubscriber::MqttSubscriptionDetails&& details,
		const std::string& objeniousId, const std::map <std::string, std::string>& variables,
		asio::io_service& ioService, DbConnectionObservations& db,
		TimeOffseter::PredefinedTimezone tz
	) :
	MqttSubscriber(station, std::move(details), ioService, db, tz),
	_objeniousId{objeniousId},
	_variables{variables}
{
}

bool ObjeniousMqttSubscriber::handleSubAck(std::uint16_t, std::vector<boost::optional<std::uint8_t>> results) {
	for (auto const& e : results) { /* we are expecting only one */
		if (!e) {
			std::cerr << SD_ERR << _stationName <<  ": subscription failed: " << mqtt::qos::to_str(*e) << std::endl;
			stop();
		}
	}
	return true;
}

void ObjeniousMqttSubscriber::processArchive(const mqtt::string_view& content)
{
	std::cout << SD_DEBUG << "Now downloading for MQTT station " << _stationName << std::endl;

	// TODO: check if there's a more clever way of building a stream out of
	// an iterable range of characters
	std::string jsonString{content.begin(), content.end()};
	std::istringstream input{jsonString};
	pt::ptree jsonTree;
	pt::read_json(input, jsonTree);

	ObjeniousApiArchiveMessage msg{&_variables};
	try {
		msg.ingest(jsonTree);

		int ret = _db.insertV2DataPoint(_station, msg);
		if (ret) {
			std::cout << SD_DEBUG << "Archive data stored\n" << std::endl;
			time_t lastArchiveDownloadTime = msg.getTimestamp().time_since_epoch().count();
			ret = _db.updateLastArchiveDownloadTime(_station, lastArchiveDownloadTime);
			if (!ret)
				std::cerr << SD_ERR << "MQTT station " << _stationName << ": Couldn't update last archive download time"
					<< std::endl;
		} else {
			std::cerr << SD_ERR << "Failed to store archive for MQTT station " << _stationName << "! Aborting"
				<< std::endl;
			return;
		}
	} catch (const std::exception& e) {
		std::cerr << SD_ERR << "Failed to receive or parse an Objenious MQTT message: " << e.what() << std::endl;
	}
}

}
