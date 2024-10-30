/**
 * @file synopdownloader.cpp
 * @brief Implementation of the SynopStandalone class
 * @author Laurent Georget
 * @date 2018-08-20
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
#include <iterator>
#include <map>
#include <sstream>
#include <fstream>

#include <boost/program_options.hpp>
#include <cassandra.h>
#include <dbconnection_observations.h>

#include "synop_standalone.h"
#include "ogimet_synop.h"
#include "synop_decoder/parser.h"
#include "config.h"

#define DEFAULT_CONFIG_FILE "/etc/meteodata/db_credentials"

using namespace meteodata;
namespace po = boost::program_options;

namespace meteodata
{

SynopStandalone::SynopStandalone(DbConnectionObservations& db) :
		_db(db)
{
}

void SynopStandalone::start(const std::string& file)
{
	std::vector<std::tuple<CassUuid, std::string>> icaos;
	_db.getAllIcaos(icaos);
	for (auto&& icao : icaos)
		_icaos.emplace(std::get<1>(icao), std::get<0>(icao));

	std::cerr << "List of stations: ";
	for (auto&& s : _icaos) {
		char uuidStr[CASS_UUID_STRING_LENGTH];
		cass_uuid_string(s.second, uuidStr);
		std::cerr << s.first << ": " << uuidStr << "\n";
	}

	std::cerr << "Now parsing SYNOP messages " << std::endl;

	std::size_t lineCount = 0;

	std::ifstream input{file};
	while (input) {
		std::string line;
		std::getline(input, line);
		lineCount++;

		// Deal with the annoying case as early as possible
		if (line.empty() || line.find("NIL") != std::string::npos)
			continue;

		std::istringstream lineIterator{line};

		Parser parser;
		if (parser.parse(lineIterator)) {
			const SynopMessage& m = parser.getDecodedMessage();
			auto uuidIt = _icaos.find(m._stationIcao);
			if (uuidIt != _icaos.end()) {
				const CassUuid& station = uuidIt->second;
				char uuidStr[CASS_UUID_STRING_LENGTH];
				cass_uuid_string(station, uuidStr);
				std::cerr << "Line " << lineCount << ", UUID identified for ICAO " << m._stationIcao << ": " << uuidStr << std::endl;

				std::string stationName;
				int pollingPeriod;
				time_t lastArchiveDownloadTime;
				_db.getStationDetails(station, stationName, pollingPeriod, lastArchiveDownloadTime);
				float latitude, longitude;
				int elevation;
				_db.getStationLocation(station, latitude, longitude, elevation);
				TimeOffseter timeOffseter = TimeOffseter::getTimeOffseterFor(TimeOffseter::PredefinedTimezone::UTC);
				timeOffseter.setLatitude(latitude);
				timeOffseter.setLongitude(longitude);
				timeOffseter.setElevation(elevation);
				timeOffseter.setMeasureStep(pollingPeriod);

				OgimetSynop synop{m, &timeOffseter};
				_db.insertV2DataPoint(synop.getObservations(uuidIt->second));
				std::pair<bool, float> rainfall24 = std::make_pair(false, 0.f);
				std::pair<bool, int> insolationTime24 = std::make_pair(false, 0);
				auto it = std::find_if(m._precipitation.begin(), m._precipitation.end(),
									   [](const auto& p) { return p._duration == 24; });
				if (it != m._precipitation.end())
					rainfall24 = std::make_pair(true, it->_amount);
				if (m._minutesOfSunshineLastDay)
					insolationTime24 = std::make_pair(true, *(m._minutesOfSunshineLastDay));
				auto day = date::floor<date::days>(m._observationTime) - date::days(1);
				_db.insertV2EntireDayValues(station, date::sys_seconds(day).time_since_epoch().count(), rainfall24,
											insolationTime24);
				if (m._minTemperature)
					_db.insertV2Tn(station, chrono::system_clock::to_time_t(m._observationTime),
								   *m._minTemperature / 10.f);
				if (m._maxTemperature)
					_db.insertV2Tx(station, chrono::system_clock::to_time_t(m._observationTime),
								   *m._maxTemperature / 10.f);
			}
		} else {
			std::cerr << "Record looks invalid, discarding..." << std::endl;
		}
	}
}

}

int main(int argc, char** argv)
{
	std::string user;
	std::string password;
	std::string address;
	std::string pguser;
	std::string pgpassword;
	std::string pgaddress;
	std::string fileName;
	std::string inputFile;

	po::options_description desc("Allowed options");
	desc.add_options()
		("help", "display the help message and exit")
		("version", "display the version of Meteodata and exit")
		("config-file", po::value<std::string>(&fileName), "alternative configuration file")
		;

	po::positional_options_description pd;
	pd.add("input-file", 1);

	po::options_description config("Configuration");
	config.add_options()
		("user,u", po::value<std::string>(&user), "database username")
		("password,p", po::value<std::string>(&password), "database password")
		("host,h", po::value<std::string>(&address), "database IP address or domain name")
		("pguser", po::value<std::string>(&pguser), "PostgreSQL database username")
		("pgpassword", po::value<std::string>(&pgpassword), "PostgreSQL database password")
		("pghost", po::value<std::string>(&pgaddress), "PostgreSQL database IP address or domain name")
		("weatherlink-apiv2-key,k", po::value<std::string>(), "Ignored")
		("weatherlink-apiv2-secret,s", po::value<std::string>(), "Ignored")
		("input-file", po::value<std::string>(&inputFile), "input CSV file containing the SYNOP messages (in the OGIMET getsynop format)")
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
		std::cout << "Usage: " << argv[0] << " file [-u user -p password]\n";
		std::cout << desc << "\n";
		std::cout << "You must give either both the username and "
					 "password or none of them." << std::endl;

		return 0;
	}

	if (vm.count("version")) {
		std::cout << VERSION << std::endl;
		return 0;
	}


	try {
		DbConnectionObservations db(address, user, password, pgaddress, pguser, pgpassword);

		cass_log_set_level(CASS_LOG_INFO);
		CassLogCallback logCallback = [](const CassLogMessage* message, void*) -> void {
			std::cerr << message->message << " (from " << message->function << ", in " << message->function << ", line "
					  << message->line << ")" << std::endl;
		};
		cass_log_set_callback(logCallback, nullptr);

		SynopStandalone synoper(db);
		synoper.start(inputFile);
	} catch (std::exception& e) {
		std::cerr << "Meteodata-synop-standalone met a critical error: " << e.what() << std::endl;
		return 255;
	}

	return 0;
}
