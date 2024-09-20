/**
 * @file http_connection.h
 * @brief Definition of the UdpConnection class
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
#ifndef UDP_CONNECTION_H
#define UDP_CONNECTION_H

#include <boost/asio/buffer.hpp>
#include <iostream>
#include <memory>

#include <boost/beast.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ip/udp.hpp>
#include <dbconnection_observations.h>

#include "connector.h"
#include "async_job_publisher.h"
#include "nbiot/nbiot_udp_request_handler.h"

namespace meteodata
{
class UdpConnection : public Connector
{
public:
	UdpConnection(boost::asio::io_context& io, DbConnectionObservations& db, AsyncJobPublisher* jobPublisher = nullptr);
	void start();
	void stop();
	void reload();


private:
	AsyncJobPublisher* _jobPublisher;
	boost::asio::ip::udp::socket _socket;
	boost::asio::ip::udp::endpoint _remote;
	std::array<char, 4096> _buffer{};

	bool _stopped = true;

	NbiotUdpRequestHandler _nbiotHandler;

	void readRequest();
	void processRequest(std::size_t size);
	void checkDeadline(const boost::system::error_code& e);

	static constexpr unsigned int CONNECTOR_PORT = 5888;
};
}

#endif //UDP_CONNECTION_H
