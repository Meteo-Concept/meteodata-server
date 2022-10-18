/**
 * @file thlora_thermohygrometer_mqtt_subscriber.cpp
 * @brief Implementation of the ThloraThermohygrometerMqttSubscriber class
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

#include <iostream>
#include <memory>
#include <iterator>
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
#include "thlora_thermohygrometer_mqtt_subscriber.h"
#include "../thlora/thlora_thermohygrometer_message.h"
#include "liveobjects_mqtt_subscriber.h"

namespace sys = boost::system;
namespace chrono = std::chrono;

namespace meteodata
{

ThloraThermohygrometerMqttSubscriber::ThloraThermohygrometerMqttSubscriber(MqttSubscriber::MqttSubscriptionDetails details, asio::io_context& ioContext,
	 DbConnectionObservations& db) :
		LiveobjectsMqttSubscriber(std::move(details), ioContext, db)
{
}

std::unique_ptr<meteodata::LiveobjectsMessage> ThloraThermohygrometerMqttSubscriber::buildMessage(
		const pt::ptree& json, const CassUuid& station, date::sys_seconds& timestamp
	)
{
	using namespace std::chrono;
	using date::operator<<;

	auto t = json.get<std::string>("timestamp");
	std::istringstream is{t};
	// don't bother parsing the seconds and subseconds
	is >> date::parse("%Y-%m-%dT%H:%M:", timestamp);

	std::cout << SD_DEBUG << "[MQTT " << station << "] measurement: " << "Data received for timestamp " << timestamp << " (" << t << ")" << std::endl;
	auto payload = json.get<std::string>("value.payload");

	std::unique_ptr<ThloraThermohygrometerMessage> msg = std::make_unique<ThloraThermohygrometerMessage>();
	msg->ingest(payload, timestamp);
	return msg;
}

}