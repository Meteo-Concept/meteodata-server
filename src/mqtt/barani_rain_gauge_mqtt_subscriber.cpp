/**
 * @file barani_rain_gauge_mqtt_subscriber.cpp
 * @brief Implementation of the BaraniRainGaugeMqttSubscriber class
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
#include <functional>
#include <iterator>
#include <map>
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
#include "barani_rain_gauge_mqtt_subscriber.h"
#include "../barani/barani_rain_gauge_message.h"
#include "liveobjects_mqtt_subscriber.h"

namespace sys = boost::system;
namespace chrono = std::chrono;

namespace meteodata
{

BaraniRainGaugeMqttSubscriber::BaraniRainGaugeMqttSubscriber(MqttSubscriber::MqttSubscriptionDetails details, asio::io_context& ioContext,
	 DbConnectionObservations& db) :
		LiveobjectsMqttSubscriber(std::move(details), ioContext, db)
{
}

std::unique_ptr<meteodata::LiveobjectsMessage> BaraniRainGaugeMqttSubscriber::buildMessage(const pt::ptree& json, const CassUuid& station,
																								  date::sys_seconds& timestamp)
{
	using namespace std::chrono;
	using date::operator<<;

	time_t lastUpdate;
	int previousClicks;
	bool result = _db.getCachedInt(station, BARANI_RAINFALL_CACHE_KEY, lastUpdate, previousClicks);
	std::optional<int> prev = std::nullopt;
	if (result && chrono::_V2::system_clock::from_time_t(lastUpdate) > chrono::_V2::system_clock::now() - 24h) {
		// the last rainfall datapoint is not too old, we can use
		// it as a reference for the current number of clicks recorded
		// by the pluviometer
		prev = previousClicks;
	}


	int previousCorrectionClicks;
	result = _db.getCachedInt(station, BARANI_RAINFALL_CORRECTION_CACHE_KEY, lastUpdate, previousCorrectionClicks);
	std::optional<int> prevCorr = std::nullopt;
	if (result && chrono::_V2::system_clock::from_time_t(lastUpdate) > chrono::_V2::system_clock::now() - 24h) {
		// the last rainfall datapoint is not too old, we can use
		// it as a reference for the current number of clicks recorded
		// by the pluviometer
		prevCorr = previousCorrectionClicks;
	}

	auto t = json.get<std::string>("timestamp");
	std::istringstream is{t};
	// don't bother parsing the seconds and subseconds
	is >> date::parse("%Y-%m-%dT%H:%M:", timestamp);

	std::cout << SD_DEBUG << "[MQTT " << station << "] measurement: " << "Data received for timestamp " << timestamp << " (" << t << ")" << std::endl;
	auto payload = json.get<std::string>("value.payload");

	std::unique_ptr<BaraniRainGaugeMessage> msg = std::make_unique<BaraniRainGaugeMessage>();
	msg->ingest(payload, timestamp, BARANI_RAIN_GAUGE_RESOLUTION, previousClicks, previousCorrectionClicks);
	return msg;
}


void BaraniRainGaugeMqttSubscriber::postInsert(const CassUuid& station, const std::unique_ptr<LiveobjectsMessage>& msg)
{
	// always safe, the pointer is constructed by the method buildMessage below
	auto* m = dynamic_cast<BaraniRainGaugeMessage*>(msg.get());
	if (m) {
		int ret = MqttSubscriber::_db.cacheInt(station, BARANI_RAINFALL_CACHE_KEY, chrono::system_clock::to_time_t(m->getObservation(station).time), m->getRainfallClicks());
		if (!ret)
			std::cerr << SD_ERR << "[MQTT " << station << "] management: "
					  << "Couldn't update the rainfall number of clicks, accumulation error possible"
					  << std::endl;
		ret = MqttSubscriber::_db.cacheInt(station, BARANI_RAINFALL_CORRECTION_CACHE_KEY, chrono::system_clock::to_time_t(m->getObservation(station).time), m->getRainfallCorrectionClicks());
		if (!ret)
			std::cerr << SD_ERR << "[MQTT " << station << "] management: "
					  << "Couldn't update the rainfall number of clicks, accumulation error possible"
					  << std::endl;
	}
}

}
