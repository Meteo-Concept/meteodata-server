/**
 * @file lorain_mqttsubscriber.cpp
 * @brief Implementation of the LorainMqttSubscriber class
 * @author Laurent Georget
 * @date 2022-02-24
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

#include <iostream>
#include <memory>
#include <functional>
#include <iterator>
#include <map>
#include <chrono>
#include <systemd/sd-daemon.h>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <cassandra.h>
#include <date.h>
#include <dbconnection_observations.h>
#include <mqtt_client_cpp.hpp>

#include "../time_offseter.h"
#include "../cassandra_utils.h"
#include "mqtt_subscriber.h"
#include "lorain_mqtt_subscriber.h"
#include "../pessl/lorain_message.h"

namespace sys = boost::system;
namespace chrono = std::chrono;

namespace meteodata
{

LorainMqttSubscriber::LorainMqttSubscriber(MqttSubscriber::MqttSubscriptionDetails details, asio::io_service& ioService,
	 DbConnectionObservations& db) :
		MqttSubscriber(details, ioService, db)
{
}

bool LorainMqttSubscriber::handleSubAck(std::uint16_t packetId, std::vector<boost::optional<std::uint8_t>> results)
{
	for (auto const& e : results) { /* we are expecting only one */
		auto subscriptionIt = _subscriptions.find(packetId);
		if (subscriptionIt == _subscriptions.end()) {
			std::cerr << SD_ERR << "[MQTT Lorain] protocol: " << "client " << _details.host
				<< ": received an invalid subscription ack?!" << std::endl;
			continue;
		}

		const std::string& topic = subscriptionIt->second;
		const auto& station = _stations[topic];
		if (!e) {
			std::cerr << SD_ERR << "[MQTT Lorain " << std::get<1>(station) << "] connection: " << "subscription failed: "
				<< mqtt::qos::to_str(*e) << std::endl;
		}
	}
	return true;
}

void LorainMqttSubscriber::processArchive(const mqtt::string_view& topicName, const mqtt::string_view& content)
{
	using namespace std::chrono;
	using date::operator<<;

	auto stationIt = _stations.find(topicName.to_string());
	if (stationIt == _stations.end()) {
		std::cout << SD_NOTICE << "[MQTT Lorain protocol]: " << "Unknown topic " << topicName << std::endl;
		return;
	}

	const CassUuid& station = std::get<0>(stationIt->second);
	const std::string& stationName = std::get<1>(stationIt->second);
	std::cout << SD_DEBUG << "[MQTT Lorain " << station << "] measurement: " << "Now receiving for MQTT station "
		<< stationName << std::endl;


	time_t lastUpdate;
	int previousClicks;
	bool result = _db.getCachedInt(station, LORAIN_RAINFALL_CACHE_KEY, lastUpdate, previousClicks);

	LorainMessage msg;
	std::optional<int> prev = std::nullopt;
	if (result && system_clock::from_time_t(lastUpdate) > system_clock::now() - 24h) {
		// the last rainfall datapoint is not too old, we can use
		// it as a reference for the current number of clicks recorded
		// by the pluviometer
		prev = previousClicks;
	}

	pt::ptree jsonTree;
	std::istringstream jsonStream{std::string{content}};
	pt::read_json(jsonStream, jsonTree);

	date::sys_seconds timestamp;
	std::string t = jsonTree.get<std::string>("timestamp");
	std::istringstream is{t};
	// don't bother parsing the seconds and subseconds
	is >> date::parse("%Y-%m-%dT%H:%M:", timestamp);

	std::cout << SD_DEBUG << "[MQTT " << station << "] measurement: " << "Data received for timestamp " << timestamp << " (" << t << ")" << std::endl;
	std::string payload = jsonTree.get<std::string>("value.payload");

	msg.ingest(payload, timestamp, prev);

	int ret = false;
	if (msg.looksValid()) {
		ret = _db.insertV2DataPoint(msg.getObservation(station));
	} else {
		std::cerr << SD_WARNING << "[MQTT " << station << "] measurement: "
			<< "Record looks invalid, discarding " << std::endl;
	}

	if (ret) {
		std::cout << SD_DEBUG << "[MQTT " << station << "] measurement: " << "Archive data stored for timestamp " << timestamp << std::endl;
		time_t lastArchiveDownloadTime = timestamp.time_since_epoch().count();
		ret = _db.updateLastArchiveDownloadTime(station, lastArchiveDownloadTime);
		if (!ret)
			std::cerr << SD_ERR << "[MQTT " << station << "] management: "
				<< "Couldn't update last archive download time" << std::endl;


		ret = _db.cacheInt(station, LORAIN_RAINFALL_CACHE_KEY, chrono::system_clock::to_time_t(timestamp), msg.getRainfallClicks());
		if (!ret)
			std::cerr << SD_ERR << "[MQTT " << station << "] management: "
				<< "Couldn't update the rainfall number of clicks, accumulation error possible" << std::endl;


	} else {
		std::cerr << SD_ERR << "[MQTT " << station << "] measurement: " << "Failed to store archive for MQTT station "
			<< stationName << "! Aborting" << std::endl;
		// will retry...
		return;
	}
}

}
