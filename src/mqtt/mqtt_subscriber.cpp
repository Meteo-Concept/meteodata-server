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
#include <memory>
#include <functional>
#include <iterator>
#include <chrono>
#include <systemd/sd-daemon.h>

#include <cassandra.h>
#include <boost/asio/basic_waitable_timer.hpp>
#include <dbconnection_observations.h>
#include <mqtt_client_cpp.hpp>
#include <utility>

#include "../time_offseter.h"
#include "../cassandra_utils.h"
#include "mqtt_subscriber.h"
#include "../connector.h"

#define DEFAULT_VERIFY_PATH "/etc/ssl/certs"

namespace asio = boost::asio;
namespace sys = boost::system;
namespace chrono = std::chrono;
namespace args = std::placeholders;

namespace meteodata
{

using namespace date;

MqttSubscriber::MqttSubscriber(const MqttSubscriber::MqttSubscriptionDetails& details, asio::io_context& ioContext,
							   DbConnectionObservations& db) :
		Connector{ioContext, db},
		_details{details},
		_timer{ioContext}
{
	_status.shortStatus = "IDLE";
	// we cheat a little bit with the activeSince value, the lastReloaded is more relevant
	_status.activeSince = date::floor<chrono::seconds>(chrono::system_clock::now());
}

bool operator<(const MqttSubscriber::MqttSubscriptionDetails& s1, const MqttSubscriber::MqttSubscriptionDetails& s2)
{
	if (s1.host < s2.host) {
		return true;
	} else if (s1.host == s2.host) {
		if (s1.port < s2.port) {
			return true;
		} else if (s1.port == s2.port) {
			if (s1.user < s2.user) {
				return true;
			} else if (s1.user == s2.user) {
				return s1.password < s2.password;
			}
		}
	}
	return false;
}


void MqttSubscriber::addStation(const std::string& topic, const CassUuid& station, TimeOffseter::PredefinedTimezone tz)
{
	std::string stationName;
	int pollingPeriod;
	time_t lastArchiveDownloadTime;
	_db.getStationDetails(station, stationName, pollingPeriod, lastArchiveDownloadTime);
	date::sys_seconds lastArchive = date::sys_seconds(chrono::seconds(lastArchiveDownloadTime));

	float latitude;
	float longitude;
	int elevation;
	std::string stationName2;
	int pollingPeriod2;
	_db.getStationCoordinates(station, latitude, longitude, elevation, stationName2, pollingPeriod2);

	TimeOffseter timeOffseter = TimeOffseter::getTimeOffseterFor(tz);
	timeOffseter.setLatitude(latitude);
	timeOffseter.setLongitude(longitude);
	timeOffseter.setElevation(elevation);
	timeOffseter.setMeasureStep(pollingPeriod);
	std::cout << SD_NOTICE << "[MQTT " << station << "] connection: " << "Discovered MQTT station " << stationName
			  << std::endl;

	_stations.emplace(topic, std::make_tuple(station, stationName, pollingPeriod, lastArchive, timeOffseter));
}

bool MqttSubscriber::handleConnAck(bool, uint8_t)
{
	for (auto&& station : _stations) {
		_subscriptions[_client->subscribe(station.first, mqtt::qos::at_least_once)] = station.first;
	}

	return true;
}

void MqttSubscriber::checkRetryStartDeadline(const sys::error_code& e)
{
	/* if the timer has been cancelled, then bail out ; we probably have been
	 * asked to die */
	if (e == sys::errc::operation_canceled)
		return;

	// verify that the timeout is not spurious
	if (_timer.expires_at() <= chrono::steady_clock::now()) {
		start();
	} else {
		/* spurious handler call, restart the timer without changing the
		 * deadline */
		auto self(shared_from_this());
		_timer.async_wait([this, self] (const sys::error_code& e) { checkRetryStartDeadline(e); });
	}
}

void MqttSubscriber::handleClose()
{
	// do nothing
}

void MqttSubscriber::handleError(sys::error_code const&)
{
	// wait a little and restart, up to three times
	auto self{shared_from_this()};
	if (_retries < MAX_RETRIES) {
		_timer.expires_from_now(chrono::minutes(_retries));
		_timer.async_wait([this, self] (const sys::error_code& e) { checkRetryStartDeadline(e); });
	} else {
		std::cerr << SD_ERR << "[MQTT] protocol: " << "impossible to reconnect" << std::endl;
		// bail off
	}
}

bool MqttSubscriber::handlePubAck(uint16_t)
{
	return true;
}

bool MqttSubscriber::handlePubRec(uint16_t)
{
	return true;
}

bool MqttSubscriber::handlePubComp(uint16_t)
{
	return true;
}

bool MqttSubscriber::handleSubAck(uint16_t, std::vector<boost::optional<std::uint8_t>>)
{
	return true;
}

bool MqttSubscriber::handlePublish(std::uint8_t, boost::optional<std::uint16_t>, mqtt::string_view topic,
								   mqtt::string_view contents)
{
	processArchive(topic, contents);
	return true;
}

void MqttSubscriber::start()
{
	std::cout << SD_DEBUG << "[MQTT] protocol: " << "About to start the MQTT client  " << std::endl;
	_client = mqtt::make_tls_client(_ioContext, _details.host, _details.port);

	std::ostringstream clientId;
	clientId << MqttSubscriber::CLIENT_ID << ":" << getConnectorSuffix();
	_client->set_client_id(clientId.str());
	_client->set_user_name(_details.user);
	_client->set_password(_details.password);
	_client->set_clean_session(false); /* this way, we can catch up on missed packets upon reconnection */
	_client->add_verify_path(DEFAULT_VERIFY_PATH);
	std::cout << SD_DEBUG << "[MQTT] protocol: " << "Created the client" << std::endl;

	auto self{shared_from_this()};
	_client->set_connack_handler([this, self](bool sp, std::uint8_t ret) {
		std::cout << SD_DEBUG << "[MQTT] protocol: " << "Connection attempt to " << _details.host << ": "
				  << mqtt::connect_return_code_to_str(ret) << std::endl;
		if (ret == mqtt::connect_return_code::accepted) {
			_retries = 0;
			std::cerr << SD_NOTICE << "[MQTT] protocol: " << "Connection established to " << _details.host << ": "
					  << mqtt::connect_return_code_to_str(ret) << std::endl;
			_status.shortStatus = "CONNECTED";
			_status.lastReloaded = date::floor<chrono::seconds>(chrono::system_clock::now());
			_status.nbDownloads = 0;

			return handleConnAck(sp, ret);
		} else {
			std::cerr << SD_ERR << "[MQTT] protocol: " << "Failed to establish connection to " << _details.host << ": "
					  << mqtt::connect_return_code_to_str(ret) << std::endl;
			_status.shortStatus = "FAILED TO CONNECT";
		}
		return true;
	});
	_client->set_close_handler([this, self]() {
		std::cerr << SD_NOTICE << "[MQTT] protocol: " << "MQTT client " << _details.host << " disconnected"
				  << std::endl;
		_status.shortStatus = "CONNECTION CLOSED";
		handleClose();
	});
	_client->set_error_handler([this, self](sys::error_code const& ec) {
		std::cerr << SD_ERR << "[MQTT] protocol: " << "MQTT client " << _details.host << ": unexpected disconnection "
				  << ec.message() << std::endl;
		_status.shortStatus = "ERROR";
		handleError(ec);
	});
	_client->set_puback_handler([this, self]([[maybe_unused]] std::uint16_t packetId) {
		return handlePubAck(packetId);
	});
	_client->set_pubrec_handler([this, self]([[maybe_unused]] std::uint16_t packetId) {
		return handlePubRec(packetId);
	});
	_client->set_pubcomp_handler([this, self]([[maybe_unused]] std::uint16_t packetId) {
		return handlePubComp(packetId);
	});
	_client->set_suback_handler(
			[this, self](std::uint16_t packetId, std::vector<boost::optional<std::uint8_t>> results) {
				return handleSubAck(packetId, std::move(results));
			});
	_client->set_publish_handler(
			[this, self](std::uint8_t header, boost::optional<std::uint16_t> packetId, mqtt::string_view topic,
						 mqtt::string_view contents) {
				++_status.nbDownloads;
				return handlePublish(header, packetId, topic, contents);
			});
	std::cout << SD_DEBUG << "[MQTT] protocol: " << "Set the handlers" << std::endl;

	_retries++;
	_client->connect();
}

void MqttSubscriber::stop()
{
	_client->disconnect();
	_status.shortStatus = "STOPPED";
}

void MqttSubscriber::reload()
{
	auto self{shared_from_this()};
	_client->set_close_handler([this, self]() {
		start();
	});
	_client->disconnect();
}

}
