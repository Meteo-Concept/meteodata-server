/**
 * @file mqttsubscriber.h
 * @brief Definition of the MqttSubscriber class
 * @author Laurent Georget
 * @date 2019-01-12
 */
/*
 * Copyright (C) 2019  SAS JD Environnement <contact@meteo-concept.fr>
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

#ifndef MQTTSUBSCRIBER_H
#define MQTTSUBSCRIBER_H

#include <iostream>
#include <memory>
#include <vector>
#include <set>
#include <map>
#include <functional>
#include <unistd.h>
#include <chrono>

#include <boost/system/error_code.hpp>
#include <boost/asio/basic_waitable_timer.hpp>
#include <cassandra.h>
#include <date.h>
#include <tz.h>
#include <dbconnection_observations.h>
#include <mqtt_client_cpp.hpp>

#include "async_job_publisher.h"
#include "time_offseter.h"
#include "connector.h"
#include "davis/vantagepro2_archive_page.h"

namespace meteodata
{

/**
 */
class MqttSubscriber : public Connector
{
public:
	struct MqttSubscriptionDetails
	{
		std::string host;
		int port;
		std::string user;
		std::string password;

		friend bool operator<(const MqttSubscriptionDetails& s1, const MqttSubscriptionDetails& s2);
	};

	MqttSubscriber(const MqttSubscriptionDetails& details, asio::io_context& ioContext,
				   DbConnectionObservations& db, AsyncJobPublisher* jobPublisher = nullptr);
	void addStation(const std::string& topic, const CassUuid& station, TimeOffseter::PredefinedTimezone tz);
	void start() override;
	void stop() override;
	void reload() override;

protected:
	bool _stopped;

	MqttSubscriptionDetails _details;

	std::map<std::uint16_t, std::string> _subscriptions;

	AsyncJobPublisher* _jobPublisher;

	/**
	 * @brief Map from topic to station UUID, station name, polling period, last archive insertion datetime, time offseter
	 */
	std::map<std::string, std::tuple<CassUuid, std::string, int, date::sys_seconds, TimeOffseter>> _stations;
	decltype(mqtt::make_tls_client(_ioContext, _details.host, _details.port)) _client;

	/**
	 * @brief The channel subscription id
	 */
	std::uint16_t _pid = 0;

	static constexpr int MAX_RETRIES = 3;

	/**
	 * @brief The number of times we have tried to restart
	 */
	int _retries = 0;

	/**
	 * @brief The timer used to retry the connection when the client
	 * disconnects
	 */
	asio::basic_waitable_timer<std::chrono::steady_clock> _timer;

	static constexpr char CLIENT_ID[] = "meteodata";
	virtual void processArchive(const mqtt::string_view& topicName, const mqtt::string_view& content) = 0;
	virtual const char* getConnectorSuffix() = 0;
	void checkRetryStartDeadline(const boost::system::error_code& e);

	virtual bool handleConnAck(bool sp, std::uint8_t ret);
	virtual void handleClose();
	virtual void handleError(boost::system::error_code const& ec);
	virtual bool handlePubAck(std::uint16_t packetId);
	virtual bool handlePubRec(std::uint16_t packetId);
	virtual bool handlePubComp(std::uint16_t packetId);
	virtual bool handleSubAck(std::uint16_t packetId, std::vector<boost::optional<std::uint8_t>> results);
	virtual bool handlePublish(std::uint8_t header, boost::optional<std::uint16_t> packet_id,
		mqtt::string_view topic_name, mqtt::string_view contents);
};

bool operator<(const MqttSubscriber::MqttSubscriptionDetails& s1, const MqttSubscriber::MqttSubscriptionDetails& s2);

}

#endif
