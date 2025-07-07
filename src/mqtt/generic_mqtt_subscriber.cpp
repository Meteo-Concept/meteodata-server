/**
 * @file generic_mqtt_subscriber.cpp
 * @brief Implementation of the GenericMqttSubscriber class
 * @author Laurent Georget
 * @date 2023-06-16
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

#include <map>
#include <iterator>
#include <functional>
#include <iostream>
#include <mutex>

#include <mqtt_client_cpp.hpp>
#include <cassobs/dbconnection_observations.h>
#include <date/date.h>
#include <cassandra.h>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <systemd/sd-daemon.h>

#include "mqtt_subscriber.h"
#include "generic_mqtt_subscriber.h"
#include "generic_message.h"
#include "cassandra_utils.h"

namespace meteodata
{
namespace pt = boost::property_tree;

GenericMqttSubscriber::GenericMqttSubscriber(const MqttSubscriber::MqttSubscriptionDetails& details,
	asio::io_context& ioContext, DbConnectionObservations& db, AsyncJobPublisher* jobPublisher) :
		MqttSubscriber{details, ioContext, db, jobPublisher}
{
}

bool GenericMqttSubscriber::handleSubAck(std::uint16_t packetId, std::vector<boost::optional<std::uint8_t>> results)
{
	for (auto const& e : results) { /* we are expecting only one */
		auto subscriptionIt = _subscriptions.find(packetId);
		if (subscriptionIt == _subscriptions.end()) {
			std::cerr << SD_ERR << "[MQTT Generic] protocol: " << "client " << _details.host
					  << ": received an invalid subscription ack?!" << std::endl;
			continue;
		}

		const std::string& topic = subscriptionIt->second;
		if (!e) {
			std::cerr << SD_ERR << "[MQTT] protocol: " << "subscription to topic " << topic << " failed: "
					  << mqtt::qos::to_str(*e) << std::endl;
		}
	}
	return true;
}

void GenericMqttSubscriber::processArchive(const mqtt::string_view& topicName, const mqtt::string_view& content)
{
	using date::operator<<;

	std::lock_guard<std::mutex> lock{_stationsMutex};

	auto stationIt = _stations.find(topicName.to_string());
	if (stationIt == _stations.end()) {
		std::cout << SD_NOTICE << "[MQTT protocol]: " << "Unknown topic " << topicName << std::endl;
		return;
	}

	const CassUuid& station = std::get<0>(stationIt->second);
	const std::string& stationName = std::get<1>(stationIt->second);
	std::cout << SD_DEBUG << "[MQTT Generic " << station << "] measurement: " << "Now receiving for MQTT station "
			  << stationName << std::endl;

	pt::ptree jsonTree;
	std::istringstream jsonStream{std::string{content}};
	pt::read_json(jsonStream, jsonTree);

	date::sys_seconds timestamp;
	GenericMessage msg = buildMessage(jsonTree, station, timestamp);

	int ret = false;
	if (msg.looksValid()) {
		Observation o = msg.getObservation(station);
		ret = _db.insertV2DataPoint(o) && _db.insertV2DataPointInTimescaleDB(o);
	} else {
		std::cerr << SD_WARNING << "[MQTT Generic " << station << "] measurement: "
				  << "Record looks invalid, discarding " << std::endl;
	}

	if (ret) {
		std::cout << SD_INFO << "[MQTT Generic " << station << "] measurement: "
				  << "Archive data stored for timestamp " << timestamp << std::endl;
		time_t lastArchiveDownloadTime = timestamp.time_since_epoch().count();
		ret = _db.updateLastArchiveDownloadTime(station, lastArchiveDownloadTime);
		if (!ret)
			std::cerr << SD_ERR << "[MQTT Generic " << station << "] management: "
					  << "Couldn't update last archive download time" << std::endl;

		if (_jobPublisher)
			_jobPublisher->publishJobsForPastDataInsertion(station, timestamp, timestamp);

		msg.cacheValues(station);
	} else {
		std::cerr << SD_ERR << "[MQTT Generic " << station << "] measurement: "
				  << "Failed to store archive for MQTT station " << stationName << "! Aborting" << std::endl;
		// will retry...
		return;
	}
}

GenericMessage GenericMqttSubscriber::buildMessage(const boost::property_tree::ptree& json, const CassUuid& station, date::sys_seconds& timestamp)
{
	return GenericMessage::buildMessage(_db, json, timestamp);
}


void GenericMqttSubscriber::reload()
{
	_client->disconnect();
	if (!_stopped) {
		std::lock_guard<std::mutex> lock{_stationsMutex};
		std::vector<std::tuple<CassUuid, std::string, int, std::string, std::unique_ptr<char[]>, size_t, std::string, int>> mqttStations;
		_db.getMqttStations(mqttStations);

		_stations.clear();
		for (auto&& station : mqttStations) {
			const CassUuid& uuid = std::get<0>(station);
			const std::string& topic = std::get<6>(station);
			TimeOffseter::PredefinedTimezone tz{std::get<7>(station)};

			if (topic.substr(0, 8) == "generic/") {
				MqttSubscriber::MqttSubscriptionDetails details{
					std::get<1>(station), std::get<2>(station),
					std::get<3>(station),
					std::string(std::get<4>(station).get(), std::get<5>(station))
				};

				if (_details == details) {
					addStation(topic, uuid, tz);
				}
			}
		}

		start();
	}
}

}
