/**
 * @file thlora_thermohygrometer_mqtt_subscriber.h
 * @brief Definition of the ThloraThermohygrometerMqttSubscriber class
 * @author Laurent Georget
 * @date 2022-10-04
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

#ifndef THLORA_THERMOHYGROMETER_MQTT_SUBSCRIBER_H
#define THLORA_THERMOHYGROMETER_MQTT_SUBSCRIBER_H

#include <vector>

#include <boost/asio/io_context.hpp>
#include <boost/property_tree/ptree.hpp>
#include <cassandra.h>
#include <dbconnection_observations.h>
#include <mqtt_client_cpp.hpp>

#include "../pessl/lorain_message.h"
#include "mqtt_subscriber.h"
#include "liveobjects_mqtt_subscriber.h"

namespace meteodata
{

using namespace meteodata;

class LiveobjectsMessage;

/**
 */
class ThloraThermohygrometerMqttSubscriber : public LiveobjectsMqttSubscriber
{
public:
	ThloraThermohygrometerMqttSubscriber(MqttSubscriptionDetails details, asio::io_context& ioContext, DbConnectionObservations& db);

protected:
	const char* getConnectorSuffix() override
	{
		return "Thlora_thermohygrometer";
	}

	std::unique_ptr<LiveobjectsMessage> buildMessage(const boost::property_tree::ptree& json, const CassUuid& station, date::sys_seconds& timestamp) override;

	const char* getTopic() const override
	{
		return "fifo/Thlora_thermohygrometer";
	}
};

}

#endif

