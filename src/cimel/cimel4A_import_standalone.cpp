/**
 * @file cimel4A_import_standalone.cpp
 * @brief Implementation of the importer program for CIMEL stations
 * @author Laurent Georget
 * @date 2021-12-21
 */
/*
 * Copyright (C) 2021  SAS Météo Concept <contact@meteo-concept.fr>
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
#include <sstream>
#include <fstream>
#include <limits>

#include <boost/program_options.hpp>

#include "../time_offseter.h"
#include "cimel4A_importer.h"
#include "config.h"

#define DEFAULT_CONFIG_FILE "/etc/meteodata/db_credentials"

using namespace meteodata;
namespace po = boost::program_options;

template<typename Importer>
bool doImport(Importer& importer, const std::string& inputFile, date::sys_seconds& start, date::sys_seconds& end,
			  bool updateLastArchiveDownloadTime)
{
	std::ifstream input{inputFile};
	return importer.import(input, start, end, updateLastArchiveDownloadTime);
}


int main(int argc, char** argv)
{
	std::string user;
	std::string password;
	std::string address;
	std::string fileName;
	std::string inputFile;
	std::string uuid;
	std::string tz;
	std::string format;
	std::string cimelId;
	int year;

	po::options_description desc("Allowed options");
	desc.add_options()
		("help", "display the help message and exit")
		("version", "display the version of Meteodata and exit")
		("config-file", po::value<std::string>(&fileName), "alternative configuration file")
		("input-file", po::value<std::string>(&inputFile), "input data file")
		("station", po::value<std::string>(&uuid), "station UUID")
		("cimel", po::value<std::string>(&cimelId), "the CIMEL station ID (INSEE code followed by the station number)")
		("timezone", po::value<std::string>(&tz), R"(timezone identifier (like "UTC" or "Europe/Paris"))")
		("update-last-download-time,t", "update the last archive download time of the station to the most recent datetime in the imported data")
		("year", po::value<int>(&year), "the year the observations were collected (defaults to the current year, it cannot be deduced from the content)")
	;

	po::positional_options_description pd;
	pd.add("input-file", 1);
	pd.add("station", 1);
	pd.add("timezone", 1);

	po::options_description config("Configuration");
	config.add_options()
		("user,u", po::value<std::string>(&user), "database username")
		("password,p", po::value<std::string>(&password), "database password")
		("host,h", po::value<std::string>(&address), "database IP address or domain name")
	;
	desc.add(config);

	po::variables_map vm;
	po::store(po::command_line_parser(argc, argv).options(desc).positional(pd).run(), vm);
	std::ifstream configFile(vm.count("config-file") ? fileName : DEFAULT_CONFIG_FILE);
	if (configFile) {
		po::store(po::parse_config_file(configFile, config, true), vm);
		configFile.close();
	}
	po::notify(vm);

	if (vm.count("help") || vm.count("user") != vm.count("password") || !vm.count("input-file")) {
		std::cout << PACKAGE_STRING"\n";
		std::cout << "Usage: " << argv[0] << " file station timezone [-u user -p password]\n";
		std::cout << desc << "\n";
		std::cout << "You must give either both the username and "
					 "password or none of them." << std::endl;

		return 0;
	}

	if (vm.count("version")) {
		std::cout << VERSION << std::endl;
		return 0;
	}

	if (vm.count("input-file") != 1 || vm.count("station") != 1 || vm.count("timezone") != 1) {
		std::cout << "You must give the input file, the station and the timezone." << std::endl;
		return 1;
	}

	bool updateLastArchiveDownloadTime = vm.count("update-last-download-time");

	date::year y = date::year_month_day{date::floor<date::days>(std::chrono::system_clock::now())}.year();
	if (vm.count("year")) {
		if (date::year{year} > y || year < 1900) {
			std::cout << "The year " << year
					  << " looks awfully suspicious, proceed anyway with Enter, or abort the program." << std::endl;

			std::cin.ignore(std::numeric_limits<std::streamsize>::max());
			std::string dummy;
			std::cin >> dummy;
		}
		y = date::year{year};
	}

	try {
		DbConnectionObservations db(address, user, password);
		CassUuid station;
		cass_uuid_from_string(uuid.c_str(), &station);
		std::ifstream fileStream(inputFile);
		date::sys_seconds start, end;

		Cimel4AImporter cimel4AImporter(station, cimelId, tz, db);
		bool importResult = cimel4AImporter.import(fileStream, start, end, y, updateLastArchiveDownloadTime);

		if (importResult) {
			std::cout << "Consider recomputing the climatology: \n"
				  << "\tmeteodata-minmax --station " << uuid
					<< " --begin " << date::format("%Y-%m-%d", start)
					<< " --end " << date::format("%Y-%m-%d", end)
					<< "\n"
				  << "\tmeteodata-month-minmax --station " << uuid
					<< " --begin " << date::format("%Y-%m", start)
					<< " --end " << date::format("%Y-%m", end)
					<< "\n"
				 << std::endl;
		} else {
			std::cout << "Failed to parse any entry" << std::endl;
			return 2;
		}
	} catch (std::exception& e) {
		std::cerr << "Meteodata-cimel-standalone met a critical error: " << e.what() << std::endl;
		return 255;
	}

	return 0;
}
