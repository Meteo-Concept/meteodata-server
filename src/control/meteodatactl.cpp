/**
 * @file meteodatactl.cpp
 * @brief Control program for meteodata
 * @author Laurent Georget
 * @date 2022-08-08
 */
/*
 * Copyright (C) 2022  SAS Météo Concept <contact@meteo-concept.fr>
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
#include <fstream>
#include <thread>

#include <boost/program_options.hpp>
#include <boost/asio.hpp>

#include "../time_offseter.h"
#include "config.h"

using namespace meteodata;
namespace po = boost::program_options;

int main(int argc, char** argv)
{
	std::vector<std::string> tokens;

	po::options_description desc("General options");
	desc.add_options()
		("help", "display the help message and exit")
		("version", "display the version of Meteodata and exit")
	;

	po::options_description hidden("Hidden options");
	hidden.add_options()
		("command", po::value<std::vector<std::string>>(&tokens)->multitoken(), "send the specified command over to Meteodata")
	;

	po::positional_options_description p;
	p.add("command", -1);

	po::options_description all("All options");
	all.add(desc).add(hidden);

	po::variables_map vm;
	po::store(po::command_line_parser{argc, argv}.options(all).positional(p).run(), vm);
	po::notify(vm);

	if (vm.count("help")) {
		std::cout << "meteodatactl\n";
		std::cout << "Usage: " << argv[0] << " TODO \n";
		std::cout << desc << std::endl;

		return 0;
	}

	if (vm.count("version")) {
		std::cout << VERSION << std::endl;
		return 0;
	}

	try {
		boost::asio::io_context ioContext;
		boost::asio::local::stream_protocol::socket socket{ioContext};
		socket.connect(boost::asio::local::stream_protocol::endpoint{CONTROL_SOCKET_PATH});

		std::ostringstream os;
		std::copy(tokens.cbegin(), tokens.cend(), std::ostream_iterator<std::string>(os, " "));
		std::string query = os.str();
		if (!query.empty())
			query[query.length() - 1] = '\n';
		else
			query = "\n";
		std::cout << "> " << query;
		boost::asio::write(socket, boost::asio::buffer(query));

		std::string reply;
		size_t replyLength = 0;
		do {
			replyLength = boost::asio::read_until(socket, boost::asio::dynamic_buffer(reply), '\n');
			std::cout << reply;
			replyLength = 0;
		} while (replyLength > 0);

		socket.close();
	} catch (std::exception& e) {
		std::cerr << e.what() << std::endl;
		return 255;
	}
}