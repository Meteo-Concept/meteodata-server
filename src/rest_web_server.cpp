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

#include <memory>
#include <chrono>

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <date.h>

#include "rest_web_server.h"
#include "connector.h"
#include "async_job_publisher.h"

namespace meteodata
{

namespace http = boost::beast::http;
namespace asio = boost::asio;
namespace chrono = std::chrono;
using tcp = boost::asio::ip::tcp;

RestWebServer::RestWebServer(asio::io_context& io, DbConnectionObservations& db,
							 AsyncJobPublisher* jobPublisher) :
	Connector{io, db},
	_acceptor{io, tcp::endpoint{tcp::v4(), 5887}},
	_stopped{true}
{
	_status.shortStatus = "IDLE";
	_status.nextDownload = date::floor<chrono::seconds>(chrono::system_clock::now());
	_status.nbDownloads = -1;
}

void RestWebServer::start()
{
	_status.shortStatus = "OK";
	_stopped = false;
	auto now = date::floor<chrono::seconds>(chrono::system_clock::now());
	_status.activeSince = now;
	_status.lastReloaded = now;
	acceptConnection();
}

void RestWebServer::stop()
{
	_status.shortStatus = "STOPPED";
	_stopped = true;
	_acceptor.close();
}

void RestWebServer::reload()
{
	_status.lastReloaded = date::floor<chrono::seconds>(chrono::system_clock::now());
}

void RestWebServer::acceptConnection()
{
	if (_stopped)
		return;

	auto self = shared_from_this();
	auto connection = std::make_shared<HttpConnection>(_ioContext, _db, _jobPublisher);
	_acceptor.async_accept(connection->getSocket(), [self, this, connection](const boost::system::error_code& error) {
		serveHttpConnection(connection, error);
	});
}

void RestWebServer::serveHttpConnection(const std::shared_ptr<HttpConnection>& connection, const boost::system::error_code& error)
{
	acceptConnection();
	if (!error) {
		connection->start();
	}
}

}
