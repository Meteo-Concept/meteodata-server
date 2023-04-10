/**
 * @file liveobjects_http_decoding_request_handler.cpp
 * @brief Implementation of the LiveobjectsHttpDecodingRequestHandler class
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

#include <vector>
#include <tuple>
#include <string>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/json/src.hpp>
#include <boost/beast/http.hpp>
#include <systemd/sd-daemon.h>
#include <date.h>

#include "../http_connection.h"
#include "../cassandra.h"
#include "../cassandra_utils.h"
#include "../time_offseter.h"
#include "liveobjects/liveobjects_message.h"
#include "liveobjects/liveobjects_http_decoding_request_handler.h"

namespace meteodata
{
namespace pt = boost::property_tree;
namespace json = boost::json;

LiveobjectsHttpDecodingRequestHandler::LiveobjectsHttpDecodingRequestHandler(DbConnectionObservations& db) :
	_db{db}
{
	std::vector<std::tuple<CassUuid, std::string, std::string>> liveobjectsStations;
	_db.getAllLiveobjectsStations(liveobjectsStations);
	for (auto&& l : liveobjectsStations) {
		_stations[std::get<1>(l)] = std::get<0>(l);
	}
}

void LiveobjectsHttpDecodingRequestHandler::processRequest(const Request& request, Response& response)
{
	bool targetFound = false;

	for (auto&&[verb, url, handler] : routes) {
		if (request.target() == url) {
			targetFound = true;
			if (verb == request.method()) {
				(this->*handler)(request, response);
				response.set(boost::beast::http::field::content_type, "application/json");
				return;
			}
		}
	}

	response.result(targetFound ? boost::beast::http::status::method_not_allowed : boost::beast::http::status::not_found);
}


bool LiveobjectsHttpDecodingRequestHandler::checkAccess(const std::string& urn, Response& response)
{
	if (_stations.count(urn) == 0) {
		response.result(boost::beast::http::status::forbidden);
		return false;
	}
	return true;
}

void LiveobjectsHttpDecodingRequestHandler::decodeMessage(const Request& request, Response& response)
{
	const std::string& content = request.body();
	std::istringstream stream(content);
	pt::ptree jsonTree;
	std::string urn;

	try {
		pt::json_parser::read_json(stream, jsonTree);
		urn = jsonTree.get<std::string>("streamId");
	} catch(pt::json_parser::json_parser_error e) {
		response.result(boost::beast::http::status::bad_request);
		return;
	}

	if (checkAccess(urn, response)) {
		date::sys_seconds timestamp;
		auto m = LiveobjectsMessage::parseMessage(_db, jsonTree, _stations[urn], timestamp);

		if (m && m->looksValid()) {
			json::object body = m->getDecodedMessage();
			response.body() = json::serialize(body);
			response.result(boost::beast::http::status::ok);
		} else {
			response.result(boost::beast::http::status::bad_request);
		}
	}
}

}
