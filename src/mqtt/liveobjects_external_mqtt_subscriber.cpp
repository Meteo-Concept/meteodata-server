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

#include <boost/asio.hpp>
#include <dbconnection_observations.h>

#include "async_job_publisher.h"
#include "mqtt/mqtt_subscriber.h"
#include "mqtt/liveobjects_external_mqtt_subscriber.h"

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

}
