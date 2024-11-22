/**
 * @file vp2_mqtt_subscriber_standalone.cpp
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

#include <cassandra.h>
#include <cassobs/dbconnection_observations.h>
#include <boost/asio.hpp>
#include <mqtt_client_cpp.hpp> // must be kept before boost/program_options.hpp else build breaks (not sure why...)
#include <date/date.h>
#include <boost/program_options.hpp>

#include "../cassandra_utils.h"
#include "config.h"

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
	std::string pguser;
	std::string pgpassword;
	std::string pgaddress;
	std::vector<std::string> namedStations;
	std::string mqttAddress;
	int mqttPort;
	std::string mqttUser;
	std::string mqttPassword;
	std::string begin;

	constexpr char CLIENT_ID[] = "meteodata_standalone";

	po::options_description config("Configuration");
	config.add_options()
		("user,u", po::value<std::string>(&user), "database username")
		("password,p", po::value<std::string>(&password), "database password")
		("host,h", po::value<std::string>(&address), "database IP address or domain name")
		("pguser", po::value<std::string>(&pguser), "PostgreSQL database username")
		("pgpassword", po::value<std::string>(&pgpassword), "PostgreSQL database password")
		("pghost", po::value<std::string>(&pgaddress), "PostgreSQL database IP address or domain name")
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
		("station", po::value<std::vector<std::string>>(&namedStations)->multitoken(), "the stations to download the data for (can be given multiple times, defaults to all MQTT VP2 stations)")
		("begin", po::value<std::string>(&begin), "Start of the range to recover (by default, 24h ago)")
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

	sys_seconds beginDate;
	if (vm.count("begin")) {
		std::istringstream in{begin};
		in >> date::parse("%Y-%m-%d %H:%M", beginDate);
		if (!in) {
			std::cerr << "'" << begin << "' does not look like a valid date and time, that's problematic (expected format : \"Y-m-d H:M\")" << std::endl;
			return EINVAL;
		}
		if (beginDate > system_clock::now()) {
			std::cerr << beginDate << " looks like it's in the future, that's problematic" << std::endl;
			return EINVAL;
		}
	} else {
		beginDate = date::floor<hours>(system_clock::now()) - date::days(1);
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
	DbConnectionObservations db{address, user, password, pgaddress, pguser, pgpassword};

	std::vector<std::tuple<CassUuid, std::string, int, std::string, std::unique_ptr<char[]>, size_t, std::string, int>> mqttStations;
	auto client = mqtt::make_tls_sync_client(ioContext, mqttAddress, mqttPort);

	client->set_client_id(CLIENT_ID);
	client->set_user_name(mqttUser);
	client->set_password(mqttPassword);
	client->set_clean_session(true);
	client->add_verify_path(DEFAULT_VERIFY_PATH);
	client->set_verify_mode(asio::ssl::verify_none);

	client->set_connack_handler([&](bool sp, std::uint8_t ret) {
		std::cerr << "Connected" << std::endl;

		if (ret != mqtt::connect_return_code::accepted)
			return false;

		db.getMqttStations(mqttStations);
		std::cerr << "Got the list of stations from the db" << std::endl;

		for (const auto & station : mqttStations) {
			CassUuid uuid = std::get<0>(station);
			const std::string& topic = std::get<6>(station);
			if (topic.substr(0, 4) != "vp2/" || topic.rfind("/dmpaft") != topic.size() - 7) {
				continue;
			}
			if (!userSelection.empty()) {
				if (userSelection.find(uuid) == userSelection.cend()) {
					continue;
				}
			}

			std::cerr << "About to download for station " << uuid << std::endl;
			client->publish(topic.substr(0, topic.size() - 7), date::format("DMPAFT %Y-%m-%d %H:%M", beginDate));
			std::this_thread::sleep_for(chrono::milliseconds(500));
		}

		client->disconnect();
		return true;
	});
	client->set_close_handler([]() {
		return true;
	});
	client->set_error_handler([](sys::error_code const& ec) {
		return true;
	});
	client->set_puback_handler([]([[maybe_unused]] std::uint16_t packetId) {
		return true;
	});
	client->set_pubrec_handler([]([[maybe_unused]] std::uint16_t packetId) {
		return true;
	});
	client->set_pubcomp_handler([]([[maybe_unused]] std::uint16_t packetId) {
		return true;
	});
	client->set_suback_handler(
	[](std::uint16_t packetId, const std::vector<boost::optional<std::uint8_t>>& results) {
			return true;
		}
	);
	client->set_publish_handler(
		[](std::uint8_t header, boost::optional<std::uint16_t> packetId, mqtt::string_view topic,
					 mqtt::string_view contents) {
			return true;
		}
	);

	client->connect();
	ioContext.run();
}
