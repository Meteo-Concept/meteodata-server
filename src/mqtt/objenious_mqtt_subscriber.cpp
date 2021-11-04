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
#include "../cassandra_utils.h"
#include "mqtt_subscriber.h"
#include "objenious_mqtt_subscriber.h"
#include "../objenious/objenious_archive_message_collection.h"

namespace sys = boost::system;

namespace meteodata {

using namespace date;

constexpr char ObjeniousMqttSubscriber::ARCHIVES_TOPIC[];

ObjeniousMqttSubscriber::ObjeniousMqttSubscriber(
		 MqttSubscriber::MqttSubscriptionDetails details,
		asio::io_service& ioService, DbConnectionObservations& db
	):
	MqttSubscriber(details, ioService, db)
{
}

bool ObjeniousMqttSubscriber::handleSubAck(std::uint16_t packetId, std::vector<boost::optional<std::uint8_t>> results) {
    for (auto const& e : results) { /* we are expecting only one */
        auto subscriptionIt = _subscriptions.find(packetId);
        if (subscriptionIt == _subscriptions.end()) {
            std::cerr << SD_ERR << "[MQTT] protocol: "
                << "client " << _details.host << " received an invalid subscription ack?!" << std::endl;
            continue;
        }

        const std::string& topic = subscriptionIt->second;
        if (!e) {
            std::cerr << SD_ERR << "[MQTT] protocol: "
                << "subscription to topic " << topic << " failed: " << mqtt::qos::to_str(*e) << std::endl;
        }
    }
    return true;
}

void ObjeniousMqttSubscriber::processArchive(const mqtt::string_view& topicName, const mqtt::string_view& content)
{
    auto stationIt = _stations.find(topicName.to_string());
    if (stationIt == _stations.end()) {
        std::cout << SD_NOTICE << "[MQTT] protocol: "
            << "Unknown topic " << topicName << std::endl;
        return;
    }

    const CassUuid& station = std::get<0>(stationIt->second);
    const std::string& stationName = std::get<1>(stationIt->second);
    // no need to check for presence of the topic, that map is filled in at the
    // same time at the other one
    const auto& variables = std::get<1>(_devices[topicName.to_string()]);

    std::cout << SD_INFO << "[MQTT " << station << "] measurement: "
        << "Now downloading for MQTT station " << stationName << std::endl;

	// TODO: check if there's a more clever way of building a stream out of
	// an iterable range of characters
	std::string jsonString{content.begin(), content.end()};
	std::istringstream input{jsonString};
	pt::ptree jsonTree;
	pt::read_json(input, jsonTree);

	ObjeniousApiArchiveMessage msg{&variables};
	try {
		msg.ingest(jsonTree);

		int ret = _db.insertV2DataPoint(station, msg);
		if (ret) {
			std::cout << SD_DEBUG << "[MQTT " << station << "] measurement: "
			    << "Archive data stored\n" << std::endl;
			time_t lastArchiveDownloadTime = msg.getTimestamp().time_since_epoch().count();
			ret = _db.updateLastArchiveDownloadTime(station, lastArchiveDownloadTime);
			if (!ret)
				std::cerr << SD_ERR << "[MQTT " << station << "] management: "
				    << "Couldn't update last archive download time for station " << stationName
					<< std::endl;
		} else {
			std::cerr << SD_ERR << "[MQTT " << station << "] measurement: "
			    << "Failed to store archive for MQTT station " << stationName << "! Aborting"
				<< std::endl;
			return;
		}
	} catch (const std::exception& e) {
		std::cerr << SD_ERR << "[MQTT " << station << "] protocol: "
		    << "Failed to receive or parse an Objenious MQTT message: " << e.what() << std::endl;
	}
}

void ObjeniousMqttSubscriber::addStation(const std::string& topic, const CassUuid& station, TimeOffseter::PredefinedTimezone tz,
                                     const std::string& objeniousId, const std::map<std::string, std::string>& variables) {
    MqttSubscriber::addStation(topic, station, tz);
    _devices.emplace(topic, std::make_tuple(objeniousId, variables));
}

}
