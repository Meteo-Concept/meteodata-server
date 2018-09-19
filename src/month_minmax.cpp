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

inline constexpr bool operator<(CassUuid u1, CassUuid u2)
{
	if (u1.time_and_version == u2.time_and_version)
		return u1.clock_seq_and_node < u2.clock_seq_and_node;
	else
		return u1.time_and_version < u2.time_and_version;
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
	std::string begin;
	std::string end;
	std::vector<std::string> namedStations;

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
		("begin", po::value<std::string>(&begin), "the beginning of the date range for which the min/max must be computed (defaults to the current month)")
		("end", po::value<std::string>(&end), "the end of the date range for which the min/max must be computed (defaults to 'begin')")
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

	year_month beginDate, endDate;
	year_month_day today{date::floor<date::days>(system_clock::now())};
	year_month currentMonth{today.year(), today.month()};
	if (vm.count("begin")) {
		std::istringstream in{begin};
		in >> date::parse("%Y-%m", beginDate);
		if (!in) {
			std::cerr << "'" << begin << "' does not look like a valid, that's problematic" << std::endl;
		}
		if (beginDate > currentMonth) {
			std::cerr << beginDate << " looks like it's in the future, that's problematic" << std::endl;
			return EINVAL;
		}
	} else {
		beginDate = currentMonth;
	}

	if (vm.count("end")) {
		std::istringstream in{end};
		in >> date::parse("%Y-%m", endDate);
		if (!in) {
			std::cerr << "'" << end << "' does not look like a valid, that's problematic" << std::endl;
		}
		if (endDate < beginDate) {
			std::cerr << endDate << " looks like it's before " << beginDate << ", that's problematic" << std::endl;
			return EINVAL;
		}
		if (endDate > currentMonth) {
			std::cerr << endDate << " looks like it's in the future, that's problematic" << std::endl;
			return EINVAL;
		}
	} else {
		endDate = beginDate;
	}

	std::vector<CassUuid> userSelection;
	if (vm.count("station")) {
		for (const auto& st : namedStations) {
			CassUuid uuid;
			CassError res = cass_uuid_from_string(st.c_str(), &uuid);
			if (res != CASS_OK) {
				std::cerr << "'" << st << "' does not look like a valid UUID, ignoring" << std::endl;
				continue;
			}
			userSelection.push_back(uuid);
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


		std::vector<CassUuid> allStations;
		std::cerr << "Fetching the list of stations" <<std::endl;
		db.getAllStations(allStations);
		std::cerr << allStations.size() << " stations identified\n" <<std::endl;

		std::vector<CassUuid> stations;
		if (userSelection.empty()) {
			stations = std::move(allStations);
		} else {
			std::sort(allStations.begin(), allStations.end());
			std::sort(userSelection.begin(), userSelection.end());
			std::vector<CassUuid> unknown;
			std::set_difference(userSelection.cbegin(), userSelection.cend(), allStations.cbegin(), allStations.cend(), std::back_inserter(unknown));
			if (!unknown.empty()) {
				std::cerr << "The following UUIDs are unknown and will be ignored:\n";
				for (const auto& st : unknown) {
 					char asStr[CASS_UUID_STRING_LENGTH];
					cass_uuid_string(st, asStr);
					std::cerr << "\t" << asStr << "\n";
				}
				std::cerr << std::endl;
			}
			std::set_intersection(allStations.cbegin(), allStations.cend(), userSelection.cbegin(), userSelection.cend(), std::back_inserter(stations));
		}

		const auto today = system_clock::now();
		year_month selectedDate = beginDate;
		while (selectedDate <= endDate) {
			for (const CassUuid& station : stations) {
				std::cerr << "Selected date: " << selectedDate << std::endl;
				int y = int(selectedDate.year());
				int m = static_cast<int>(unsigned(selectedDate.month()));
				std::cerr << "Getting daily values (all except wind)" << std::endl;
				db.getDailyValues(station, y, m, values);

				auto day = sys_days{year_month_day{year{y}/m/1}};
				auto end = sys_days{year_month_day{year{y}/m/last}};

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
			selectedDate += date::months{1};
		}
		std::cerr << "Done" << std::endl;
	} catch (std::exception& e) {
		std::cerr << "Meteodata-month-minmax met a critical error: " << e.what() << std::endl;
		return 255;
	}
}
