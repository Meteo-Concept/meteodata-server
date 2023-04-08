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

#include <chrono>
#include <map>
#include <iterator>
#include <functional>
#include <memory>
#include <iostream>

#include <mqtt_client_cpp.hpp>
#include <dbconnection_observations.h>
#include <date.h>
#include <cassandra.h>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <systemd/sd-daemon.h>

#include "mqtt_subscriber.h"
#include "liveobjects_mqtt_subscriber.h"
#include "cassandra_utils.h"
#include "dragino/cpl01_pluviometer_message.h"
#include "dragino/lsn50v2_thermohygrometer_message.h"

namespace meteodata
{

LiveobjectsMqttSubscriber::LiveobjectsMqttSubscriber(const MqttSubscriber::MqttSubscriptionDetails& details,
		asio::io_context& ioContext, DbConnectionObservations& db) :
		MqttSubscriber(details, ioContext, db)
{
}

bool LiveobjectsMqttSubscriber::handleConnAck(bool res, uint8_t)
{
	std::string topic = getTopic();
	_subscriptions[_client->subscribe(topic, mqtt::qos::at_least_once)] = topic;
	return true;
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

		if (!e) {
			std::cerr << SD_ERR << "[MQTT Liveobjects " << getTopic() << "] connection: "
					  << "subscription failed: " << mqtt::qos::to_str(*e) << std::endl;
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
	const std::string& streamId = jsonTree.get<std::string>("streamId");

	auto stationIt = _stations.find(streamId);
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
	if (msg && msg->looksValid()) {
		ret = _db.insertV2DataPoint(msg->getObservation(station));
	} else {
		std::cerr << SD_WARNING << "[MQTT Liveobjects " << station << "] measurement: "
				  << "Record looks invalid, discarding " << std::endl;
	}

	if (ret) {
		std::cout << SD_DEBUG << "[MQTT Liveobjects " << station << "] measurement: "
				  << "Archive data stored for timestamp " << timestamp << std::endl;
		time_t lastArchiveDownloadTime = timestamp.time_since_epoch().count();
		ret = _db.updateLastArchiveDownloadTime(station, lastArchiveDownloadTime);
		if (!ret)
			std::cerr << SD_ERR << "[MQTT Liveobjects " << station << "] management: "
					  << "Couldn't update last archive download time" << std::endl;


		postInsert(station, msg);
	} else {
		std::cerr << SD_ERR << "[MQTT Liveobjects " << station << "] measurement: "
				  << "Failed to store archive for MQTT station " << stationName << "! Aborting" << std::endl;
		// will retry...
		return;
	}
}

void LiveobjectsMqttSubscriber::postInsert(const CassUuid& station, const std::unique_ptr<LiveobjectsMessage>& msg)
{
	msg->cacheValues(station);
}

std::unique_ptr<LiveobjectsMessage> LiveobjectsMqttSubscriber::buildMessage(const boost::property_tree::ptree& json, const CassUuid& station, date::sys_seconds& timestamp)
{
	auto sensor = json.get<std::string>("extra.sensors", "");
	auto payload = json.get<std::string>("value.payload");
	auto port = json.get<int>("metadata.network.lora.port", -1);

	std::unique_ptr<LiveobjectsMessage> m;
	if (sensor == "dragino-cpl01-pluviometer" && port == 2) {
		m = std::make_unique<Cpl01PluviometerMessage>(_db);
	} else if (sensor == "dragino-lsn50v2" && port == 2) {
		m = std::make_unique<Lsn50v2ThermohygrometerMessage>();
	}

	if (!m) {
		std::cerr << SD_ERR << "[MQTT Liveobjects " << station << "] protocol: "
				  << "Misconfigured sensor, unknown sensor type! Aborting." << std::endl;
		return {};
	}

	if (payload.length() > 6) {
		// This is a group of measurements, only return the first packet in the group.
		payload = payload.substr(1, 6);
	}
	m->ingest(station, payload, timestamp);
	return m;
}

void LiveobjectsMqttSubscriber::addStation(const std::string& topic, const CassUuid& station,
	TimeOffseter::PredefinedTimezone tz, const std::string& streamId)
{
	// Use the stream id instead of the topic, all the stations message go to the same topic
	MqttSubscriber::addStation(streamId, station, tz);
}

}
