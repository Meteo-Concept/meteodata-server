/**
 * @file liveobjects_external_mqtt_subscriber.cpp
 * @brief Implementation of the LiveobjectsExternalMqttSubscriber class
 * @author Laurent Georget
 * @date 2023-12-07
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

#include <string>

#include <dbconnection_observations.h>

#include "async_job_publisher.h"
#include "mqtt/mqtt_subscriber.h"
#include "mqtt/liveobjects_external_mqtt_subscriber.h"
#include "cassandra_utils.h"

namespace meteodata
{
namespace pt = boost::property_tree;

LiveobjectsExternalMqttSubscriber::LiveobjectsExternalMqttSubscriber(
	std::string clientIdentifier,
	const MqttSubscriber::MqttSubscriptionDetails& details,
	asio::io_context& ioContext,
	DbConnectionObservations& db,
	AsyncJobPublisher* jobPublisher) :
		LiveobjectsMqttSubscriber{details, ioContext, db, jobPublisher},
		_clientIdentifier{std::move(clientIdentifier)}
{
}

void LiveobjectsExternalMqttSubscriber::reload()
{
	_client->disconnect();
	if (!_stopped) {
		std::vector<std::tuple<CassUuid, std::string, int, std::string, std::unique_ptr<char[]>, size_t, std::string, int>> mqttStations;
		_db.getMqttStations(mqttStations);
		std::vector<std::tuple<CassUuid, std::string, std::string>> liveobjectsStations;
		_db.getAllLiveobjectsStations(liveobjectsStations);

		_stations.clear();
		for (auto&& station : mqttStations) {
			const CassUuid& uuid = std::get<0>(station);
			const std::string& topic = std::get<6>(station);
			TimeOffseter::PredefinedTimezone tz{std::get<7>(station)};

			if (topic == "fifo/meteoconcept") {
				MqttSubscriber::MqttSubscriptionDetails details{
					std::get<1>(station), std::get<2>(station),
					std::get<3>(station),
					std::string(std::get<4>(station).get(), std::get<5>(station))
				};

				if (_details == details ) {
					auto it = std::find_if(liveobjectsStations.begin(), liveobjectsStations.end(),
						[&uuid](auto&& objSt) { return uuid == std::get<0>(objSt); });
					if (it != liveobjectsStations.end())
						addStation(topic, uuid, tz, std::get<1>(*it));
				}
			}
		}

		start();
	}
}

}
