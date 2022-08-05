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
#include <algorithm>

#include "connectors_query_handler.h"
#include "../meteo_server.h"

namespace meteodata {

ConnectorsQueryHandler::ConnectorsQueryHandler(meteodata::MeteoServer& meteoServer) :
	QueryHandler{"connectors"},
	_meteoServer{meteoServer}
{
	_commands.push_back(NamedCommand{ "list", static_cast<Command>(&ConnectorsQueryHandler::list) });
	_commands.push_back(NamedCommand{ "status", static_cast<Command>(&ConnectorsQueryHandler::status) });
	_commands.push_back(NamedCommand{ "help", static_cast<Command>(&ConnectorsQueryHandler::help) });
	_defaultCommand = "list";
}

std::string ConnectorsQueryHandler::list(const std::string&)
{
	std::ostringstream os;
	for (auto it = _meteoServer.beginConnectors() ; it != _meteoServer.endConnectors() ; ++it) {
		os << std::get<0>(*it) << "\n";
	}
	return os.str();
}

std::string ConnectorsQueryHandler::status(const std::string& name)
{
	auto it = std::find_if(_meteoServer.beginConnectors(), _meteoServer.endConnectors(),
						   [&name](const auto& connector) { return std::get<0>(connector) == name; });
	if (it != _meteoServer.endConnectors())
		return std::get<1>(*it)->getStatus();
	else
		return R"(Unknown or unavailable connector ")" + name + R"(")";
}

std::string ConnectorsQueryHandler::help(const std::string&)
{
	return R"(The "connectors" queries are used to get information and act
on the various components of Meteodata in charge of retrieving weather data.
There is one connector for each "way" of getting the data, be it an API,
a proprietary protocol, etc.

Available options :
- list: list the active connectors
- status <connector>: gives the latest status of the connector identified by its name
- help: displays this message)";
}

}
