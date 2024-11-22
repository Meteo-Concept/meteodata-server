/**
 * @file generic_mqtt_subscriber.h
 * @brief Definition of the GenericMqttSubscriber class
 * @author Laurent Georget
 * @date 2023-06-16
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
#ifndef METEODATA_SERVER_GENERIC_MQTT_SUBSCRIBER_H
#define METEODATA_SERVER_GENERIC_MQTT_SUBSCRIBER_H

#include <vector>
#include <boost/asio/io_context.hpp>
#include <cassandra.h>
#include <cassobs/dbconnection_observations.h>
#include <mqtt_client_cpp.hpp>

#include "async_job_publisher.h"
#include "generic_message.h"
#include "mqtt_subscriber.h"

namespace meteodata
{

class GenericMqttSubscriber : public MqttSubscriber
{
public:
	GenericMqttSubscriber(const MqttSubscriptionDetails& details, asio::io_context& ioContext,
						  DbConnectionObservations& db, AsyncJobPublisher* jobScheduler = nullptr);

protected:
	bool handleSubAck(std::uint16_t packetId, std::vector<boost::optional<std::uint8_t>> results) override;

	void processArchive(const mqtt::string_view& topicName, const mqtt::string_view& content) override;
	GenericMessage buildMessage(const boost::property_tree::ptree& json, const CassUuid& station, date::sys_seconds& timestamp);

	const char* getConnectorSuffix() override
	{
		return "generic";
	}

	void reload() override;
};

}


#endif //METEODATA_SERVER_GENERIC_MQTT_SUBSCRIBER_H
