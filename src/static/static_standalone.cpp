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

#include "../time_offseter.h"
#include "static_message.h"
#include "config.h"

using namespace meteodata;
namespace po = boost::program_options;

int main(int argc, char** argv)
{
	std::string inputFile;

	po::options_description desc("Allowed options");
	desc.add_options()
		("help", "display the help message and exit")
		("version", "display the version of Meteodata and exit")
		("input-file", po::value<std::string>(&inputFile), "input StatIC file")
	;

	po::positional_options_description pd;
	pd.add("input-file", 1);

	po::variables_map vm;
	po::store(po::command_line_parser(argc, argv).options(desc).allow_unregistered().positional(pd).run(), vm);
	po::notify(vm);

	if (vm.count("help")) {
		std::cout << PACKAGE_STRING"\n";
		std::cout << "Usage: " << argv[0] << " file\n";
		std::cout << desc << "\n";

		return 0;
	}

	if (vm.count("version")) {
		std::cout << VERSION << std::endl;
		return 0;
	}


	try {
		auto timeOffseter = TimeOffseter::getTimeOffseterFor(TimeOffseter::PredefinedTimezone(0));
		std::ifstream fileStream(inputFile);
		StatICMessage m{fileStream, timeOffseter};
		if (!m)
			std::cerr << "Impossible to parse the message" << std::endl;
	} catch (std::exception& e) {
		std::cerr << "Meteodata-static-standalone met a critical error: " << e.what() << std::endl;
		return 255;
	}

	return 0;
}
