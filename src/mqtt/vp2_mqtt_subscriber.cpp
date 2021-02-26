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
#include <chrono>
#include <sstream>
#include <systemd/sd-daemon.h>

#include <cassandra.h>
#include <dbconnection_observations.h>
#include <mqtt_client_cpp.hpp>

#include "../time_offseter.h"
#include "mqtt_subscriber.h"
#include "vp2_mqtt_subscriber.h"
#include "../davis/vantagepro2_archive_page.h"

namespace sys = boost::system;

namespace meteodata {

using namespace date;

constexpr char VP2MqttSubscriber::ARCHIVES_TOPIC[];

VP2MqttSubscriber::VP2MqttSubscriber(const CassUuid& station, MqttSubscriber::MqttSubscriptionDetails&& details,
	asio::io_service& ioService, DbConnectionObservations& db, TimeOffseter::PredefinedTimezone tz) :
	MqttSubscriber(station, std::move(details), ioService, db, tz)
{
}

bool VP2MqttSubscriber::handleSubAck(std::uint16_t packetId, std::vector<boost::optional<std::uint8_t>> results) {
	for (auto const& e : results) { /* we are expecting only one */
		if (!e) {
			std::cerr << _stationName <<  ": subscription failed: " << mqtt::qos::to_str(*e) << std::endl;
			stop();
		}
	}
	if (packetId == _pid) {
		if (chrono::system_clock::now() - _lastArchive > chrono::minutes(_pollingPeriod))
			_client->publish_at_least_once(_details.topic, date::format("DMPAFT %Y-%m-%d %H:%M", _lastArchive));
	}
	return true;
}

void VP2MqttSubscriber::processArchive(const mqtt::string_view& content)
{
	std::cout << SD_DEBUG << "Now downloading for MQTT station " << _stationName << std::endl;

	std::size_t expectedSize = sizeof(VantagePro2ArchiveMessage::ArchiveDataPoint);
	std::size_t receivedSize = content.size();
	if (receivedSize != expectedSize) {
		std::cerr << SD_WARNING << "MQTT station " << _stationName << ": input from broker has an invalid size "
		    << "(" << receivedSize << " byts instead of " << expectedSize << ")" << std::endl;
		return;
	}

	VantagePro2ArchiveMessage::ArchiveDataPoint data;
	std::memcpy(&data, content.data(), sizeof(data));
	VantagePro2ArchiveMessage msg{data, &_timeOffseter};
	int ret = false;
	if (msg.looksValid()) {
		// Do not bother in inserting v1 data points
		ret = _db.insertV2DataPoint(_station, msg);
	} else {
		std::cerr << SD_WARNING << "Record looks invalid, discarding... (for information, timestamp says " << msg.getTimestamp() << " and system clock says " << chrono::system_clock::now() << ")" << std::endl;
	}
	if (ret) {
		std::cout << SD_DEBUG << "Archive data stored\n" << std::endl;
		time_t lastArchiveDownloadTime = msg.getTimestamp().time_since_epoch().count();
		ret = _db.updateLastArchiveDownloadTime(_station, lastArchiveDownloadTime);
		if (!ret)
			std::cerr << SD_ERR << "MQTT station " << _stationName << ": Couldn't update last archive download time" << std::endl;
	} else {
		std::cerr << SD_ERR << "Failed to store archive for MQTT station " << _stationName << "! Aborting" << std::endl;
		// will retry...
		return;
	}
}

}
