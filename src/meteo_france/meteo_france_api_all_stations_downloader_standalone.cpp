/**
 * @file meteo_france_all_stations_downloader_standalone.cpp
 * @brief Definition of the main function
 * @author Laurent Georget
 * @date 2024-02-23
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
#include <fstream>
#include <memory>
#include <functional>
#include <vector>
#include <chrono>
#include <thread>

#include <cassandra.h>
#include <dbconnection_observations.h>
#include <boost/asio.hpp>
#include <date.h>
#include <boost/program_options.hpp>
#include <boost/json.hpp>
#include <boost/json/src.hpp>

#include "cassandra_utils.h"
#include "config.h"
#include "curl_wrapper.h"
#include "meteo_france_api_6m_downloader.h"
#include "meteo_france_api_downloader.h"

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
	std::string apiKey;
	std::vector<std::string> namedStations;
	std::string begin;
	std::string end;

	constexpr char CLIENT_ID[] = "meteodata_all_stations_standalone";
	constexpr char SCHEDULER_ID[] = "meteo_france";

	po::options_description config("Configuration");
	config.add_options()
		("user,u", po::value<std::string>(&user), "database username")
		("password,p", po::value<std::string>(&password), "database password")
		("host,h", po::value<std::string>(&address), "database IP address or domain name")
		("meteofrance-key,k", po::value<std::string>(&apiKey), "Météo France API key from an appropriate subscription")
	;

	po::options_description desc("Allowed options");
	desc.add_options()
		("help", "display the help message and exit")
		("version", "display the version of Meteodata and exit")
		("config-file", po::value<std::string>(), "alternative configuration file")
		("begin", po::value<std::string>(&begin), "Start of the range to recover (by default, last download time)")
		("end", po::value<std::string>(&end), "End of the range to recover (by default, now, will update the last download time in this case)")
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

	DbConnectionObservations db{address, user, password};

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
		time_t time;
		bool ret = db.getLastSchedulerDownloadTime(SCHEDULER_ID, time);
		if (ret) {
			beginDate = date::floor<seconds>(chrono::system_clock::from_time_t(time)) + MeteoFranceApi6mDownloader::UpdatePeriod{1};
		} else {
			beginDate = date::floor<hours>(system_clock::now()) - chrono::hours(1);
		}
	}

	sys_seconds endDate;
	bool updateLastDownloadDate = false;
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
		updateLastDownloadDate = true;
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

	std::vector<std::tuple<CassUuid, std::string, std::string, int, float, float, int, int>> mfStations;
	db.getMeteoFranceStations(mfStations);

	std::cerr << "Got the list of stations from the db: " << mfStations.size() << " stations" << std::endl;

	curl_global_init(CURL_GLOBAL_SSL);
	CurlWrapper client;

	int retry = 0;
	auto d = date::floor<MeteoFranceApi6mDownloader::UpdatePeriod>(beginDate);
	while (d <= endDate) {
		std::cerr << "About to download for time " << date::format("%Y-%m-%dT%H:%M:%SZ", d) << std::endl;
		auto tick = chrono::system_clock::now();

		MeteoFranceApi6mDownloader downloader{db, apiKey};
		try {
			downloader.download(client, d);
			retry = 0;
		} catch (const std::runtime_error& e) {
			retry++;
			if (retry >= 2) {
				std::cerr << "Tried twice already, moving on..." << std::endl;
				retry = 0;
			} else {
				throw e;
			}
		} catch (std::exception& e) {
			std::cerr << e.what() << std::endl;
			curl_global_cleanup();
			return 255;
		}

		if (updateLastDownloadDate) {
			// might fail but there's nothing more we can do about it
			bool ret = db.insertLastSchedulerDownloadTime(SCHEDULER_ID, chrono::system_clock::to_time_t(d));
			if (!ret) {
				std::cerr << "Failed updating the last download time" << std::endl;
			}
		}

		d += MeteoFranceApi6mDownloader::UpdatePeriod{1};
		auto elapsed = chrono::system_clock::now() - tick;
		if (elapsed < MeteoFranceApiDownloader::MIN_DELAY) {
			// cap at 50 requests / minute
			std::this_thread::sleep_for(MeteoFranceApiDownloader::MIN_DELAY - elapsed);
		}
	}

	curl_global_cleanup();
}
