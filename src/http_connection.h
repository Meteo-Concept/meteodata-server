/**
 * @file http_connection.h
 * @brief Definition of the HttpConnection class
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
#ifndef HTTP_CONNECTION_H
#define HTTP_CONNECTION_H

#include <iostream>
#include <memory>

#include <boost/beast.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <dbconnection_observations.h>

namespace meteodata
{
	class HttpConnection : public std::enable_shared_from_this<HttpConnection>
	{
	public:
		HttpConnection(boost::asio::ip::tcp::socket&& socket, DbConnectionObservations& db);
		void start();


	private:
		boost::asio::ip::tcp::socket _socket;
		DbConnectionObservations& _db;
		boost::beast::flat_buffer _buffer{4096};
		boost::beast::http::request<boost::beast::http::string_body> _request;
		boost::beast::http::response<boost::beast::http::string_body> _response;
		boost::asio::steady_timer _timeout{_socket.get_executor()};

		void readRequest();
		void processRequest();
		void writeResponse();
		void checkDeadline(const boost::system::error_code& e);
	};
}

#endif //HTTP_CONNECTION_H
