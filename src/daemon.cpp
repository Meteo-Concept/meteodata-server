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
#include <systemd/sd-daemon.h>
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
	MeteoServer::MeteoServerConfiguration serverConfig;
	unsigned long threads = 1;
	bool daemonized;

	po::options_description config("Configuration");
	config.add_options()
		("user,u", po::value<std::string>(&serverConfig.user), "database username")
		("password,p", po::value<std::string>(&serverConfig.password), "database password")
		("host,h", po::value<std::string>(&serverConfig.address), "database IP address or domain name")
		("pguser", po::value<std::string>(&serverConfig.user), "PostgreSQL database username")
		("pgpassword", po::value<std::string>(&serverConfig.password), "PostgreSQL database password")
		("pghost", po::value<std::string>(&serverConfig.address), "PostgreSQL database IP address or domain name")
		("jobs-db-user", po::value<std::string>(&serverConfig.jobsDbUsername), "asynchronous jobs database username")
		("jobs-db-password", po::value<std::string>(&serverConfig.jobsDbPassword), "asynchronous jobs database password")
		("jobs-db-host", po::value<std::string>(&serverConfig.jobsDbAddress), "asynchronous jobs database IP address or domain name")
		("jobs-db-database", po::value<std::string>(&serverConfig.jobsDbDatabase), "asynchronous jobs database name")
		("weatherlink-apiv2-key,k", po::value<std::string>(&serverConfig.weatherlinkApiV2Key), "api.weatherlink.com/v2/ key")
		("weatherlink-apiv2-secret,s", po::value<std::string>(&serverConfig.weatherlinkApiV2Secret), "api.weatherlink.com/v2/ secret")
		("threads", po::value<unsigned long>(&threads), "number of threads to start to listen to ASIO events, defaults to 1")
		("fieldclimate-key", po::value<std::string>(&serverConfig.fieldClimateApiKey), "api.fieldclimate.com key")
		("fieldclimate-secret", po::value<std::string>(&serverConfig.fieldClimateApiSecret), "api.fieldclimate.com secret")
		("objenious-key", po::value<std::string>(&serverConfig.objeniousApiKey), "api.objenious.com key")
		("meteofrance-key", po::value<std::string>(&serverConfig.meteofranceApiKey), "Météo France developer portal API key")
	;

	po::options_description desc("Allowed options");
	desc.add_options()
		("help", "display the help message and exit")
		("version", "display the version of Meteodata and exit")
		("config-file", po::value<std::string>(), "alternative configuration file")
		("no-daemon,D", "tell the program that it's not daemonized and that it should not try to notify systemd")
		("no-async-jobs", "tell the program that it shouldn't try to schedule asynchronous jobs")
		("no-mqtt", "don't start the MQTT downloaders")
		("no-synop", "don't start the SYNOP downloaders")
		("no-ship", "don't start the SHIP and BUOY downloader")
		("no-meteofrance", "don't start the Météo France API downloader")
		("no-static", "don't start the StatIC downloaders")
		("no-weatherlink", "don't start the Weatherlink downloaders")
		("no-weatherlink-v2", "don't start the Weatherlink APIv2 downloaders")
		("no-fieldclimate", "don't start the Fieldclimate downloaders")
		("no-mbdata", "don't start the MBData downloaders")
		("no-virtual", "don't start the virtual observations computer")
		("no-rest", "don't start the REST server")
		("no-vp2", "don't start the main Vantage Pro 2 server")
		("no-udp", "don't start the UDP server")
		("only-mqtt", "start only the MQTT downloaders")
		("only-synop", "start only the SYNOP downloaders")
		("only-ship", "start only the SHIP and BUOY downloader")
		("only-meteofrance", "start only the Météo France API downloader")
		("only-static", "start only the StatIC downloaders")
		("only-weatherlink", "start only the Weatherlink downloaders")
		("only-weatherlink-v2", "start only the Weatherlink APIv2 downloaders")
		("only-fieldclimate", "start only the Fieldclimate downloaders")
		("only-mbdata", "start only the MBData downloaders")
		("only-virtual", "start only the virtual observations computer")
		("only-rest", "start only the REST server")
		("only-vp2", "start only the main Vantage Pro 2 server")
		("only-udp", "start only the UDP server")
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
		std::cout << "Usage: " << argv[0]
				  << " [-h cassandra_host -u user -p password -k weatherlink-apiv2-key -s weatherlink-apiv2-secret]\n";
		std::cout << desc << "\n";
		std::cout << "You must give either both the username and "
					 "password or none of them." << std::endl;

		return 0;
	}

	if (vm.count("version")) {
		std::cout << VERSION << std::endl;
		return 0;
	}

	daemonized = !vm.count("no-daemon");
	serverConfig.publishJobs = !vm.count("no-jobs");

	serverConfig.startMqtt = !vm.count("no-mqtt");
	serverConfig.startSynop = !vm.count("no-synop");
	serverConfig.startShip = !vm.count("no-ship");
	serverConfig.startMeteoFrance = !vm.count("no-meteofrance");
	serverConfig.startStatic = !vm.count("no-static");
	serverConfig.startWeatherlink = !vm.count("no-weatherlink");
	serverConfig.startWeatherlinkV2 = !vm.count("no-weatherlink-v2");
	serverConfig.startFieldclimate = !vm.count("no-fieldclimate");
	serverConfig.startMbdata = !vm.count("no-mbdata");
	serverConfig.startRest = !vm.count("no-rest");
	serverConfig.startVp2 = !vm.count("no-vp2");
	serverConfig.startUdp = !vm.count("no-udp");

	if (vm.count("only-mqtt") || vm.count("only-synop") || vm.count("only-ship") ||
		vm.count("only-meteofrance") || vm.count("only-static") ||
		vm.count("only-weatherlink") || vm.count("only-weatherlink-v2") || vm.count("only-fieldclimate") ||
		vm.count("only-mbdata") || vm.count("only-virtual") ||
		vm.count("only-rest") || vm.count("only-vp2") || vm.count("only-udp")) {
		serverConfig.startMqtt = false;
		serverConfig.startSynop = false;
		serverConfig.startShip = false;
		serverConfig.startMeteoFrance = false;
		serverConfig.startStatic = false;
		serverConfig.startWeatherlink = false;
		serverConfig.startWeatherlinkV2 = false;
		serverConfig.startFieldclimate = false;
		serverConfig.startMbdata = false;
		serverConfig.startVirtual = false;
		serverConfig.startRest = false;
		serverConfig.startVp2 = false;
		serverConfig.startUdp = false;

		if (vm.count("only-mqtt"))
			serverConfig.startMqtt = true;
		if (vm.count("only-synop"))
			serverConfig.startSynop = true;
		if (vm.count("only-ship"))
			serverConfig.startShip = true;
		if (vm.count("only-meteofrance"))
			serverConfig.startMeteoFrance = true;
		if (vm.count("only-static"))
			serverConfig.startStatic = true;
		if (vm.count("only-weatherlink"))
			serverConfig.startWeatherlink = true;
		if (vm.count("only-weatherlink-v2"))
			serverConfig.startWeatherlinkV2 = true;
		if (vm.count("only-fieldclimate"))
			serverConfig.startFieldclimate = true;
		if (vm.count("only-mbdata"))
			serverConfig.startMbdata = true;
		if (vm.count("only-virtual"))
			serverConfig.startVirtual = true;
		if (vm.count("only-rest"))
			serverConfig.startRest = true;
		if (vm.count("only-vp2"))
			serverConfig.startVp2 = true;
		if (vm.count("only-udp"))
			serverConfig.startUdp = true;
	}

	if (daemonized) {
		serverConfig.daemonized = true;
	}

	cass_log_set_level(CASS_LOG_INFO);
	CassLogCallback logCallback =
		[](const CassLogMessage *message, void*) -> void {
			const char* logLevel =
				message->severity == CASS_LOG_CRITICAL ? SD_CRIT :
				message->severity == CASS_LOG_ERROR    ? SD_ERR :
				message->severity == CASS_LOG_WARN     ? SD_WARNING :
				message->severity == CASS_LOG_INFO     ? SD_INFO :
									 SD_DEBUG;

		std::cerr << logLevel << "[Cassandra] database: " << message->message << "(from " << message->function
				  << ", in " << message->file << ", line " << message->line << ")" << std::endl;
	};
	cass_log_set_callback(logCallback, nullptr);

	try {
		curl_global_init(CURL_GLOBAL_SSL);
		boost::asio::io_context ioContext;

		MeteoServer server(ioContext, std::move(serverConfig));
		server.start();

		std::vector<std::thread> workers;
		// start all the workers
		for (unsigned long i = 0 ; i < threads ; i++) {
			workers.emplace_back([&]() { ioContext.run(); });
		}
		if (daemonized)
			sd_notifyf(0, "READY=1\n" "STATUS=Data collection started\n" "MAINPID=%d", getpid());
		// and wait for them to die
		for (unsigned long i = 0 ; i < threads ; i++) {
			workers[i].join();
		}
	} catch (std::exception& e) {
		// exit on error, and let systemd restart the daemon
		std::cerr << SD_CRIT << e.what() << std::endl;
		if (daemonized)
			sd_notifyf(0, "STATUS=Critical error met: %s, bailing off\n" "ERRNO=255", e.what());
		curl_global_cleanup();
		return 255;
	}

	// clean exit, not reached in daemon mode
	curl_global_cleanup();
}
