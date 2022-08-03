/**
 * @file connectors_query_handler.h
 * @brief Definition of the ConnectorsQueryHandler class
 * @author Laurent Georget
 * @date 2022-08-02
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


#ifndef CONNECTORS_QUERY_HANDLER_H
#define CONNECTORS_QUERY_HANDLER_H

#include <string>
#include <array>
#include <tuple>

#include "query_handler.h"

namespace meteodata
{

class MeteoServer;

class ConnectorsQueryHandler : public QueryHandler
{
public:
	ConnectorsQueryHandler(MeteoServer& meteoServer);
	std::string list(const std::string&);
	std::string status(const std::string& name);
	std::string help(const std::string&);
	std::string handleQuery(const std::string& query) override;

private:
	MeteoServer& _meteoServer;

	using Command = std::string (ConnectorsQueryHandler::*)(const std::string& query);
	struct NamedCommand {
		const char* verb;
		Command command;
	};
	constexpr static std::array<NamedCommand, 3> _commands = {
			NamedCommand{ "list", &ConnectorsQueryHandler::list },
			NamedCommand{ "status", &ConnectorsQueryHandler::status },
			NamedCommand{ "help", &ConnectorsQueryHandler::help },
	};

	constexpr static char DEFAULT_COMMAND[] = "list";
};

}


#endif // CONNECTORS_QUERY_HANDLER_H
