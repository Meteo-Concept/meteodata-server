/**
 * @file http_connection.cpp
 * @brief Implementation of the HttpConnection class
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

#include <boost/beast.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/basic_waitable_timer.hpp>
#include <boost/system/error_code.hpp>
#include <chrono>

#include <cassobs/dbconnection_observations.h>

#include "http_connection.h"
#include "async_job_publisher.h"
#include "davis/vantagepro2_http_request_handler.h"
#include "davis/monitorII_http_request_handler.h"
#include "cimel/cimel_http_request_handler.h"
#include "liveobjects/liveobjects_http_decoding_request_handler.h"

namespace meteodata
{
namespace beast = boost::beast;
namespace http = beast::http;
namespace chrono = std::chrono;
namespace sys = boost::system;
using tcp = boost::asio::ip::tcp;

HttpConnection::HttpConnection(boost::asio::io_context& io, DbConnectionObservations& db, AsyncJobPublisher* jobPublisher) :
	_ioContext{io},
	_db{db},
	_jobPublisher{jobPublisher},
	_socket{io},
	_timeout{io}
{
}

void HttpConnection::start()
{
	_timeout.expires_from_now(chrono::seconds(60));
	auto self = shared_from_this();
	_timeout.async_wait([self, this](const sys::error_code& ec) {
		checkDeadline(ec);
	});
	readRequest();
}

void HttpConnection::checkDeadline(const sys::error_code& e)
{
	/* if the timer has been cancelled, then bail out, we have nothing more
	 * to do here. It's our original caller's responsibility to restart us
	 * if needs be */
	if (e == sys::errc::operation_canceled)
		return;

	// verify that the timeout is not spurious
	if (_timeout.expires_at() <= chrono::steady_clock::now()) {
		// there's a chance the socket has already been wrecked by the
		// remote end
		if (_socket.is_open()) {
			_socket.cancel();
			_socket.shutdown(tcp::socket::shutdown_both);
		}
	} else {
		/* spurious handler call, restart the timer without changing the
		 * deadline */
		auto self = shared_from_this();
		_timeout.async_wait([self, this](const sys::error_code& ec) {
			checkDeadline(ec);
		});
	}
}

void HttpConnection::readRequest()
{
	auto self = shared_from_this();

	http::async_read(_socket, _buffer, _request,
		[self, this](beast::error_code ec, std::size_t) {
		if (!ec)
			processRequest();
		// we only cancel the timeout and the end of the full round-trip
	});
}

void HttpConnection::processRequest()
{
	auto url = _request.target();

	if (url.substr(0, 13) == "/imports/vp2/") {
		VantagePro2HttpRequestHandler handler{_db, _jobPublisher};
		handler.processRequest(_request, _response);
	} else if (url.substr(0, 19) == "/imports/monitorII/") {
		MonitorIIHttpRequestHandler handler{_db, _jobPublisher};
		handler.processRequest(_request, _response);
	} else if (url.substr(0, 15) == "/imports/cimel/") {
		CimelHttpRequestHandler handler{_db, _jobPublisher};
		handler.processRequest(_request, _response);
	} else if (url.substr(0, 28) == "/imports/decode/liveobjects") {
		LiveobjectsHttpDecodingRequestHandler handler{_db};
		handler.processRequest(_request, _response);
	} else {
		_response.result(http::status::not_found);
	}
	_timeout.expires_from_now(chrono::seconds(60));
	writeResponse();
}


void HttpConnection::writeResponse()
{
	auto self = shared_from_this();

	_response.set(boost::beast::http::field::server, "Meteodata");
	_response.keep_alive(_request.keep_alive());
	_response.prepare_payload();

	http::async_write(_socket, _response, [self, this](beast::error_code ec, std::size_t) {
		if (ec) {
			std::cerr << SD_ERR << "[HTTP] protocol: " << "Failed to send the response " << ec << std::endl;
		}
		if (_socket.is_open()) {
			_socket.shutdown(tcp::socket::shutdown_both, ec);
			if (ec) {
				std::cerr << SD_ERR << "[HTTP] protocol: " << "Socket shutdown failure " << ec << std::endl;
			}
		}
		_timeout.cancel();
	});
}
}
