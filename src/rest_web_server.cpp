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
		_socket{_io},
		_db{db}
{}

void RestWebServer::start()
{
	auto self = shared_from_this();
	_acceptor.async_accept(_socket, [self, this](const boost::system::error_code& error) {
		serveHttpConnection(std::move(_socket), error);
		_socket = tcp::socket{_io};
	});
}

void RestWebServer::serveHttpConnection(boost::asio::ip::tcp::socket&& socket, const boost::system::error_code& error)
{
	if (!error) {
		auto connection = std::make_shared<HttpConnection>(std::forward<boost::asio::ip::tcp::socket>(socket), _db);
		connection->start();
	}
	start();
}

}
