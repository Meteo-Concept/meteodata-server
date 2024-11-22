/**
 * @file minmax_daemon.cpp
 * @brief Minmax recomputing program
 * @author Laurent Georget
 * @date 2023-07-26
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
#include <unistd.h>
#include <systemd/sd-daemon.h>
#include <tuple>

#include <boost/asio.hpp>
#include <boost/program_options.hpp>
#include <cassobs/dbconnection_minmax.h>
#include <cassobs/dbconnection_jobs.h>
#include <date.h>

#include "config.h"
#include "minmax/minmax_worker.h"

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
	MinmaxWorker::Configuration serverConfig;
	bool daemonized;

	po::options_description config("Configuration");
	config.add_options()
		("user,u", po::value<std::string>(&serverConfig.user), "database username")
		("password,p", po::value<std::string>(&serverConfig.password), "database password")
		("host,h", po::value<std::string>(&serverConfig.address), "database IP address or domain name")
		("jobs-db-user", po::value<std::string>(&serverConfig.jobsDbUsername), "asynchronous jobs database username")
		("jobs-db-password", po::value<std::string>(&serverConfig.jobsDbPassword), "asynchronous jobs database password")
		("jobs-db-host", po::value<std::string>(&serverConfig.jobsDbAddress), "asynchronous jobs database IP address or domain name")
		("jobs-db-database", po::value<std::string>(&serverConfig.jobsDbDatabase), "asynchronous jobs database name")
		("threads", po::value<unsigned long>(&serverConfig.threads), "number of threads to start to listen to ASIO events, defaults to 1")
	;

	po::options_description desc("Allowed options");
	desc.add_options()
		("help", "display the help message and exit")
		("version", "display the version of Meteodata and exit")
		("config-file", po::value<std::string>(), "alternative configuration file")
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
		std::cout << desc << "\n";

		return 0;
	}

	if (vm.count("version")) {
		std::cout << VERSION << std::endl;
		return 0;
	}

	daemonized = !vm.count("no-daemon");

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
		boost::asio::io_context ioContext;

		MinmaxWorker minmaxer{serverConfig, ioContext};
		minmaxer.start();

		std::vector<std::thread> workers;
		// start all the workers
		for (unsigned long i = 0 ; i < serverConfig.threads ; i++) {
			workers.emplace_back([&]() { ioContext.run(); });
		}
		if (daemonized)
			sd_notifyf(0, "READY=1\n" "STATUS=Minmax recomputing monitor started\n" "MAINPID=%d", getpid());
		// and wait for them to die
		for (unsigned long i = 0 ; i < serverConfig.threads ; i++) {
			workers[i].join();
		}
	} catch (std::exception& e) {
		// exit on error, and let systemd restart the daemon
		std::cerr << SD_CRIT << e.what() << std::endl;
		if (daemonized)
			sd_notifyf(0, "STATUS=Critical error met: %s, bailing off\n" "ERRNO=255", e.what());
		return 255;
	}
}
