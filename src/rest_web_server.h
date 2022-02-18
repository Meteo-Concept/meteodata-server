/**
 * @file rest_web_server.h
 * @brief Definition of the RestWebServer class
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
#ifndef REST_WEB_SERVER_H
#define REST_WEB_SERVER_H

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <string>
#include <memory>

#include <dbconnection_observations.h>

#include "http_connection.h"

namespace meteodata
{

class RestWebServer : public std::enable_shared_from_this<RestWebServer>
{
public:
	RestWebServer(boost::asio::io_context& io, DbConnectionObservations& db);

	// Start accepting incoming connections
	void start();


private:
	boost::asio::io_context& _io;
	boost::asio::ip::tcp::acceptor _acceptor;
	boost::asio::ip::tcp::socket _socket;
	DbConnectionObservations& _db;

	void serveHttpConnection(boost::asio::ip::tcp::socket&& socket, const boost::system::error_code& error);
};

}

#endif //REST_WEB_SERVER_H
