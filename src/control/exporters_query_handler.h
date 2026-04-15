/**
 * @file exporters_query_handler.h
 * @brief Definition of the ExportersQueryHandler class
 * @author Laurent Georget
 * @date 2026-04-15
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


#ifndef EXPORTERS_QUERY_HANDLER_H
#define EXPORTERS_QUERY_HANDLER_H

#include <string>
#include <array>
#include <tuple>

#include "query_handler.h"

namespace meteodata
{

class MeteoServer;
class Exporter;

class ExportersQueryHandler : public QueryHandler
{
public:
	explicit ExportersQueryHandler(MeteoServer& meteoServer);
	std::string list(const std::string&);
	std::string help(const std::string&);
	std::string start(const std::string&);
	std::string stop(const std::string&);
	std::string reload(const std::string&);

private:
	MeteoServer& _meteoServer;

	using Action = void (Exporter::*)();
	using Query = std::string (Exporter::*)();
	template<typename T>
	std::string callOnExporter(const std::string& name, T action);
};

}


#endif // EXPORTERS_QUERY_HANDLER_H
