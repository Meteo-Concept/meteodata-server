/**
 * @file chirpstack_mqtt_subscriber.cpp
 * @brief Implementation of the ChirpstackMqttSubscriber class
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

#include <mqtt_client_cpp.hpp>
#include <dbconnection_observations.h>
#include <date.h>
#include <cassandra.h>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <systemd/sd-daemon.h>
#include <openssl/evp.h>

#include "mqtt_subscriber.h"
#include "chirpstack_mqtt_subscriber.h"
#include "liveobjects/liveobjects_message.h"
#include "cassandra_utils.h"

namespace meteodata
{
namespace pt = boost::property_tree;

ChirpstackMqttSubscriber::ChirpstackMqttSubscriber(const MqttSubscriber::MqttSubscriptionDetails& details,
	asio::io_context& ioContext, DbConnectionObservations& db, AsyncJobPublisher* jobPublisher) :
		MqttSubscriber{details, ioContext, db, jobPublisher}
{
}

bool ChirpstackMqttSubscriber::handleSubAck(std::uint16_t packetId, std::vector<boost::optional<std::uint8_t>> results)
{
	for (auto const& e : results) { /* we are expecting only one */
		auto subscriptionIt = _subscriptions.find(packetId);
		if (subscriptionIt == _subscriptions.end()) {
			std::cerr << SD_ERR << "[MQTT Chirpstack] protocol: " << "client " << _details.host
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

void ChirpstackMqttSubscriber::processArchive(const mqtt::string_view& topicName, const mqtt::string_view& content)
{
	using date::operator<<;

	auto stationIt = _stations.find(topicName.to_string());
	if (stationIt == _stations.end()) {
		std::cout << SD_NOTICE << "[MQTT protocol]: " << "Unknown topic " << topicName << std::endl;
		return;
	}

	const CassUuid& station = std::get<0>(stationIt->second);
	const std::string& stationName = std::get<1>(stationIt->second);
	std::cout << SD_DEBUG << "[MQTT Chirpstack " << station << "] measurement: " << "Now receiving for MQTT station "
			  << stationName << std::endl;

	pt::ptree jsonTree;
	std::istringstream jsonStream{std::string{content}};
	pt::read_json(jsonStream, jsonTree);

	date::sys_seconds timestamp;
	auto msg = buildMessage(jsonTree, station, timestamp);

	int ret = false;
	if (msg && msg->looksValid()) {
		ret = _db.insertV2DataPoint(msg->getObservation(station));
	} else {
		std::cerr << SD_WARNING << "[MQTT Chirpstack " << station << "] measurement: "
				  << "Record looks invalid, discarding " << std::endl;
	}

	if (ret) {
		std::cout << SD_DEBUG << "[MQTT Chirpstack " << station << "] measurement: "
				  << "Archive data stored for timestamp " << timestamp << std::endl;
		time_t lastArchiveDownloadTime = timestamp.time_since_epoch().count();
		ret = _db.updateLastArchiveDownloadTime(station, lastArchiveDownloadTime);
		if (!ret)
			std::cerr << SD_ERR << "[MQTT Chirpstack " << station << "] management: "
					  << "Couldn't update last archive download time" << std::endl;

		if (_jobPublisher)
			_jobPublisher->publishJobsForPastDataInsertion(station, timestamp, timestamp);

		msg->cacheValues(station);
	} else {
		std::cerr << SD_ERR << "[MQTT Chirpstack " << station << "] measurement: "
				  << "Failed to store archive for MQTT station " << stationName << "! Aborting" << std::endl;
		// will retry...
		return;
	}
}

std::unique_ptr<LiveobjectsMessage> ChirpstackMqttSubscriber::buildMessage(const boost::property_tree::ptree& json, const CassUuid& station, date::sys_seconds& timestamp)
{
	auto sensor = json.get<std::string>("deviceInfo.tags.sensors", "");
	auto b64payload = json.get<std::string>("data");
	auto port = json.get<int>("fPort", -1);

	std::unique_ptr<EVP_ENCODE_CTX, typeof(&EVP_ENCODE_CTX_free)> ctx{EVP_ENCODE_CTX_new(), &EVP_ENCODE_CTX_free};
	EVP_DecodeInit(ctx.get());
	const auto expectedSize = 3 * b64payload.length() / 4;
	std::vector<uint8_t> output(expectedSize+1);
	int outputSize = 0;
	int result = EVP_DecodeUpdate(ctx.get(), output.data(), &outputSize, reinterpret_cast<const unsigned char*>(b64payload.data()), b64payload.length());
	if (result == -1) {
		std::cerr << SD_ERR << "[Chirpstack " << station << "] protocol: "
			  << "Decoding failed" << std::endl;
		return {};
	}
	output.resize(outputSize);
	result = EVP_DecodeFinal(ctx.get(), output.data() + output.size(), &outputSize);
	if (result == -1) {
		std::cerr << SD_ERR << "[Chirpstack " << station << "] protocol: "
			  << "Decoding failed" << std::endl;
		return {};
	}
	std::ostringstream os;
	os << std::hex << std::setfill('0');
	for (uint8_t o : output) {
		os << std::setw(2) << unsigned(o);
	}
	std::string payload = os.str();

	std::unique_ptr<LiveobjectsMessage> m = LiveobjectsMessage::instantiateMessage(_db, sensor, port, station);

	if (!m) {
		std::cerr << SD_ERR << "[Chirpstack " << station << "] protocol: "
			  << "Misconfigured sensor, unknown sensor type! Aborting." << std::endl;
		return {};
	}

	auto t = json.get<std::string>("time");
	std::istringstream is{t};
	// don't bother parsing the subseconds
	is >> date::parse("%Y-%m-%dT%H:%M:%S", timestamp);

	using namespace date;
	std::cout << SD_DEBUG << "Parsing message with timestamp " << timestamp << std::endl;

	m->ingest(station, payload, timestamp);
	return m;
}

}
