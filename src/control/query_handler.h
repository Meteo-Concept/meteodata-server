/**
 * @file query_handler.h
 * @brief Definition of the QueryHandler class
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


#ifndef QUERY_HANDLER_H
#define QUERY_HANDLER_H

#include <memory>

namespace meteodata
{

class QueryHandler
{
public:
	virtual ~QueryHandler() = default;
	virtual void setNext(std::unique_ptr<QueryHandler>&& next) {
		_next = std::move(next);
	};
	virtual std::string handleQuery(const std::string& query) = 0;

protected:
	std::unique_ptr<QueryHandler> _next = nullptr;
};

}

#endif // QUERY_HANDLER_H
