/**
 * @file objenious_mqttsubscriber.h
 * @brief Definition of the ObjeniousMqttSubscriber class
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

#ifndef OBJENIOUS_MQTTSUBSCRIBER_H
#define OBJENIOUS_MQTTSUBSCRIBER_H

#include <iostream>
#include <vector>
#include <map>
#include <functional>
#include <unistd.h>

#include <boost/system/error_code.hpp>
#include <dbconnection_observations.h>
#include <mqtt_client_cpp.hpp>

#include "time_offseter.h"
#include "async_job_publisher.h"
#include "mqtt_subscriber.h"
#include "davis/vantagepro2_archive_page.h"

namespace meteodata
{

using namespace meteodata;

/**
*/
class ObjeniousMqttSubscriber : public MqttSubscriber
{
public:
	ObjeniousMqttSubscriber(MqttSubscriptionDetails details, asio::io_context& ioContext,
							DbConnectionObservations& db, AsyncJobPublisher* jobPublisher = nullptr);
	void addStation(const std::string& topic, const CassUuid& station, TimeOffseter::PredefinedTimezone tz,
					const std::string& objeniousId, const std::map<std::string, std::string>& variables);

private:
	/**
	 * @brief The suffix of the topic instances of this class will receive data at
	 */
	static constexpr char ARCHIVES_TOPIC[] = "/data";

	/**
	 * @brief The map from topic to tuple {objeniousId, variables}
	 */
	std::map<std::string, std::tuple<std::string, std::map<std::string, std::string>>> _devices;

	/**
	 * @brief What variables should be extracted from the data points, and what
	 * their name is in said data points
	 */
	std::map<std::string, std::string> _variables;


protected:
	bool handleSubAck(std::uint16_t packetId, std::vector<boost::optional<std::uint8_t>> results) override;
	void processArchive(const mqtt::string_view& topicName, const mqtt::string_view& content) override;

	const char* getConnectorSuffix() override
	{
		return "objenious";
	}
};

}

#endif
