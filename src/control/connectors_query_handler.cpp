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
#include <type_traits>

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
	_commands.push_back(NamedCommand{ "start", static_cast<Command>(&ConnectorsQueryHandler::start) });
	_commands.push_back(NamedCommand{ "stop", static_cast<Command>(&ConnectorsQueryHandler::stop) });
	_commands.push_back(NamedCommand{ "reload", static_cast<Command>(&ConnectorsQueryHandler::reload) });
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



std::string ConnectorsQueryHandler::help(const std::string&)
{
	return R"(The "connectors" queries are used to get information and act
on the various components of Meteodata in charge of retrieving weather data.
There is one connector for each "way" of getting the data, be it an API,
a proprietary protocol, etc.

Available options :
- list: list the active connectors
- status <connector>: gives the latest status of the connector identified by its name
- start <connector>: starts a connector previously stopped
- stop <connector>: stop an active connector
- reload <connector>: make a connector reload its configuration and list of stations
- help: displays this message)";
}

template <typename T>
std::string ConnectorsQueryHandler::callOnConnector(const std::string& name, T action)
{
	auto it = std::find_if(_meteoServer.beginConnectors(), _meteoServer.endConnectors(),
						   [&name](const auto& connector) { return std::get<0>(connector) == name; });
	if (it != _meteoServer.endConnectors()) {
		Connector* connector = std::get<1>(*it).get();
		if constexpr (std::is_base_of<std::string, std::invoke_result_t<T, Connector*>>()) {
			return (connector->*action)();
		} else {
			(connector->*action)();
			return "OK";
		}
	} else {
		return R"(Unknown or unavailable connector ")" + name + R"(")";
	}
}

std::string ConnectorsQueryHandler::start(const std::string& name)
{
	return callOnConnector(name, &Connector::start);
}

std::string ConnectorsQueryHandler::stop(const std::string& name)
{
	return callOnConnector(name, &Connector::stop);
}

std::string ConnectorsQueryHandler::reload(const std::string& name)
{
	return callOnConnector(name, &Connector::reload);
}

std::string ConnectorsQueryHandler::status(const std::string& name)
{
	return callOnConnector(name, &Connector::getStatus);
}

}
