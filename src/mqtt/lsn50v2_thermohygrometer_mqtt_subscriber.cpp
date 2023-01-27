/**
 * @file lsn50v2_thermohygrometer_mqttsubscriber.cpp
 * @brief Implementation of the Lsn50v2ThermohygrometerMqttSubscriber class
 * @author Laurent Georget
 * @date 2023-01-27
 */
/*
 * Copyright (C) 2023  SAS JD Environnement <contact@meteo-concept.fr>
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
#include <iterator>
#include <chrono>
#include <systemd/sd-daemon.h>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <cassandra.h>
#include <date.h>
#include <dbconnection_observations.h>
#include <mqtt_client_cpp.hpp>
#include <utility>

#include "../time_offseter.h"
#include "../cassandra_utils.h"
#include "mqtt_subscriber.h"
#include "lsn50v2_thermohygrometer_mqtt_subscriber.h"
#include "../dragino/lsn50v2_thermohygrometer_message.h"
#include "liveobjects_mqtt_subscriber.h"

namespace sys = boost::system;
namespace chrono = std::chrono;

namespace meteodata
{

Lsn50v2ThermohygrometerMqttSubscriber::Lsn50v2ThermohygrometerMqttSubscriber(MqttSubscriber::MqttSubscriptionDetails details, asio::io_context& ioContext,
	 DbConnectionObservations& db) :
		LiveobjectsMqttSubscriber(std::move(details), ioContext, db)
{
}

void Lsn50v2ThermohygrometerMqttSubscriber::processArchive(const mqtt::string_view& topicName, const mqtt::string_view& content)
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
	auto t = jsonTree.get<std::string>("timestamp");
	std::istringstream is{t};
	// don't bother parsing the seconds and subseconds
	is >> date::parse("%Y-%m-%dT%H:%M:", timestamp);
	auto payload = jsonTree.get<std::string>("value.payload");

	Lsn50v2ThermohygrometerMessage msg;
	msg.ingest(payload, timestamp);

	int ret = false;
	if (msg.looksValid()) {
		ret = _db.insertV2DataPoint(msg.getObservation(station));
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
	} else {
		std::cerr << SD_ERR << "[MQTT Liveobjects " << station << "] measurement: "
				  << "Failed to store archive for MQTT station " << stationName << "! Aborting" << std::endl;
		// will retry...
		return;
	}
}

std::unique_ptr<LiveobjectsMessage> Lsn50v2ThermohygrometerMqttSubscriber::buildMessage(const boost::property_tree::ptree& json, const CassUuid& station, date::sys_seconds& timestamp)
{
	std::unique_ptr<Lsn50v2ThermohygrometerMessage> m = std::make_unique<Lsn50v2ThermohygrometerMessage>();

	auto payload = json.get<std::string>("value.payload");
	m->ingest(payload, timestamp);
	return m;
}

}
