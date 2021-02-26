/**
 * @file mqttsubscriber.cpp
 * @brief Implementation of the MqttSubscriber class
 * @author Laurent Georget
 * @date 2018-01-10
 */
/*
 * Copyright (C) 2019  SAS JD Environnement <contact@meteo-concept.fr>
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
#include <sstream>
#include <memory>
#include <functional>
#include <iterator>
#include <chrono>
#include <systemd/sd-daemon.h>
#include <unistd.h>

#include <cassandra.h>
#include <dbconnection_observations.h>
#include <mqtt_client_cpp.hpp>

#include "../time_offseter.h"
#include "../cassandra_utils.h"
#include "mqtt_subscriber.h"
#include "../davis/vantagepro2_archive_page.h"

#define DEFAULT_VERIFY_PATH "/etc/ssl/certs"

namespace sys = boost::system;

namespace meteodata {

using namespace date;

constexpr char MqttSubscriber::CLIENT_ID[];

MqttSubscriber::MqttSubscriptionDetails::MqttSubscriptionDetails(const std::string& host, int port, const std::string& user, std::unique_ptr<char[]>&& password, size_t passwordLength, const std::string& topic) :
	host(host),
	port(port),
	user(user),
	password(std::move(password)),
	passwordLength(passwordLength),
	topic(topic)
{}

MqttSubscriber::MqttSubscriptionDetails::MqttSubscriptionDetails(MqttSubscriber::MqttSubscriptionDetails&& other) :
	host(std::move(other.host)),
	port(other.port),
	user(std::move(other.user)),
	password(std::move(other.password)),
	passwordLength(other.passwordLength),
	topic(std::move(other.topic))
{}

MqttSubscriber::MqttSubscriber(const CassUuid& station, MqttSubscriber::MqttSubscriptionDetails&& details,
	asio::io_service& ioService, DbConnectionObservations& db, TimeOffseter::PredefinedTimezone tz) :
	_ioService(ioService),
	_db(db),
	_station(station),
	_details(std::move(details))
{
	time_t lastArchiveDownloadTime;
	db.getStationDetails(station, _stationName, _pollingPeriod, lastArchiveDownloadTime);
	_lastArchive = date::sys_seconds(chrono::seconds(lastArchiveDownloadTime));

	float latitude;
	float longitude;
	int elevation;
	std::string stationName;
	int pollingPeriod;
	db.getStationCoordinates(station, latitude, longitude, elevation, stationName, pollingPeriod);

	_timeOffseter = TimeOffseter::getTimeOffseterFor(tz);
	_timeOffseter.setLatitude(latitude);
	_timeOffseter.setLongitude(longitude);
	_timeOffseter.setElevation(elevation);
	_timeOffseter.setMeasureStep(_pollingPeriod);
	std::cout << SD_NOTICE << "Discovered MQTT station " << _stationName << std::endl;
}

bool MqttSubscriber::handleConnAck(bool, uint8_t) {
	return true;
}

void MqttSubscriber::handleClose() {
}

void MqttSubscriber::handleError(sys::error_code const&) {
}

bool MqttSubscriber::handlePubAck(uint16_t) {
	return true;
}

bool MqttSubscriber::handlePubRec(uint16_t) {
	return true;
}

bool MqttSubscriber::handlePubComp(uint16_t) {
	return true;
}

bool MqttSubscriber::handleSubAck(uint16_t, std::vector<boost::optional<std::uint8_t>>) {
	return true;
}

bool MqttSubscriber::handlePublish(std::uint8_t,
		boost::optional<std::uint16_t>,
		mqtt::string_view,
		mqtt::string_view contents) {
	processArchive(contents);
	return true;
}

void MqttSubscriber::start()
{
	std::cout << SD_DEBUG << "About to start the MQTT client for " << _stationName << std::endl;
	_client = mqtt::make_tls_client(_ioService, _details.host, _details.port);

	std::ostringstream clientId;
	clientId << MqttSubscriber::CLIENT_ID << "." << _station;
	_client->set_client_id(clientId.str());
	_client->set_user_name(_details.user);
	_client->set_password(_details.password.get());
	_client->set_clean_session(false); /* this way, we can catch up on missed packets upon reconnection */
	_client->add_verify_path(DEFAULT_VERIFY_PATH);
	std::cout << SD_DEBUG << "Created the client" << std::endl;

	auto self{shared_from_this()};
	_client->set_connack_handler(
		[this,self](bool sp, std::uint8_t ret) {
			std::cout << SD_DEBUG << "Connection attempt to " << _details.host << " for station " << _stationName << ": " << mqtt::connect_return_code_to_str(ret) << std::endl;
			if (ret == mqtt::connect_return_code::accepted) {
				std::cerr << SD_NOTICE << "Connection established to " << _details.host << " for station " << _stationName << ": " << mqtt::connect_return_code_to_str(ret) << std::endl;
				_pid = _client->subscribe(_details.topic, mqtt::qos::at_least_once);
				return handleConnAck(sp, ret);
			} else {
				std::cerr << SD_ERR << "Failed to establish connection to " << _details.host << " for station " << _stationName << ": " << mqtt::connect_return_code_to_str(ret) << std::endl;
			}
			return true;
		}
	);
	_client->set_close_handler(
		[this,self]() {
			std::cerr << SD_NOTICE << "MQTT station " << _stationName << " disconnected" << std::endl;
			handleClose();
		}
	);
	_client->set_error_handler(
		[this,self](sys::error_code const& ec) {
			std::cerr << SD_ERR << "MQTT station " << _stationName << ": unexpected disconnection " << ec.message() << std::endl;
			handleError(ec);
		}
	);
	_client->set_puback_handler(
		[this,self]([[maybe_unused]] std::uint16_t packetId) {
			return handlePubAck(packetId);
		}
	);
	_client->set_pubrec_handler(
		[this,self]([[maybe_unused]] std::uint16_t packetId) {
			return handlePubRec(packetId);
		}
	);
	_client->set_pubcomp_handler(
		[this,self]([[maybe_unused]] std::uint16_t packetId) {
			return handlePubComp(packetId);
		}
	);
	_client->set_suback_handler(
		[this,self](std::uint16_t packetId, std::vector<boost::optional<std::uint8_t>> results) {
			return handleSubAck(packetId, results);
		}
	);
	_client->set_publish_handler(
		[this,self](std::uint8_t header,
			boost::optional<std::uint16_t> packetId,
			mqtt::string_view topic,
			mqtt::string_view contents) {
			return handlePublish(header, packetId, topic, contents);
		}
	);
	std::cout << SD_DEBUG << "Set the handlers" << std::endl;

	_client->connect();
}

void MqttSubscriber::stop()
{
	_client->disconnect();
}

}
