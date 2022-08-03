/**
 * @file connectors_query_handler.cpp
 * @brief Implementation of the ControlConnector class
 * @author Laurent Georget
 * @date 2022-08-01
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

#include <sstream>

#include "connectors_query_handler.h"

namespace meteodata {

ConnectorsQueryHandler::ConnectorsQueryHandler(meteodata::MeteoServer& meteoServer) :
	_meteoServer{meteoServer}
{

}

std::string ConnectorsQueryHandler::list(const std::string& name)
{
	return std::string();
}

std::string ConnectorsQueryHandler::status(const std::string& name)
{
	return std::string();
}

std::string ConnectorsQueryHandler::help(const std::string& name)
{
	return std::string();
}

std::string ConnectorsQueryHandler::handleQuery(const std::string& query)
{
	std::istringstream is{query};
	std::string category;
	is >> category;

	if (category == "connectors") {
		std::string verb;
		is >> verb;
		if (verb.empty())
			verb = DEFAULT_COMMAND;

		bool found = false;
		for (auto it = _commands.cbegin() ; it != _commands.end() && !found ; ++it) {
			if (it->verb == verb) {
				found = true;
				std::string restOfQuery;
				std::getline(is, restOfQuery);
				(this->*(it->command))(restOfQuery);
			}
		}
	}

	return _next->handleQuery(query);
}

}
