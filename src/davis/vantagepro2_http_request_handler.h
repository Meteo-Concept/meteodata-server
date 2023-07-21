/**
 * @file vantagepro2_http_connection.h
 * @brief Definition of the VantagePro2HttpConnection class
 * @author Laurent Georget
 * @date 2021-12-23
 */
/*
 * Copyright (C) 2021 SAS Météo Concept <contact@meteo-concept.fr>
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
#ifndef VANTAGEPRO2_HTTP_CONNECTION_H
#define VANTAGEPRO2_HTTP_CONNECTION_H

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio.hpp>
#include <systemd/sd-daemon.h>
#include <date.h>
#include <dbconnection_observations.h>

#include <map>
#include <vector>
#include <tuple>
#include <regex>

#include "async_job_publisher.h"
#include "http_connection.h"
#include "cassandra.h"
#include "cassandra_utils.h"
#include "time_offseter.h"
#include "davis/vantagepro2_archive_message.h"

namespace meteodata
{

class VantagePro2HttpRequestHandler
{
public:
	using Request = boost::beast::http::request<boost::beast::http::string_body>;
	using Response = boost::beast::http::response<boost::beast::http::string_body>;

	explicit VantagePro2HttpRequestHandler(DbConnectionObservations& db, AsyncJobPublisher* jobPublisher = nullptr);

	void processRequest(const Request& request, Response& response);

private:
	DbConnectionObservations& _db;

	AsyncJobPublisher* _jobPublisher;

	struct ClientInformation
	{
		std::string authorizedUser;
		TimeOffseter::PredefinedTimezone timezone{TimeOffseter::PredefinedTimezone::UTC};
	};

	std::map<CassUuid, ClientInformation> _userAndTimezoneByStation;

	bool getUuidAndCheckAccess(const Request& request, Response& response, CassUuid& uuid, const std::cmatch& url);

	void getLastArchive(const Request& request, Response& response, std::cmatch&& url);

	void postArchivePage(const Request& request, Response& response, std::cmatch&& url);

	using Route = void (VantagePro2HttpRequestHandler::*)(const Request& request, Response& response,
														  std::cmatch&& url);

	const std::array<std::tuple<boost::beast::http::verb, std::regex, VantagePro2HttpRequestHandler::Route>, 2> routes = {
		std::make_tuple(boost::beast::http::verb::get, std::regex{
			"/imports/vp2/([0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12})/last_archive/?"},
			&VantagePro2HttpRequestHandler::getLastArchive),
		std::make_tuple(boost::beast::http::verb::post, std::regex{
			"/imports/vp2/([0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12})/archive_page/?"},
			&VantagePro2HttpRequestHandler::postArchivePage)};

};

}


#endif //VANTAGEPRO2_HTTP_CONNECTION_H
