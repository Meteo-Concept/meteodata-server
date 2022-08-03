/**
 * @file barani_rain_gauge_mqtt_subscriber.h
 * @brief Definition of the BaraniRainGaugeMqttSubscriber class
 * @author Laurent Georget
 * @date 2022-04-29
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

#ifndef BARANI_RAIN_GAUGE_MQTT_SUBSCRIBER_H
#define BARANI_RAIN_GAUGE_MQTT_SUBSCRIBER_H

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
class BaraniRainGaugeMqttSubscriber : public LiveobjectsMqttSubscriber
{
public:
	BaraniRainGaugeMqttSubscriber(MqttSubscriptionDetails details, asio::io_context& ioContext, DbConnectionObservations& db);

protected:
	const char* getConnectorSuffix() override
	{
		return "barani_rain_gauge";
	}

	void postInsert(const CassUuid& station, const std::unique_ptr<LiveobjectsMessage>& msg) override;

	std::unique_ptr<LiveobjectsMessage> buildMessage(const boost::property_tree::ptree& json, const CassUuid& station, date::sys_seconds& timestamp) override;

	const char* getTopic() const override
	{
		return "fifo/Barani_rain";
	}

private:
	static constexpr float BARANI_RAIN_GAUGE_RESOLUTION = 0.2f;
	static constexpr char BARANI_RAINFALL_CACHE_KEY[] = "barani_rainfall_clicks";
	static constexpr char BARANI_RAINFALL_CORRECTION_CACHE_KEY[] = "barani_raincorr_clicks";
};

}

#endif

