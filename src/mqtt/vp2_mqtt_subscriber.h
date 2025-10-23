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
#include <map>
#include <vector>
#include <functional>
#include <chrono>
#include <system_error>
#include <unistd.h>

#include <boost/asio/steady_timer.hpp>
#include <boost/system/error_code.hpp>
#include <boost/optional.hpp>
#include <cassandra.h>
#include <date/date.h>
#include <date/tz.h>
#include <cassobs/dbconnection_observations.h>
#include <mqtt_client_cpp.hpp>

#include "time_offseter.h"
#include "async_job_publisher.h"
#include "mqtt/mqtt_subscriber.h"
#include "davis/vantagepro2_archive_page.h"

namespace meteodata
{

using namespace meteodata;

/**
 */
class VP2MqttSubscriber : public MqttSubscriber
{
public:
	VP2MqttSubscriber(const MqttSubscriptionDetails& details, asio::io_context& ioContext,
		DbConnectionObservations& db, AsyncJobPublisher* jobPublisher = nullptr);

private:
	static constexpr char ARCHIVES_TOPIC[] = "/dmpaft";
	std::map<std::string, date::sys_seconds, std::less<>> _clockResetTimes;
	void setClock(const std::string& topic, const CassUuid& station, const TimeOffseter& timeOffseter);

protected:
	bool handleSubAck(packet_id_t packetId, std::vector<mqtt::suback_return_code> results) override;
	void processArchive(const std::string_view& topicName, const std::string_view& content) override;

	const char* getConnectorSuffix() override
	{ return "vp2"; }

	void reload() override;
};

}

#endif
