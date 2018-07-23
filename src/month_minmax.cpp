/**
 * @file month_minmax.cpp
 * @brief Entry point of the minmax program
 * @author Laurent Georget
 * @date 2018-07-10
 */
/*
 * Copyright (C) 2018  SAS Météo Concept <contact@meteo-concept.fr>
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
#include <string>
#include <tuple>
#include <vector>

#include <fstream>
#include <unistd.h>
#include <syslog.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <syslog.h>
#include <unistd.h>

#include <boost/program_options.hpp>
#include <cassandra.h>

#include "date/date.h"

#include "dbconnection_month_minmax.h"
#include "config.h"


/**
 * @brief The configuration file default path
 */
#define DEFAULT_CONFIG_FILE "/etc/meteodata/db_credentials"

using namespace meteodata;
using namespace date;
using namespace std::chrono;
namespace po = boost::program_options;

template<typename T1, typename T2>
inline std::ostream& operator<<(std::ostream& os, const std::pair<T1,T2>& pair)
{
	os << "(" << pair.first << ", " << pair.second << ")";
	return os;
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
	date::sys_days selectedDate{date::floor<date::days>(system_clock::now())};
	int y, m;

	po::options_description config("Configuration");
	config.add_options()
		("user,u", po::value<std::string>(&user), "database username")
		("password,p", po::value<std::string>(&password), "database password")
		("host,h", po::value<std::string>(&address), "database IP address or domain name")
	;

	po::options_description desc("Allowed options");
	desc.add_options()
		("help", "display the help message and exit")
		("version", "display the version of Meteodata and exit")
		("config-file", po::value<std::string>(), "alternative configuration file")
		("year,y", po::value<int>(&y), "the date for which the min/max must be computed (defaults to today)")
		("month,m", po::value<int>(&m), "the date for which the min/max must be computed (defaults to today)")
	;
	desc.add(config);

	po::variables_map vm;
	po::store(po::parse_command_line(argc, argv, desc), vm);
	std::string configFileName = vm.count("config-file") ?
		vm["config-file"].as<std::string>() :
		DEFAULT_CONFIG_FILE;
	std::ifstream configFile(configFileName);
	if (configFile) {
		po::store(po::parse_config_file(configFile, config), vm);
		configFile.close();
	}
	po::notify(vm);

	if (vm.count("help")) {
		std::cout << PACKAGE_STRING"\n";
		std::cout << "Usage: " << argv[0] << " [-u user -p password]\n";
		std::cout << desc << "\n";
		std::cout << "You must give either both the username and "
			"password or none of them." << std::endl;

		return 0;
	}

	if (vm.count("version")) {
		std::cout << VERSION << std::endl;
		return 0;
	}

	if (vm.count("year") && vm.count("month")) {
		selectedDate = year_month_day{year{y}/m/1};
		if (selectedDate > system_clock::now()) {
			std::cerr << selectedDate << " looks like it's in the future, that's problematic" << std::endl;
			return EINVAL;
		}
	}


	try {
		DbConnectionMonthMinmax db(address, user, password);
		DbConnectionMonthMinmax::Values values;

		cass_log_set_level(CASS_LOG_INFO);
		CassLogCallback logCallback =
			[](const CassLogMessage *message, void*) -> void {
				std::cerr << message->message << " (from " << message->function << ", in " << message->function << ", line " << message->line << ")" << std::endl;
			};
		cass_log_set_callback(logCallback, NULL);

		std::cerr << "Selected date: " << selectedDate << std::endl;

		std::vector<CassUuid> stations;
		std::cerr << "Fetching the list of stations" <<std::endl;
		db.getAllStations(stations);
		std::cerr << stations.size() << " stations identified\n" <<std::endl;

		for (const CassUuid& station : stations) {
			std::cerr << "Getting daily values (all except wind)" << std::endl;
			db.getDailyValues(station, y, m, values);

			auto day = sys_days{year_month_day{year{y}/m/1}};
			auto end = sys_days{year_month_day{year{y}/m/last}};
			auto today = system_clock::now();

			std::vector<std::pair<int,float>> winds;
			while (day <= end && day <= today) {
				std::cerr << "Getting wind values for day " << day << std::endl;
				db.getWindValues(station, day, winds);
				day += days{1};
			}

			int count = 0;
			int dirs[16] = {0};
			for (auto&& w : winds) {
				if (w.second / 3.6 >= 2.0) {
					int rounded = ((w.first % 360) * 100 + 1125) / 2250;
					dirs[rounded]++;
					count++;
				}
			}
			values.winddir.second.resize(16);
			for (int i=0 ; i<16 ; i++) {
				int v = dirs[i];
				std::cerr << "v = " << v << " | count = " << count << std::endl;
				values.winddir.second[i] = count == 0 ? 0 : v * 1000 / count;
			}
			values.winddir.first = true;

			std::cerr << "Inserting into database" << std::endl;
			db.insertDataPoint(station, y, m, values);
			std::cerr << "-----------------------" << std::endl;
		}
		std::cerr << "Done" << std::endl;
	} catch (std::exception& e) {
		std::cerr << "Meteodata-month-minmax met a critical error: " << e.what() << std::endl;
		return 255;
	}
}
