/**
 * @file nbiot_standalone.cpp
 * @brief Recovery system for NB-IoT from the raw UDP log
 * @author Laurent Georget
 * @date 2024-12-10
 */
/*
 * Copyright (C) 2024  SAS Météo Concept <contact@meteo-concept.fr>
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
#include <iterator>
#include <map>
#include <fstream>
#include <thread>

#include <boost/program_options.hpp>
#include <cassandra.h>

#include "cassandra_utils.h"
#include "nbiot/nbiot_udp_request_handler.h"
#include "config.h"

#define DEFAULT_CONFIG_FILE "/etc/meteodata/db_credentials"

using namespace meteodata;
namespace po = boost::program_options;

int main(int argc, char** argv)
{
	std::string user;
	std::string password;
	std::string address;
	std::string pguser;
	std::string pgpassword;
	std::string pgaddress;
	std::vector<std::string> namedStations;
	std::string inputFile;

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
		("station", po::value<std::vector<std::string>>(&namedStations)->multitoken(), "the stations to get the data for (can be given multiple times, defaults to all stations)")
		("input-file", po::value<std::string>(&inputFile), "the UDP raw log file, one message per line, in ASCII-encoded hexadecimal")
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
		std::cout << "Usage: " << argv[0] << " [-h cassandra_host -u user -p password]\n";
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
		if (userSelection.empty()) {
			std::cerr << "No valid station selected, aborting" << std::endl;
			return 1;
		}
	}

	cass_log_set_level(CASS_LOG_INFO);
	CassLogCallback logCallback = [](const CassLogMessage* message, void*) -> void {
		std::string logLevel =
				message->severity == CASS_LOG_CRITICAL ? "critical" :
				message->severity == CASS_LOG_ERROR ? "error" :
				message->severity == CASS_LOG_WARN ? "warning" :
				message->severity == CASS_LOG_INFO ? "info" : "debug";

		std::cerr << logLevel << ": " << message->message << " (from " << message->function << ", in "
				  << message->file << ", line " << message->line << std::endl;
	};
	cass_log_set_callback(logCallback, nullptr);

	DbConnectionObservations db{address, user, password, pgaddress, pguser, pgpassword};

	std::vector<NbiotStation> nbiotStations;
	db.getAllNbiotStations(nbiotStations);
	if (!userSelection.empty()) {
		std::vector<NbiotStation> finalSelection;
		for (const NbiotStation& st : nbiotStations) {
			if (userSelection.find(st.station) != userSelection.end()) {
				finalSelection.emplace_back(std::move(st));
			}
		}
		nbiotStations = std::move(finalSelection);
	}

	NbiotUdpRequestHandler processor{db};
	processor.loadSelectionOfStations(nbiotStations.begin(), nbiotStations.end());

	std::ifstream is{inputFile};

	if (!is) {
		std::cerr << "Impossible to open " << inputFile << ", aborting\n";
		return 1;
	}

	std::string s;
	for (std::getline(is, s) ; is ; std::getline(is, s)) {
		processor.processHexifiedRequest(s);
	}
}
