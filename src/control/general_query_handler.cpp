/**
 * @file general_query_handler.cpp
 * @brief Implementation of the ControlConnector class
 * @author Laurent Georget
 * @date 2022-08-05
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

#include "general_query_handler.h"
#include "../meteo_server.h"

namespace meteodata {

GeneralQueryHandler::GeneralQueryHandler(meteodata::MeteoServer& meteoServer) :
	QueryHandler{"general"},
	_meteoServer{meteoServer}
{
	_commands.push_back(NamedCommand{"shutdown", static_cast<Command>(&GeneralQueryHandler::shutdown)});
	_commands.push_back(NamedCommand{"help", static_cast<Command>(&GeneralQueryHandler::help)});
	_defaultCommand = "help";
}

std::string GeneralQueryHandler::shutdown(const std::string&)
{
	_meteoServer.stop();
	return "stopped";
}

std::string GeneralQueryHandler::help(const std::string&)
{
	return R"(The "general" queries are used to control the execution of the
meteodata server as a whole.

Available commands :
- shutdown: make the server gracefully exits
- help: displays this message)";
}

}
