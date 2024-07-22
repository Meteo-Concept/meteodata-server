/**
 * @file chirpstack_mqtt_subscriber_standalone.cpp
 * @brief Definition of the main function
 * @author Laurent Georget
 * @date 2022-07-04
 */
/*
 * Copyright (C) 2022  SAS Météo Concept <contact@meteo-concept.fr>
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

#include <iostream>
#include <fstream>
#include <memory>
#include <functional>
#include <vector>
#include <chrono>
#include <thread>
#include <tuple>
#include <map>

#include <cassandra.h>
#include <dbconnection_observations.h>
#include <boost/asio.hpp>
#include <boost/json/src.hpp>
#include <mqtt_client_cpp.hpp> // must be kept before boost/program_options.hpp else build breaks (not sure why...)
#include <date.h>
#include <boost/program_options.hpp>

#include "../cassandra_utils.h"
#include "config.h"
#include "mqtt/mqtt_subscriber.h"
#include "mqtt/chirpstack_mqtt_subscriber.h"

/**
 * @brief The configuration file default path
 */
#define DEFAULT_CONFIG_FILE "/etc/meteodata/db_credentials"

#define DEFAULT_VERIFY_PATH "/etc/ssl/certs"

using namespace meteodata;
namespace po = boost::program_options;
namespace asio = boost::asio;
namespace sys = boost::system;

using namespace std::chrono;
using namespace date;

/**
 * @brief Entry point
 *
 * @param argc the number of arguments passed on the command line
 * @param argv the arguments passed on the command line
 *
 * @return 0 if everything went well, and either an "errno-style" error code
 * or 255 otherwise
 */
int main(int argc, char** argv)
{
	std::string user;
	std::string password;
	std::string address;
	std::vector<std::string> namedStations;
	std::string mqttAddress;
	int mqttPort;
	std::string mqttUser;
	std::string mqttPassword;
	std::string begin;

	constexpr char CLIENT_ID[] = "meteodata_standalone_chirpstack";

	po::options_description config("Configuration");
	config.add_options()
		("user,u", po::value<std::string>(&user), "database username")
		("password,p", po::value<std::string>(&password), "database password")
		("host,h", po::value<std::string>(&address), "database IP address or domain name")
		("mqtt-host", po::value<std::string>(&mqttAddress), "MQTT broker IP address or domain name")
		("mqtt-port", po::value<int>(&mqttPort), "MQTT port")
		("mqtt-user", po::value<std::string>(&mqttUser), "MQTT user name")
		("mqtt-password", po::value<std::string>(&mqttPassword), "MQTT password")
	;

	po::options_description desc("Allowed options");
	desc.add_options()
		("help", "display the help message and exit")
		("version", "display the version of Meteodata and exit")
		("config-file", po::value<std::string>(), "alternative configuration file")
		("station", po::value<std::vector<std::string>>(&namedStations)->multitoken(), "the stations to download the data for (can be given multiple times, defaults to all MQTT Chirpstack stations)")
	;
	desc.add(config);

	po::variables_map vm;
	po::store(po::parse_command_line(argc, argv, desc), vm);
	std::string configFileName = vm.count("config-file") ? vm["config-file"].as<std::string>() : DEFAULT_CONFIG_FILE;
	std::ifstream configFile(configFileName);
	if (configFile) {
		po::store(po::parse_config_file(configFile, config, true), vm);
		configFile.close();
	}
	po::notify(vm);

	if (vm.count("help")) {
		std::cout << PACKAGE_STRING"\n";
		std::cout << "Usage: " << argv[0] << " [-h cassandra_host -u user -p password --mqtt-host host --mqtt-port 1883 --mqtt-user \"\" --mqtt password \"\"]\n";
		std::cout << desc << "\n";
		std::cout << "You must give either both the username and "
					 "password or none of them." << std::endl;

		return 0;
	}

	if (vm.count("version")) {
		std::cout << VERSION << std::endl;
		return 0;
	}

	std::set<CassUuid> userSelection;
	if (vm.count("station")) {
		for (const auto& st : namedStations) {
			CassUuid uuid;
			CassError res = cass_uuid_from_string(st.c_str(), &uuid);
			if (res != CASS_OK) {
				std::cerr << "'" << st << "' does not look like a valid UUID, ignoring" << std::endl;
				continue;
			}
			userSelection.emplace(uuid);
		}
	}

	cass_log_set_level(CASS_LOG_INFO);
	CassLogCallback logCallback =
		[](const CassLogMessage *message, void*) -> void {
			std::string logLevel =
				message->severity == CASS_LOG_CRITICAL ? "critical" :
				message->severity == CASS_LOG_ERROR    ? "error" :
				message->severity == CASS_LOG_WARN     ? "warning" :
				message->severity == CASS_LOG_INFO     ? "info" :
									 "debug";

		std::cerr << logLevel << ": " << message->message << " (from " << message->function << ", in "
				  << message->file << ", line " << message->line << std::endl;
	};
	cass_log_set_callback(logCallback, nullptr);

	asio::io_context ioContext;
	DbConnectionObservations db{address, user, password};

	std::vector<std::tuple<CassUuid, std::string, int, std::string, std::unique_ptr<char[]>, size_t, std::string, int>> mqttStations;
	db.getMqttStations(mqttStations);
	std::cerr << "Got the list of stations from the db" << std::endl;

	std::map<MqttSubscriber::MqttSubscriptionDetails, std::shared_ptr<ChirpstackMqttSubscriber>> chirpstackMqttSubscribers;

	for (auto&& station : mqttStations) {
		const CassUuid& uuid = std::get<0>(station);
		const std::string& topic = std::get<6>(station);

		if (topic.substr(0, 11) != "chirpstack/") {
			continue;
		}
		if (!userSelection.empty()) {
			if (userSelection.find(uuid) == userSelection.cend()) {
				continue;
			}
		}

		MqttSubscriber::MqttSubscriptionDetails details{std::get<1>(station), std::get<2>(station),
			std::get<3>(station), std::string(std::get<4>(station).get(), std::get<5>(station))};
		TimeOffseter::PredefinedTimezone tz{std::get<7>(station)};

		auto mqttSubscribersIt = chirpstackMqttSubscribers.find(details);
		if (mqttSubscribersIt == chirpstackMqttSubscribers.end()) {
			std::shared_ptr<ChirpstackMqttSubscriber> subscriber = std::make_shared<ChirpstackMqttSubscriber>(
				details, ioContext, db, nullptr
			);
			mqttSubscribersIt = chirpstackMqttSubscribers.emplace(details, subscriber).first;
		}
		mqttSubscribersIt->second->addStation(topic, uuid, tz);

		std::cerr << "Waiting for message for station " << uuid << std::endl;
	}

	for (auto&& mqttSubscriber: chirpstackMqttSubscribers) {
		mqttSubscriber.second->start();
	}
	ioContext.run();
}
