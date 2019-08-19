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
#include <array>
#include <vector>
#include <functional>
#include <syslog.h>
#include <unistd.h>

#include <boost/system/error_code.hpp>
#include <cassandra.h>
#include <date/date.h>
#include <date/tz.h>
#include <dbconnection_observations.h>
#include <mqtt_client_cpp.hpp>

#include "vantagepro2archivepage.h"
#include "timeoffseter.h"

namespace meteodata {

using namespace meteodata;

/**
 */
class MqttSubscriber : public std::enable_shared_from_this<MqttSubscriber>
{
public:
	struct MqttSubscriptionDetails {
		std::string host;
		int port;
		std::string user;
		std::unique_ptr<char[]> password;
		size_t passwordLength;
		std::string topic;

		MqttSubscriptionDetails(const std::string& host, int port, const std::string& user, std::unique_ptr<char[]>&& password, size_t passwordLength, const std::string& topic);
		MqttSubscriptionDetails(MqttSubscriptionDetails&& other);
	};

	MqttSubscriber(const CassUuid& station, MqttSubscriptionDetails&& details,
		asio::io_service& ioService, DbConnectionObservations& db,
		TimeOffseter::PredefinedTimezone tz);
	void start();
	void stop();

private:
	asio::io_service& _ioService;
	DbConnectionObservations& _db;
	/**
	 * @brief The connected station's identifier in the database
	 */
	CassUuid _station;
	std::string _stationName;
	MqttSubscriptionDetails _details;
	decltype(mqtt::make_tls_client(_ioService, _details.host, _details.port)) _client;
	/**
	 * @brief The amount of time between two queries for data to the stations
	 */
	int _pollingPeriod;

	/**
	 * @brief The timestamp (in POSIX time) of the last archive entry
	 * retrieved from the station
	 */
	date::sys_seconds _lastArchive;

	/**
	 * @brief The \a TimeOffseter to use to convert timestamps between the
	 * station's time and POSIX time
	 */
	TimeOffseter _timeOffseter;

	/**
	 * @brief The channel subscription id
	 */
	std::uint16_t _pid = 0;

	static constexpr char CLIENT_ID[] = "meteodata";
	static constexpr char ARCHIVES_TOPIC[] = "/dmpaft";
	void processArchive(const mqtt::string_view& content);
};

}

#endif
