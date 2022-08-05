/**
 * @file query_handler.cpp
 * @brief Implementation of the QueryHandler class
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
#include <string>
#include <algorithm>

#include "query_handler.h"
#include "../meteo_server.h"

namespace meteodata {

QueryHandler::QueryHandler(std::string cat) :
	_category{std::move(cat)}
{}

std::string QueryHandler::handleQuery(const std::string& query)
{
	std::istringstream is{query};
	std::string category;
	is >> category;

	if (category == _category) {
		std::string verb;
		is >> verb;
		if (verb.empty())
			verb = _defaultCommand;

		for (auto&& [name, cmd]: _commands) {
			if (name == verb) {
				std::string restOfQuery;
				is >> std::ws;
				std::getline(is, restOfQuery);
				return (this->*(cmd))(restOfQuery);
			}
		}
	}

	return _next ? _next->handleQuery(query) : "";
}

}
