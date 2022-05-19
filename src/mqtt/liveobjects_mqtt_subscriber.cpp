/**
 * @file liveobjects_mqtt_subscriber.cpp
 * @brief Implementation of the LiveobjectsMqttSubscriber class
 * @author Laurent Georget
 * @date 2022-04-28
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

#include "../pessl/lorain_message.h"
#include "lorain_mqtt_subscriber.h"
#include "mqtt_subscriber.h"
#include "../cassandra_utils.h"
#include "../time_offseter.h"
#include <mqtt_client_cpp.hpp>
#include <dbconnection_observations.h>
#include <date.h>
#include <cassandra.h>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <systemd/sd-daemon.h>
#include <chrono>
#include <map>
#include <iterator>
#include <functional>
#include <memory>
#include <iostream>
#include "liveobjects_mqtt_subscriber.h"

namespace meteodata {

LiveobjectsMqttSubscriber::LiveobjectsMqttSubscriber(MqttSubscriber::MqttSubscriptionDetails details, asio::io_service& ioService,
	 DbConnectionObservations& db) :
		MqttSubscriber(details, ioService, db)
{
}

bool LiveobjectsMqttSubscriber::handleSubAck(std::uint16_t packetId, std::vector<boost::optional<std::uint8_t>> results)
{
	for (auto const& e : results) { /* we are expecting only one */
		auto subscriptionIt = _subscriptions.find(packetId);
		if (subscriptionIt == _subscriptions.end()) {
			std::cerr << SD_ERR << "[MQTT Liveobjects] protocol: " << "client " << _details.host
				<< ": received an invalid subscription ack?!" << std::endl;
			continue;
		}

		const std::string& topic = subscriptionIt->second;
		const auto& station = _stations[topic];
		if (!e) {
			std::cerr << SD_ERR << "[MQTT Liveobjects " << std::get<1>(station) << "] connection: " << "subscription failed: "
				<< mqtt::qos::to_str(*e) << std::endl;
		}
	}
	return true;
}

void LiveobjectsMqttSubscriber::processArchive(const mqtt::string_view& topicName, const mqtt::string_view& content)
{
	using date::operator<<;

	pt::ptree jsonTree;
	std::istringstream jsonStream{std::string{content}};
	pt::read_json(jsonStream, jsonTree);
	std::string streamId = jsonTree.get<std::string>("streamId");

	auto stationIt = _stations.find(topicName.to_string());
	if (stationIt == _stations.end()) {
		std::cout << SD_NOTICE << "[MQTT Liveobjects] protocol: " << "Unknown stream id " << streamId << std::endl;
		return;
	}

	const CassUuid& station = std::get<0>(stationIt->second);
	const std::string& stationName = std::get<1>(stationIt->second);
	std::cout << SD_DEBUG << "[MQTT Liveobjects " << station << "] measurement: " << "Now receiving for MQTT station "
		<< stationName << std::endl;


	date::sys_seconds timestamp;
	std::unique_ptr<LiveobjectsMessage> msg = buildMessage(jsonTree, station, timestamp);

	int ret = false;
	if (msg->looksValid()) {
		ret = _db.insertV2DataPoint(msg->getObservation(station));
	} else {
		std::cerr << SD_WARNING << "[MQTT Liveobjects " << station << "] measurement: "
			<< "Record looks invalid, discarding " << std::endl;
	}

	if (ret) {
		std::cout << SD_DEBUG << "[MQTT Liveobjects " << station << "] measurement: " << "Archive data stored for timestamp " << timestamp << std::endl;
		time_t lastArchiveDownloadTime = timestamp.time_since_epoch().count();
		ret = _db.updateLastArchiveDownloadTime(station, lastArchiveDownloadTime);
		if (!ret)
			std::cerr << SD_ERR << "[MQTT Liveobjects " << station << "] management: "
				<< "Couldn't update last archive download time" << std::endl;


		postInsert(station, msg);
	} else {
		std::cerr << SD_ERR << "[MQTT Liveobjects " << station << "] measurement: " << "Failed to store archive for MQTT station "
			<< stationName << "! Aborting" << std::endl;
		// will retry...
		return;
	}
}

void LiveobjectsMqttSubscriber::postInsert(const CassUuid& station, const std::unique_ptr<LiveobjectsMessage>& msg)
{
	// no-op
}

}
