/**
 * @file liveobjects_external_mqtt_subscriber.h
 * @brief Definition of the LiveobjectsExternalMqttSubscriber class
 * @author Laurent Georget
 * @date 2023-12-07
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
#ifndef METEODATA_SERVER_LIVEOBJECTS_EXTERNAL_MQTT_SUBSCRIBER_H
#define METEODATA_SERVER_LIVEOBJECTS_EXTERNAL_MQTT_SUBSCRIBER_H

#include <vector>
#include <boost/asio/io_context.hpp>
#include <cassandra.h>
#include <dbconnection_observations.h>
#include <mqtt_client_cpp.hpp>

#include "liveobjects_mqtt_subscriber.h"

namespace meteodata
{

class LiveobjectsExternalMqttSubscriber : public LiveobjectsMqttSubscriber
{
public:
	LiveobjectsExternalMqttSubscriber(
		std::string clientIdentifier,
		const MqttSubscriptionDetails& details, asio::io_context& ioContext,
		DbConnectionObservations& db, AsyncJobPublisher* jobPublisher = nullptr);

private:
	std::string _clientIdentifier;

protected:
	const char* getConnectorSuffix() override
	{
		return _clientIdentifier.c_str();
	}

	const char* getTopic() const override
	{
		return "fifo/meteoconcept";
	}
};

}


#endif //METEODATA_SERVER_LIVEOBJECTS_EXTERNAL_MQTT_SUBSCRIBER_H
