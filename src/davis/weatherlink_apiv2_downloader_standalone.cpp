/**
 * @file weatherlink_download_apiv2_standalone.cpp
 * @brief Definition of the main function
 * @author Laurent Georget
 * @date 2019-09-19
 */
/*
 * Copyright (C) 2019  SAS Météo Concept <contact@meteo-concept.fr>
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
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <tuple>
#include <set>
#include <thread>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/program_options.hpp>
#include <dbconnection_observations.h>
#include <cassandra.h>

#include "config.h"
#include "../cassandra_utils.h"
#include "weatherlink_apiv2_downloader.h"
#include "weatherlink_download_scheduler.h"

/**
 * @brief The configuration file default path
 */
#define DEFAULT_CONFIG_FILE "/etc/meteodata/db_credentials"

using namespace meteodata;
namespace po = boost::program_options;


constexpr char meteodata::WeatherlinkDownloadScheduler::HOST[];
constexpr char meteodata::WeatherlinkDownloadScheduler::APIHOST[];
constexpr int meteodata::WeatherlinkDownloadScheduler::POLLING_PERIOD;

void connectSocket(asio::io_service& ioService, asio::ssl::stream<ip::tcp::socket>& socket)
{
	ip::tcp::resolver resolver(ioService);
	ip::tcp::resolver::query query(WeatherlinkDownloadScheduler::APIHOST, "https");
	ip::tcp::resolver::iterator endpointIterator = resolver.resolve(query);

        if(!SSL_set_tlsext_host_name(socket.native_handle(), WeatherlinkDownloadScheduler::APIHOST))
        {
            sys::error_code ec{static_cast<int>(::ERR_get_error()), boost::asio::error::get_ssl_category()};
            throw boost::system::system_error{ec};
        }
	boost::asio::connect(socket.lowest_layer(), endpointIterator);

	socket.set_verify_mode(asio::ssl::verify_peer);
	socket.set_verify_callback(asio::ssl::rfc2818_verification(WeatherlinkDownloadScheduler::APIHOST));
	socket.handshake(asio::ssl::stream<ip::tcp::socket>::client);
}

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
	std::string weatherlinkApiV2Key;
	std::string weatherlinkApiV2Secret;

	po::options_description config("Configuration");
	config.add_options()
		("user,u", po::value<std::string>(&user), "database username")
		("password,p", po::value<std::string>(&password), "database password")
		("host,h", po::value<std::string>(&address), "database IP address or domain name")
		("weatherlink-apiv2-key,k", po::value<std::string>(&weatherlinkApiV2Key), "api.weatherlink.com/v2/ key")
		("weatherlink-apiv2-secret,s", po::value<std::string>(&weatherlinkApiV2Secret), "api.weatherlink.com/v2/ secret")
	;

	po::options_description desc("Allowed options");
	desc.add_options()
		("help", "display the help message and exit")
		("version", "display the version of Meteodata and exit")
		("config-file", po::value<std::string>(), "alternative configuration file")
		("station", po::value<std::vector<std::string>>(&namedStations)->multitoken(), "the stations for which the min/max must be computed (can be given multiple times, defaults to all stations)")
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

	boost::asio::io_service ioService;
	boost::asio::io_service::work work(ioService);
	std::thread ioServiceRunner{[&ioService] {
		ioService.run();
		std::cerr << "All downloaders have finished executing" << std::endl;
	}};

	try {
		cass_log_set_level(CASS_LOG_INFO);
		CassLogCallback logCallback =
			[](const CassLogMessage *message, void*) -> void {
				std::string logLevel =
					message->severity == CASS_LOG_CRITICAL ? "critical" :
					message->severity == CASS_LOG_ERROR    ? "error" :
					message->severity == CASS_LOG_WARN     ? "warning" :
					message->severity == CASS_LOG_INFO     ? "info" :
										 "debug";

				std::cerr << logLevel << ": " <<  message->message << " (from " << message->function << ", in " << message->file << ", line " << message->line << std::endl;
			};
		cass_log_set_callback(logCallback, NULL);

		// Start the Weatherlink downloaders workers (one per Weatherlink station)
		std::vector<std::tuple<CassUuid, bool, std::map<int,CassUuid>, std::string>> weatherlinkStations;
		DbConnectionObservations db{address, user, password};
		db.getAllWeatherlinkAPIv2Stations(weatherlinkStations);
		std::cerr << "Got the list of stations from the db" << std::endl;

		// Open the TLS stream
		asio::ssl::context ctx(asio::ssl::context::sslv23);
		ctx.set_default_verify_paths();
		std::unique_ptr<asio::ssl::stream<ip::tcp::socket>> socket{new asio::ssl::stream<ip::tcp::socket>(ioService, ctx)};
		connectSocket(ioService, *socket);

		int retry = 0;
		for (auto it = weatherlinkStations.cbegin() ; it != weatherlinkStations.cend() ;) {
			const auto& station = *it;
			if (!userSelection.empty()) {
				if (userSelection.find(std::get<0>(station)) == userSelection.cend()) {
					retry = 0;
					++it;
					continue;
				}
			}

			std::cerr << "About to download for station " << std::get<0>(station) << std::endl;
			WeatherlinkApiv2Downloader downloader{
				std::get<0>(station), std::get<3>(station), std::get<2>(station),
				weatherlinkApiV2Key, weatherlinkApiV2Secret,
				ioService, db,
				TimeOffseter::PredefinedTimezone(0)
			};
			try {
				if (!std::get<1>(station)) {
					std::cerr << "No access to archives for station " << std::get<0>(station) << std::endl;
					retry = 0;
					++it;
					continue;
				}
				downloader.download(*socket);
				retry = 0;
				++it;
			} catch (const sys::system_error& e) {
				retry++;
				if (e.code() == asio::error::in_progress) {
					std::cerr << "Lost connection to server while attempting to download, but some progress was made, keeping up the work." << std::endl;
					socket.reset(new asio::ssl::stream<ip::tcp::socket>(ioService, ctx));
					connectSocket(ioService, *socket);

					retry = 0;
				} else if (e.code() == asio::error::eof) {
					std::cerr << "Lost connection to server while attempting to download, retrying." << std::endl;
					socket.reset(new asio::ssl::stream<ip::tcp::socket>(ioService, ctx));
					connectSocket(ioService, *socket);

					if (retry >= 2) {
						std::cerr << "Tried twice already, moving on..." << std::endl;
						retry =  0;
						++it;
					}
				} else {
					throw e;
				}
			}
		}

		ioService.stop();
	} catch (std::exception& e) {
		// exit on error, and let systemd restart the daemon
		std::cerr << e.what() << std::endl;
		return 255;
	}

	ioServiceRunner.join();
}
