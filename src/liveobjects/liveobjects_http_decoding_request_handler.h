/**
 * @file liveobjects_http_decoding_request_handler.h
 * @brief Definition of the LiveobjectsHttpDecodingRequestHandler class
 * @author Laurent Georget
 * @date 2023-04-10
 */
/*
 * Copyright (C) 2023 SAS Météo Concept <contact@meteo-concept.fr>
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
#ifndef LIVEOBJECTS_HTTP_DECODING_REQUEST_HANDLER_H
#define LIVEOBJECTS_HTTP_DECODING_REQUEST_HANDLER_H

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio.hpp>
#include <date/date.h>

#include <map>
#include <array>
#include <memory>
#include <tuple>
#include <string>

#include "../http_connection.h"
#include "cassandra_utils.h"

namespace meteodata
{

class LiveobjectsHttpDecodingRequestHandler
{
public:
	using Request = boost::beast::http::request<boost::beast::http::string_body>;
	using Response = boost::beast::http::response<boost::beast::http::string_body>;

	explicit LiveobjectsHttpDecodingRequestHandler(DbConnectionObservations& db);

	void processRequest(const Request& request, Response& response);

private:
	DbConnectionObservations& _db;

	std::map<std::string, CassUuid> _stations;

	bool checkAccess(const std::string& urn, Response& response);

	void decodeMessage(const Request& request, Response& response);

	using Route = void (LiveobjectsHttpDecodingRequestHandler::*)(const Request& request, Response& response);
	const std::array<std::tuple<boost::beast::http::verb, std::string, LiveobjectsHttpDecodingRequestHandler::Route>, 1> routes = {
		std::make_tuple(
			boost::beast::http::verb::post,
			std::string{"/imports/decode/liveobjects"},
			&LiveobjectsHttpDecodingRequestHandler::decodeMessage
		)
	};
};

}


#endif // LIVEOBJECTS_HTTP_DECODING_REQUEST_HANDLER_H
