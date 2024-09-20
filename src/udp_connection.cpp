/**
 * @file http_connection.cpp
 * @brief Implementation of the UdpConnection class
 * @author Laurent Georget
 * @date 2024-06-21
 */
/*
 * Copyright (C) 2024 SAS Météo Concept <contact@meteo-concept.fr>
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

#include <boost/asio/ip/udp.hpp>
#include <boost/system/error_code.hpp>
#include <chrono>

#include <dbconnection_observations.h>

#include "udp_connection.h"
#include "nbiot/nbiot_udp_request_handler.h"
#include "async_job_publisher.h"

namespace meteodata
{
namespace asio = boost::asio;
namespace sys = boost::system;
using udp = boost::asio::ip::udp;

UdpConnection::UdpConnection(boost::asio::io_context& io, DbConnectionObservations& db, AsyncJobPublisher* jobPublisher) :
	Connector{io, db},
	_jobPublisher{jobPublisher},
	_socket{io},
	_nbiotHandler{_db, _jobPublisher}
{
	_status.activeSince = date::floor<chrono::seconds>(chrono::system_clock::now());
}

void UdpConnection::start()
{
	_nbiotHandler.reloadStations();

	_stopped = false;

	_socket.open(udp::v4());
	_socket.bind(udp::endpoint{udp::v4(), CONNECTOR_PORT});
	readRequest();

	_status.shortStatus = "Waiting for requests";
	_status.lastReloaded = date::floor<chrono::seconds>(chrono::system_clock::now());
	_status.nbDownloads = 0;
}

void UdpConnection::stop()
{
	_stopped = true;

	if (_socket.is_open()) {
		_socket.cancel();
		_socket.close();
	}

	_status.shortStatus = "Stopped";
}

void UdpConnection::reload()
{
	// since this is a simple UDP connection, reloading is just closing
	// and reopening the socket
	stop();
	start();
}

void UdpConnection::readRequest()
{
	auto self = shared_from_this();
	_socket.async_receive_from(asio::buffer(_buffer), _remote,
		// self is used to keep the current instance alive during the
		// asynchronous wait
		[this,self](sys::error_code ec, std::size_t size) {
			if (!_stopped) {
				readRequest();
				if (!ec) {
					processRequest(size);
				}
			}
		}
	);
}

void UdpConnection::processRequest(std::size_t size)
{
	auto self = shared_from_this();

	_nbiotHandler.processRequest(std::string{_buffer.data(), size},
		[this, self](const std::string& response) {
			_socket.async_send_to(asio::buffer(response), _remote,
				[this, self](sys::error_code ec, std::size_t size) {
				 if (ec) {
					std::cerr << SD_ERR << "[UDP] protocol: Failed sending downlink to " << _remote << std::endl;
				 }
			});
	});
}


}
