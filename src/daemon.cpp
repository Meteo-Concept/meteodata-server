/**
 * @file daemon.cpp
 * @brief Definition of the main function
 * @author Laurent Georget
 * @date 2016-10-05
 */
/*
 * Copyright (C) 2016  SAS Météo Concept <contact@meteo-concept.fr>
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
#include <unistd.h>
#include <syslog.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <tuple>

#include <boost/asio.hpp>
#include <boost/program_options.hpp>
#include <curl/curl.h>
#include <dbconnection_observations.h>

#include "config.h"
#include "meteo_server.h"

/**
 * @brief The configuration file default path
 */
#define DEFAULT_CONFIG_FILE "/etc/meteodata/db_credentials"

using namespace meteodata;
namespace po = boost::program_options;

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
	std::string weatherlinkApiV2Key;
	std::string weatherlinkApiV2Secret;
	std::string fieldClimateKey;
	std::string fieldClimateSecret;
	unsigned long threads;

	po::options_description config("Configuration");
	config.add_options()
		("user,u", po::value<std::string>(&user), "database username")
		("password,p", po::value<std::string>(&password), "database password")
		("host,h", po::value<std::string>(&address), "database IP address or domain name")
		("weatherlink-apiv2-key,k", po::value<std::string>(&weatherlinkApiV2Key), "api.weatherlink.com/v2/ key")
		("weatherlink-apiv2-secret,s", po::value<std::string>(&weatherlinkApiV2Secret), "api.weatherlink.com/v2/ secret")
		("threads", po::value<unsigned long>(&threads), "number of threads to start to listen to ASIO events, defaults to 5")
		("fieldclimate-key,k", po::value<std::string>(&fieldClimateKey), "api.fieldclimate.com key")
		("fieldclimate-secret,s", po::value<std::string>(&fieldClimateSecret), "api.fieldclimate.com secret")
	;

	po::options_description desc("Allowed options");
	desc.add_options()
		("help", "display the help message and exit")
		("version", "display the version of Meteodata and exit")
		("config-file", po::value<std::string>(), "alternative configuration file")
		("no-daemon,D", "do not daemonize at startup")
	;
	desc.add(config);

	po::variables_map vm;
	po::store(po::parse_command_line(argc, argv, desc), vm);
	std::string configFileName = vm.count("config-file") ?
		vm["config-file"].as<std::string>() :
		DEFAULT_CONFIG_FILE;
	std::ifstream configFile(configFileName);
	if (configFile) {
		po::store(po::parse_config_file(configFile, config, true), vm);
		configFile.close();
	}
	po::notify(vm);

	if (vm.count("help")) {
		std::cout << PACKAGE_STRING"\n";
		std::cout << "Usage: " << argv[0] << " [-h cassandra_host -u user -p password -k weatherlink-apiv2-key -s weatherlink-apiv2-secret]\n";
		std::cout << desc << "\n";
		std::cout << "You must give either both the username and "
			"password or none of them." << std::endl;

		return 0;
	}

	if (vm.count("version")) {
		std::cout << VERSION << std::endl;
		return 0;
	}

	if (!vm.count("no-daemon")) {
		int daemonization = daemon(0,0);

		if (daemonization) {
			int errsv = errno;
			perror("Could not start Meteodata");
			return errsv;
		}
	}

	openlog("meteodata", LOG_PID, LOG_DAEMON);
	cass_log_set_level(CASS_LOG_INFO);
	CassLogCallback logCallback =
		[](const CassLogMessage *message, void*) -> void {
			int logLevel =
				message->severity == CASS_LOG_CRITICAL ? LOG_CRIT :
				message->severity == CASS_LOG_ERROR    ? LOG_ERR :
				message->severity == CASS_LOG_WARN     ? LOG_WARNING :
				message->severity == CASS_LOG_INFO     ? LOG_INFO :
									 LOG_DEBUG;

			syslog(logLevel, "%s (from %s, in %s, line %i)", message->message, message->function, message->file, message->line);
		};
	cass_log_set_callback(logCallback, NULL);

	try {
		curl_global_init(CURL_GLOBAL_SSL);
		boost::asio::io_service ioService;
		MeteoServer server(ioService, address, user, password, weatherlinkApiV2Key, weatherlinkApiV2Secret, fieldClimateKey, fieldClimateSecret);
		server.start();

		std::vector<std::thread> workers;
		// start all of the workers
		for (unsigned long i=0 ; i<threads ; i++) {
			workers.emplace_back([&]() { ioService.run(); });
		}
		// and wait for them to die
		for (unsigned long i=0 ; i<threads ; i++) {
			workers[i].join();
		}
	} catch (std::exception& e) {
		// exit on error, and let systemd restart the daemon
		std::cerr << e.what() << std::endl;
		curl_global_cleanup();
		syslog(LOG_ERR, "%s", e.what());
		closelog();
		return 255;
	}

	// clean exit, not reached in daemon mode
	curl_global_cleanup();
	closelog();
}
