/**
 * @file rest_web_server.cpp
 * @brief Implementation of the RestWebServer class
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

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <memory>

#include "rest_web_server.h"

namespace meteodata
{

namespace http = boost::beast::http;
namespace asio = boost::asio;
using tcp = boost::asio::ip::tcp;

RestWebServer::RestWebServer(asio::io_context& io, DbConnectionObservations& db) :
	_io{io},
	_acceptor{io, tcp::endpoint{tcp::v4(), 5887}},
	_db{db}
{}

void RestWebServer::start()
{
	auto self = shared_from_this();
	auto connection = std::make_shared<HttpConnection>(_io, _db);
	_acceptor.async_accept(connection->getSocket(), [self, this, connection](const boost::system::error_code& error) {
		serveHttpConnection(connection, error);
	});
}

void RestWebServer::serveHttpConnection(const std::shared_ptr<HttpConnection>& connection, const boost::system::error_code& error)
{
	start();
	if (!error) {
		connection->start();
	}
}

}
