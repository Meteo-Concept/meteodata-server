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
#include <sstream>
#include <cstring>
#include <cctype>
#include <syslog.h>
#include <unistd.h>

#include <cassandra.h>
#include <dbconnection_observations.h>
#include <mqtt_client_cpp.hpp>

#include "mqttsubscriber.h"
#include "vantagepro2archivepage.h"
#include "timeoffseter.h"

#define DEFAULT_VERIFY_PATH "/etc/ssl/certs"

namespace sys = boost::system; //system() is a function, it cannot be redefined
//as a namespace

namespace meteodata {

using namespace date;

constexpr char MqttSubscriber::CLIENT_ID[];
constexpr char MqttSubscriber::ARCHIVES_TOPIC[];

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
	_timeOffseter.setMeasureStep(_pollingPeriod);
	std::cerr << "Discovered MQTT station " << _stationName << std::endl;
}

void MqttSubscriber::start()
{
	std::cerr << "About to start the MQTT client for " << _stationName << std::endl;
	_client = mqtt::make_tls_client(_ioService, _details.host, _details.port);
	std::cerr << "Created the client" << std::endl;
	_client->set_client_id(MqttSubscriber::CLIENT_ID);
	_client->set_user_name(_details.user);
	_client->set_password(_details.password.get());
	_client->set_clean_session(false); /* this way, we can catch up on missed packets upon reconnection */
	_client->add_verify_path(DEFAULT_VERIFY_PATH);
	std::cerr << "Set info" << std::endl;

	auto self{shared_from_this()};
	_client->set_connack_handler(
		[this,self]([[maybe_unused]] bool sp, std::uint8_t ret) {
			std::cerr << "Connection attempt to " << _details.host << " for station " << _stationName << ": " << mqtt::connect_return_code_to_str(ret) << std::endl;
			syslog(ret == mqtt::connect_return_code::accepted ? LOG_NOTICE : LOG_ERR, "Connection attempt to %s for station %s: %s", _details.host.c_str(), _stationName.c_str(), mqtt::connect_return_code_to_str(ret));
			if (ret == mqtt::connect_return_code::accepted) {
				_pid = _client->subscribe(_details.topic, mqtt::qos::at_least_once);
				_client->subscribe(_details.topic + ARCHIVES_TOPIC, mqtt::qos::at_least_once);
			}
			return true;
		}
	);
	_client->set_close_handler(
		[this,self]() {
			syslog(LOG_NOTICE, "%s: disconnected", _stationName.c_str());
		}
	);
	_client->set_error_handler(
		[this,self](sys::error_code const& ec) {
			std::cerr << _stationName << ": unexpected disconnection " << ec.message() << std::endl;
			syslog(LOG_ERR, "%s: unexpected disconnection %s", _stationName.c_str(), ec.message().c_str());
		}
	);
	_client->set_puback_handler(
		[self]([[maybe_unused]] std::uint16_t packet_id) {
			return true;
		}
	);
	_client->set_pubrec_handler(
		[self]([[maybe_unused]] std::uint16_t packet_id) {
			return true;
		}
	);
	_client->set_pubcomp_handler(
		[self]([[maybe_unused]] std::uint16_t packet_id) {
			return true;
		}
	);
	_client->set_suback_handler(
		[this,self](std::uint16_t packet_id, std::vector<boost::optional<std::uint8_t>> results) {
			for (auto const& e : results) { /* we are expecting only one */
				if (!e) {
					std::cerr << _stationName <<  ": subscription failed: " << mqtt::qos::to_str(*e) << std::endl;
					stop();
				}
			}
			if (packet_id == _pid) {
				if (chrono::system_clock::now() - _lastArchive > chrono::minutes(_pollingPeriod))
					_client->publish_at_least_once(_details.topic, date::format("DMPAFT %Y-%m-%d %H:%M", _lastArchive));
			}
			return true;
		}
	);
	_client->set_publish_handler(
		[this,self]([[maybe_unused]] std::uint8_t header,
			[[maybe_unused]]  boost::optional<std::uint16_t> packet_id,
			[[maybe_unused]] std::string topic_name,
			std::string contents) {
			processArchive(contents);
			return true;
		}
	);
	std::cerr << "Set the handlers" << std::endl;

	_client->connect();
}

void MqttSubscriber::stop()
{
	_client->disconnect();
}

void MqttSubscriber::processArchive(const std::string& content)
{
	std::cerr << "Now downloading for station " << _stationName << std::endl;

	if (content.size() != sizeof(VantagePro2ArchiveMessage::ArchiveDataPoint)) {
		syslog(LOG_ERR, "station %s: input from MQTT broker has an invalid size", _stationName.c_str());
		std::cerr << "station " << _stationName << ": input has an invalid size " << std::endl;
		return;
	}

	VantagePro2ArchiveMessage::ArchiveDataPoint data;
	std::memcpy(&data, content.c_str(), sizeof(VantagePro2ArchiveMessage::ArchiveDataPoint));
	VantagePro2ArchiveMessage msg{data, &_timeOffseter};
	int ret = false;
	if (msg.looksValid()) {
		// Do not bother in inserting v1 data points
		ret = _db.insertV2DataPoint(_station, msg);
	} else {
		std::cerr << "Record looks invalid, discarding... (for information, timestamp says " << msg.getTimestamp() << " and system clock says " << chrono::system_clock::now() << ")" << std::endl;
	}
	if (ret) {
		std::cerr << "Archive data stored\n" << std::endl;
		time_t lastArchiveDownloadTime = msg.getTimestamp().time_since_epoch().count();
		ret = _db.updateLastArchiveDownloadTime(_station, lastArchiveDownloadTime);
		if (!ret)
			syslog(LOG_ERR, "station %s: Couldn't update last archive download time", _stationName.c_str());
	} else {
		std::cerr << "Failed to store archive! Aborting" << std::endl;
		syslog(LOG_ERR, "station %s: Couldn't store archive", _stationName.c_str());
		//stop();
		return;
	}
}

}
