/**
 * @file liveobjects_downloader_standalone.cpp
 * @brief Definition of the main function
 * @author Laurent Georget
 * @date 2023-04-09
 */
/*
 * Copyright (C) 2023  SAS Météo Concept <contact@meteo-concept.fr>
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
#include <date/date.h>
#include <boost/program_options.hpp>
#include <boost/json.hpp>
#include <boost/json/src.hpp>

#include "cassandra_utils.h"
#include "config.h"
#include "curl_wrapper.h"
#include "liveobjects_api_downloader.h"

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
	std::string apiKey;
	std::vector<std::string> namedStations;
	std::string begin;
	std::string end;

	constexpr char CLIENT_ID[] = "meteodata_standalone";

	po::options_description config("Configuration");
	config.add_options()
		("user,u", po::value<std::string>(&user), "database username")
		("password,p", po::value<std::string>(&password), "database password")
		("host,h", po::value<std::string>(&address), "database IP address or domain name")
		("pguser", po::value<std::string>(&pguser), "PostgreSQL database username")
		("pgpassword", po::value<std::string>(&pgpassword), "PostgreSQL database password")
		("pghost", po::value<std::string>(&pgaddress), "PostgreSQL database IP address or domain name")
		("apikey,k", po::value<std::string>(&apiKey), "Liveobjects API key with appropriate privileges")
	;

	po::options_description desc("Allowed options");
	desc.add_options()
		("help", "display the help message and exit")
		("version", "display the version of Meteodata and exit")
		("config-file", po::value<std::string>(), "alternative configuration file")
		("station", po::value<std::vector<std::string>>(&namedStations)->multitoken(), "the stations to download the data for (can be given multiple times, defaults to all MQTT VP2 stations)")
		("begin", po::value<std::string>(&begin), "Start of the range to recover (by default, 24h ago)")
		("end", po::value<std::string>(&end), "End of the range to recover (by default, now)")
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

	sys_seconds endDate;
	if (vm.count("end")) {
		std::istringstream in{end};
		in >> date::parse("%Y-%m-%d %H:%M", endDate);
		if (!in) {
			std::cerr << "'" << end << "' does not look like a valid date and time, that's problematic (expected format : \"Y-m-d H:M\")" << std::endl;
			return EINVAL;
		}
		if (endDate > system_clock::now()) {
			std::cerr << endDate << " looks like it's in the future, that's problematic" << std::endl;
			return EINVAL;
		}
		if (endDate < beginDate) {
			std::cerr << endDate << " looks like it's before the beginning date, that's problematic" << std::endl;
			return EINVAL;
		}
	} else {
		endDate = date::floor<hours>(system_clock::now());
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

	DbConnectionObservations db{address, user, password, pgaddress, pguser, pgpassword};

	std::vector<std::tuple<CassUuid, std::string, std::string>> liveobjectsStations;
	db.getAllLiveobjectsStations(liveobjectsStations);

	std::cerr << "Got the list of stations from the db" << std::endl;

	curl_global_init(CURL_GLOBAL_SSL);
	CurlWrapper client;

	int retry = 0;
	for (auto it = liveobjectsStations.cbegin() ; it != liveobjectsStations.cend() ;) {
		const auto& station = *it;
		if (!userSelection.empty()) {
			if (userSelection.find(std::get<0>(station)) == userSelection.cend()) {
				++it;
				continue;
			}
		}

		std::cerr << "About to download for station " << std::get<0>(station) << std::endl;
		LiveobjectsApiDownloader downloader{std::get<0>(station), std::get<1>(station), db, apiKey};
		try {
			downloader.download(client, beginDate, endDate, true);
			retry = 0;
			++it;
			std::this_thread::sleep_for(chrono::milliseconds(100));
		} catch (const std::runtime_error& e) {
			retry++;
			if (retry >= 2) {
				std::cerr << "Tried twice already, moving on..." << std::endl;
				retry = 0;
				++it;
			} else {
				throw e;
			}
		} catch (std::exception& e) {
			std::cerr << e.what() << std::endl;
			curl_global_cleanup();
			return 255;
		}
	}

	curl_global_cleanup();
}
