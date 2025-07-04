/**
 * @file mqtt_pyload_ingester_standalone.cpp
 * @brief Definition of the main function
 * @author Laurent Georget
 * @date 2025-05-20
 */
/*
 * Copyright (C) 2025  SAS Météo Concept <contact@meteo-concept.fr>
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
#include <sstream>
#include <memory>
#include <functional>
#include <vector>
#include <chrono>
#include <tuple>
#include <map>

#include <cassandra.h>
#include <cassobs/dbconnection_observations.h>
#include <date/date.h>
#include <boost/json/src.hpp>
#include <boost/program_options.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include "cassandra_utils.h"
#include "liveobjects/liveobjects_message.h"
#include "config.h"

/**
 * @brief The configuration file default path
 */
#define DEFAULT_CONFIG_FILE "/etc/meteodata/db_credentials"

using namespace meteodata;
namespace po = boost::program_options;

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
	std::string namedStation;
	std::string file;
	float baseValue;

	po::options_description config("Configuration");
	config.add_options()
		("user,u", po::value<std::string>(&user), "database username")
		("password,p", po::value<std::string>(&password), "database password")
		("host,h", po::value<std::string>(&address), "database IP address or domain name")
		("pguser", po::value<std::string>(&pguser), "PostgreSQL database username")
		("pgpassword", po::value<std::string>(&pgpassword), "PostgreSQL database password")
		("pghost", po::value<std::string>(&pgaddress), "PostgreSQL database IP address or domain name")
	;

	po::options_description desc("Allowed options");
	desc.add_options()
		("help", "display the help message and exit")
		("version", "display the version of Meteodata and exit")
		("config-file", po::value<std::string>(), "alternative configuration file")
		("station", po::value<std::string>(&namedStation), "the station to ingest the data for")
		("data-file", po::value<std::string>(&file), "Four-column tab-separated file with in order on each row: the datetime, the port, the sensor type, the hexadecimal-encoded payload")
		("base-value", po::value<float>(&baseValue), "A base counter for accumulated values")
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
		std::cout << "Usage: " << argv[0] << " [-h cassandra_host -u user -p password] --station station\n";
		std::cout << desc << "\n";
		std::cout << "You must give either both the username and "
			     "password or none of them." << std::endl;

		return 0;
	}

	if (vm.count("version")) {
		std::cout << VERSION << std::endl;
		return 0;
	}

	if (!vm.count("station")) {
		std::cout << PACKAGE_STRING"\n";
		std::cout << "Usage: " << argv[0] << " [-h cassandra_host -u user -p password] --station station\n";
		std::cout << "It's mandatory to give the station to ingest the data for." << "\n";

		return 1;
	}
	CassUuid uuid;
	CassError res = cass_uuid_from_string(namedStation.c_str(), &uuid);
	if (res != CASS_OK) {
		std::cerr << "'" << namedStation << "' does not look like a valid UUID, aborting" << std::endl;
		return 1;
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

	DbConnectionObservations db{address, user, password, pgaddress, pguser, pgpassword};

	std::vector<std::tuple<CassUuid, std::string, int, std::string, std::unique_ptr<char[]>, size_t, std::string, int>> mqttStations;
	db.getMqttStations(mqttStations);
	std::cerr << "Got the list of stations from the db" << std::endl;

	auto it = std::find_if(mqttStations.begin(), mqttStations.end(), [&uuid](auto&& st) { return std::get<0>(st) == uuid; });
	if (it == mqttStations.end()) {
		std::cerr << "Station not found among the MQTT stations, aborting" << std::endl;
		return 1;
	}

	auto forcedBaseValue = vm.count("base-value") ? std::optional<float>{baseValue} : std::nullopt;

	std::ifstream input{file};
	std::string line;
	int nbLines = 0;
	std::getline(input, line);
	while (input) {
		nbLines++;

		date::sys_seconds d;
		int fport;
		std::string sensorType;
		std::string payload;

		std::istringstream is{line};
		is >> date::parse("%Y-%m-%dT%H:%M:%S", d)
		   >> fport
		   >> sensorType
		   >> payload;

		if (!is) {
			std::cerr << "Invalid input at line " << nbLines << ": "
				  << line << " (" << d << "," << fport << "," << sensorType << "," << payload << ")\n"
				  << "Aborting" << std::endl;
			return 2;
		}

		std::unique_ptr<LiveobjectsMessage> m = LiveobjectsMessage::instantiateMessage(db, sensorType, fport, uuid, forcedBaseValue);
		if (m) {
			m->ingest(uuid, payload, d);
		}

		if (m && m->looksValid()) {
			Observation o = m->getObservation(uuid);
			if (!db.insertV2DataPoint(o) || !db.insertV2DataPointInTimescaleDB(o)) {
				std::cerr << "Failed to store archive" << std::endl;
			}
			forcedBaseValue = m->getSingleCachedValue();
		} else {
			std::cerr << "Record looks invalid, discarding " << std::endl;
		}

		if (nbLines % 100 == 0) {
			std::cout << "Ingested " << nbLines << " so far: " << date::format("%Y-%m-%dT%H:%M:%S", d) << "\n";
		}

		std::getline(input, line);
	}

	std::cout << nbLines << " lines ingested.";
}
