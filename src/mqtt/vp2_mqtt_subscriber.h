/**
 * @file vp2_mqttsubscriber.h
 * @brief Definition of the VP2MqttSubscriber class
 * @author Laurent Georget
 * @date 2021-02-23
 */
/*
 * Copyright (C) 2021  SAS JD Environnement <contact@meteo-concept.fr>
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

#ifndef VP2_MQTTSUBSCRIBER_H
#define VP2_MQTTSUBSCRIBER_H

#include <iostream>
#include <memory>
#include <array>
#include <vector>
#include <functional>
#include <unistd.h>

#include <boost/system/error_code.hpp>
#include <cassandra.h>
#include <date/date.h>
#include <date/tz.h>
#include <dbconnection_observations.h>
#include <mqtt_client_cpp.hpp>

#include "../time_offseter.h"
#include "../davis/vantagepro2_archive_page.h"
#include "mqtt_subscriber.h"

namespace meteodata {

using namespace meteodata;

/**
 */
class VP2MqttSubscriber : public MqttSubscriber
{
public:
	VP2MqttSubscriber(MqttSubscriptionDetails details,
		asio::io_service& ioService, DbConnectionObservations& db);

private:
	static constexpr char ARCHIVES_TOPIC[] = "/dmpaft";

protected:
	bool handleSubAck(std::uint16_t packetId, std::vector<boost::optional<std::uint8_t>> results) override;
	void processArchive(const mqtt::string_view& topicName, const mqtt::string_view& content) override;
	const char* getConnectorSuffix() override { return "vp2"; }
};

}

#endif
