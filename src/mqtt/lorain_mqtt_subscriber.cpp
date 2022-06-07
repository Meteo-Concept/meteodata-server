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
#include "lorain_mqtt_subscriber.h"
#include "../pessl/lorain_message.h"
#include "liveobjects_mqtt_subscriber.h"

namespace sys = boost::system;
namespace chrono = std::chrono;

namespace meteodata
{

LorainMqttSubscriber::LorainMqttSubscriber(MqttSubscriber::MqttSubscriptionDetails details, asio::io_service& ioService,
	 DbConnectionObservations& db) :
		LiveobjectsMqttSubscriber(std::move(details), ioService, db)
{
}

void LorainMqttSubscriber::postInsert(const CassUuid& station, const std::unique_ptr<LiveobjectsMessage>& msg)
{
	// always safe, the pointer is constructed by the method buildMessage below
	auto* m = dynamic_cast<LorainMessage*>(msg.get());
	if (m) {
		int ret = MqttSubscriber::_db.cacheInt(station, LorainMqttSubscriber::LORAIN_RAINFALL_CACHE_KEY, chrono::system_clock::to_time_t(m->getObservation(station).time), m->getRainfallClicks());
		if (!ret)
			std::cerr << SD_ERR << "[MQTT " << station << "] management: "
					  << "Couldn't update the rainfall number of clicks, accumulation error possible"
					  << std::endl;
	}
}

std::unique_ptr<meteodata::LiveobjectsMessage> LorainMqttSubscriber::buildMessage(const pt::ptree& json, const CassUuid& station,
																				  date::sys_seconds& timestamp)
{
	using namespace std::chrono;
	using date::operator<<;

	time_t lastUpdate;
	int previousClicks;
	bool result = _db.getCachedInt(station, LORAIN_RAINFALL_CACHE_KEY, lastUpdate, previousClicks);
	std::optional<int> prev = std::nullopt;
	if (result && chrono::_V2::system_clock::from_time_t(lastUpdate) > chrono::_V2::system_clock::now() - 24h) {
		// the last rainfall datapoint is not too old, we can use
		// it as a reference for the current number of clicks recorded
		// by the pluviometer
		prev = previousClicks;
	}

	std::string t = json.get<std::string>("timestamp");
	std::istringstream is{t};
	// don't bother parsing the seconds and subseconds
	is >> date::parse("%Y-%m-%dT%H:%M:", timestamp);

	std::cout << SD_DEBUG << "[MQTT " << station << "] measurement: " << "Data received for timestamp " << timestamp << " (" << t << ")" << std::endl;
	std::string payload = json.get<std::string>("value.payload");

	std::unique_ptr<LorainMessage> msg = std::make_unique<LorainMessage>();
	msg->ingest(payload, timestamp, prev);
	return msg;
}

}
