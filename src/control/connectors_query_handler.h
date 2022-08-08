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
class Connector;

class ConnectorsQueryHandler : public QueryHandler
{
public:
	explicit ConnectorsQueryHandler(MeteoServer& meteoServer);
	std::string list(const std::string&);
	std::string status(const std::string& name);
	std::string help(const std::string&);
	std::string start(const std::string&);
	std::string stop(const std::string&);
	std::string reload(const std::string&);

private:
	MeteoServer& _meteoServer;

	using Action = void (Connector::*)();
	using Query = std::string (Connector::*)();
	template<typename T>
	std::string callOnConnector(const std::string& name, T action);
};

}


#endif // CONNECTORS_QUERY_HANDLER_H
