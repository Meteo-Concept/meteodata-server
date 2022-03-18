/**
 * @file cimel_http_connection.h
 * @brief Definition of the CimelHttpConnection class
 * @author Laurent Georget
 * @date 2022-01-10
 */
/*
 * Copyright (C) 2022 SAS Météo Concept <contact@meteo-concept.fr>
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
#ifndef CIMEL_HTTP_CONNECTION_H
#define CIMEL_HTTP_CONNECTION_H

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio.hpp>
#include <systemd/sd-daemon.h>
#include <date.h>

#include <map>
#include <memory>
#include <vector>
#include <tuple>
#include <regex>

#include "../http_connection.h"
#include "../cassandra.h"
#include "../cassandra_utils.h"
#include "../time_offseter.h"
#include "cimel_importer.h"

namespace meteodata
{

class CimelHttpRequestHandler
{
public:
	using Request = boost::beast::http::request<boost::beast::http::string_body>;
	using Response = boost::beast::http::response<boost::beast::http::string_body>;

	explicit CimelHttpRequestHandler(DbConnectionObservations& db);

	void processRequest(const Request& request, Response& response);

private:
	DbConnectionObservations& _db;

	struct StationInformation
	{
		std::string cimelId;
		TimeOffseter::PredefinedTimezone timezone;
	};

	std::map<CassUuid, StationInformation> _stations;

	bool getUuidAndCheckAccess(const Request& request, Response& response, CassUuid& uuid, const std::cmatch& url);

	void getLastArchive(const Request& request, Response& response, std::cmatch&& url);
	void postArchiveFile(const Request& request, Response& response, std::cmatch&& url);
	std::unique_ptr<CimelImporter> makeImporter(const std::cmatch& url, const CassUuid& station,
		const std::string& cimelId, TimeOffseter&& timeOffseter, DbConnectionObservations& db);

	using Route = void (CimelHttpRequestHandler::*)(const Request& request, Response& response, std::cmatch&& url);

	const std::array<std::tuple<boost::beast::http::verb, std::regex, CimelHttpRequestHandler::Route>, 2> routes = {
			std::make_tuple(
				boost::beast::http::verb::get,
				std::regex{"/imports/cimel/([0-9A-F]+)/([0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12})/last_archive/?"},
				&CimelHttpRequestHandler::getLastArchive
			),
			std::make_tuple(
				boost::beast::http::verb::post,
				std::regex{"/imports/cimel/([0-9A-F]+)/([0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12})/archive_page/((?:19|20)[0-9]{2})/?"},
				&CimelHttpRequestHandler::postArchiveFile
			)
	};

};

}


#endif //CIMEL_HTTP_CONNECTION_H
