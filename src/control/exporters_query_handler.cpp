/**
 * @file exporters_query_handler.cpp
 * @brief Implementation of the ExportersQueryHandler class
 * @author Laurent Georget
 * @date 2026-04-14
 */
/*
 * Copyright (C) 2026  SAS Météo Concept <contact@meteo-concept.fr>
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

#include "control/exporters_query_handler.h"
#include "export/exporter.h"
#include "meteo_server.h"

namespace meteodata {

ExportersQueryHandler::ExportersQueryHandler(meteodata::MeteoServer& meteoServer) :
	QueryHandler{"exporters"},
	_meteoServer{meteoServer}
{
	_commands.push_back(NamedCommand{ "list", static_cast<Command>(&ExportersQueryHandler::list) });
	_commands.push_back(NamedCommand{ "help", static_cast<Command>(&ExportersQueryHandler::help) });
	_commands.push_back(NamedCommand{ "start", static_cast<Command>(&ExportersQueryHandler::start) });
	_commands.push_back(NamedCommand{ "stop", static_cast<Command>(&ExportersQueryHandler::stop) });
	_commands.push_back(NamedCommand{ "reload", static_cast<Command>(&ExportersQueryHandler::reload) });
	_defaultCommand = "list";
}

std::string ExportersQueryHandler::list(const std::string&)
{
	std::ostringstream os;
	for (auto it = _meteoServer.beginExporters() ; it != _meteoServer.endExporters() ; ++it) {
		os << std::get<0>(*it) << "\n";
	}
	return os.str();
}



std::string ExportersQueryHandler::help(const std::string&)
{
	return R"(The "exporters" queries are used to get information and act
on the various components of Meteodata in charge of exporting or forwarding
weather data.
There is one exporter for each destination where data can be pushed (an API,
a FTP server, a proprietary protocol, etc.)

Available options :
- list: list the active exporters
- start <exporter>: starts an exporter previously stopped
- stop <exporter>: stop an exporter
- reload <exporter>: make a connector reload its configuration and list of stations
- help: displays this message)";
}

template <typename T>
std::string ExportersQueryHandler::callOnExporter(const std::string& name, T action)
{
	auto it = std::find_if(_meteoServer.beginExporters(), _meteoServer.endExporters(),
		[&name](const auto& connector) { return std::get<0>(connector) == name; });
	if (it != _meteoServer.endExporters()) {
		Exporter* exporter = std::get<1>(*it).get();
		if constexpr (std::is_base_of<std::string, std::invoke_result_t<T, Exporter*>>()) {
			return (exporter->*action)();
		} else {
			(exporter->*action)();
			return "OK";
		}
	} else {
		return R"(Unknown or unavailable exporter ")" + name + R"(")";
	}
}

std::string ExportersQueryHandler::start(const std::string& name)
{
	return callOnExporter(name, &Exporter::start);
}

std::string ExportersQueryHandler::stop(const std::string& name)
{
	return callOnExporter(name, &Exporter::stop);
}

std::string ExportersQueryHandler::reload(const std::string& name)
{
	return callOnExporter(name, &Exporter::reload);
}

}
