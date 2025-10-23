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
#include <optional>
#include <chrono>
#include <system_error>
#include <unistd.h>

#include <boost/system/error_code.hpp>
#include <boost/asio/basic_waitable_timer.hpp>
#include <cassandra.h>
#include <date/date.h>
#include <date/tz.h>
#include <cassobs/dbconnection_observations.h>
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

	AsyncJobPublisher* _jobPublisher;

	std::mutex _stationsMutex;

	/**
	 * @brief Map from topic to station UUID, station name, polling period, last archive insertion datetime, time offseter
	 */
	std::map<std::string, std::tuple<CassUuid, std::string, int, date::sys_seconds, TimeOffseter>, std::less<>> _stations;
	decltype(mqtt::make_tls_client(_ioContext, _details.host, _details.port)) _client;

	using packet_id_t = typename std::remove_reference_t<decltype(*_client)>::packet_id_t;

	std::map<packet_id_t, std::string> _subscriptions;

	/**
	 * @brief The channel subscription id
	 */
	packet_id_t _pid = 0;

	static constexpr int MAX_RETRIES_EXPONENTIAL_BACKOFF = 8;

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
	virtual void processArchive(const std::string_view& topicName, const std::string_view& content) = 0;
	virtual const char* getConnectorSuffix() = 0;
	void checkRetryStartDeadline(const std::error_code& e);

	virtual bool handleConnAck(bool sp, mqtt::connect_return_code ret);
	virtual void handleClose();
	virtual void handleError(std::error_code const& ec);
	virtual bool handlePubAck(packet_id_t packetId);
	virtual bool handlePubRec(packet_id_t packetId);
	virtual bool handlePubComp(packet_id_t packetId);
	virtual bool handleSubAck(packet_id_t packetId, std::vector<mqtt::suback_return_code> results);
	virtual bool handlePublish(std::optional<packet_id_t> packet_id, mqtt::publish_options opts,
		std::string_view topic_name, std::string_view contents);

};

bool operator<(const MqttSubscriber::MqttSubscriptionDetails& s1, const MqttSubscriber::MqttSubscriptionDetails& s2);
bool operator==(const MqttSubscriber::MqttSubscriptionDetails& s1, const MqttSubscriber::MqttSubscriptionDetails& s2);

}

#endif
