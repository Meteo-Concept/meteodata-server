/**
 * @file vp2_mqttsubscriber.cpp
 * @brief Implementation of the VP2MqttSubscriber class
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
#include <map>
#include <chrono>
#include <sstream>
#include <systemd/sd-daemon.h>

#include <cassandra.h>
#include <dbconnection_observations.h>
#include <mqtt_client_cpp.hpp>

#include "../time_offseter.h"
#include "../cassandra_utils.h"
#include "mqtt_subscriber.h"
#include "vp2_mqtt_subscriber.h"
#include "../davis/vantagepro2_archive_page.h"

namespace sys = boost::system;
namespace chrono = std::chrono;

namespace meteodata
{

using namespace date;

constexpr char VP2MqttSubscriber::ARCHIVES_TOPIC[];

VP2MqttSubscriber::VP2MqttSubscriber(MqttSubscriber::MqttSubscriptionDetails details,
		asio::io_service& ioService, DbConnectionObservations& db) :
	MqttSubscriber(details, ioService, db)
{
}

bool VP2MqttSubscriber::handleSubAck(std::uint16_t packetId, std::vector<boost::optional<std::uint8_t>> results)
{
	for (auto const& e : results) { /* we are expecting only one */
		auto subscriptionIt = _subscriptions.find(packetId);
		if (subscriptionIt == _subscriptions.end()) {
			std::cerr << SD_ERR << "[MQTT] protocol: " << "client " << _details.host
				<< ": received an invalid subscription ack?!" << std::endl;
			continue;
		}

		const std::string& topic = subscriptionIt->second;
		const auto& station = _stations[subscriptionIt->second];
		if (!e) {
			std::cerr << SD_ERR << "[MQTT" << std::get<1>(station) << "] connection: " << "subscription failed: "
				<< mqtt::qos::to_str(*e) << std::endl;
		} else {
			const TimeOffseter& timeOffseter = std::get<4>(station);
			const date::sys_seconds& lastArchive = std::get<3>(station);
			int pollingPeriod = std::get<2>(station);
			// The topic name ought to be vp2/<client>/dmpaft, we can write
			// to vp2/<client> to request the archives
			if (topic.rfind("/dmpaft") == topic.size() - 7) { // ends_with("/dmpaft")
				// Sending the GETTIME command will wake up the scheduler if it's stuck for some reason
				_client->publish_at_least_once(topic.substr(0, topic.size() - 7), "GETTIME");
				if (chrono::system_clock::now() - lastArchive > chrono::minutes(pollingPeriod)) {
					// Fetch all the archives available right now, this will resync the scheduler at the same time
					// The 2h offset is somewhat arbitrary, it prevents missing observations in case of multiple
					// disconnections over short periods of time
					date::local_seconds archiveTime = timeOffseter.convertToLocalTime(lastArchive - chrono::hours(2));
					_client->publish_at_least_once(topic.substr(0, topic.size() - 7),
						date::format("DMPAFT %Y-%m-%d %H:%M", archiveTime));
				}
			}
		}
	}
	return true;
}

void VP2MqttSubscriber::processArchive(const mqtt::string_view& topicName, const mqtt::string_view& content)
{
	auto stationIt = _stations.find(topicName.to_string());
	if (stationIt == _stations.end()) {
		std::cout << SD_NOTICE << "[MQTT protocol]: " << "Unknown topic " << topicName << std::endl;
		return;
	}

	const CassUuid& station = std::get<0>(stationIt->second);
	const std::string& stationName = std::get<1>(stationIt->second);
	const TimeOffseter& timeOffseter = std::get<4>(stationIt->second);
	std::cout << SD_DEBUG << "[MQTT " << station << "] measurement: " << "Now receiving for MQTT station "
		<< stationName << std::endl;

	std::size_t expectedSize = sizeof(VantagePro2ArchiveMessage::ArchiveDataPoint);
	std::size_t receivedSize = content.size();
	if (receivedSize != expectedSize) {
		std::cerr << SD_WARNING << "[MQTT " << station << "] protocol: " << "input from broker has an invalid size "
			<< "(" << receivedSize << " bytes instead of " << expectedSize << ")" << std::endl;
		return;
	}

	VantagePro2ArchiveMessage::ArchiveDataPoint data;
	std::memcpy(&data, content.data(), sizeof(data));
	VantagePro2ArchiveMessage msg{data, &timeOffseter};
	int ret = false;
	if (msg.looksValid()) {
		ret = _db.insertV2DataPoint(msg.getObservation(station));
	} else {
		std::cerr << SD_WARNING << "[MQTT " << station << "] measurement: "
			<< "Record looks invalid, discarding... (for information, timestamp says " << msg.getTimestamp()
			<< " and system clock says " << chrono::system_clock::now() << ")" << std::endl;
	}

	if (ret) {
		std::cout << SD_DEBUG << "[MQTT " << station << "] measurement: " << "Archive data stored" << std::endl;
		time_t lastArchiveDownloadTime = msg.getTimestamp().time_since_epoch().count();
		ret = _db.updateLastArchiveDownloadTime(station, lastArchiveDownloadTime);
		if (!ret)
			std::cerr << SD_ERR << "[MQTT " << station << "] management: "
				<< "Couldn't update last archive download time" << std::endl;
	} else {
		std::cerr << SD_ERR << "[MQTT " << station << "] measurement: " << "Failed to store archive for MQTT station "
			<< stationName << "! Aborting" << std::endl;
		// will retry...
		return;
	}

	// about twice a day, set the clock (it seems very frequent, but it doesn't matter)
	using namespace date;
	date::sys_seconds now = date::floor<chrono::seconds>(chrono::system_clock::now());
	// The topic name ought to be vp2/<client>/dmpaft, we can write
	// to vp2/<client> to send the SETTIME command
	if (topicName.rfind("/dmpaft") == topicName.size() - 7) { // ends_with("/dmpaft")
		std::string topic{topicName.substr(0, topicName.size() - 7)};
		if (_clockResetTimes[topic] + chrono::hours(12) < now) {
			if (timeOffseter.usesUTC()) {
				std::cerr << SD_INFO << "[MQTT " << station << "] protocol: " << "Setting the station clock to "
					  << date::format("%Y-%m-%d %H:%M:%S+0000", now) << std::endl;
				_client->publish_at_least_once(topic, date::format("SETTIME %Y-%m-%d %H:%M:%S", now));
			} else {
				std::cerr << SD_INFO << "[MQTT " << station << "] protocol: "
					  << "Setting the station clock to the Raspberry Pi current time" << std::endl;
				_client->publish_at_least_once(topic, "SETTIME");
			}
		}
		_clockResetTimes[topic] = now;
	}
}

}
