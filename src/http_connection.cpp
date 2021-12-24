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
#include <boost/asio/socket_base.hpp>
#include <boost/asio/basic_waitable_timer.hpp>
#include <boost/system/error_code.hpp>
#include <memory>
#include <chrono>

#include "http_connection.h"
#include "davis/vantagepro2_http_request_handler.h"
#include <dbconnection_observations.h>

namespace meteodata
{
	namespace beast = boost::beast;
	namespace http = beast::http;
	namespace chrono = std::chrono;
	namespace sys = boost::system;
	using tcp = boost::asio::ip::tcp;

	HttpConnection::HttpConnection(boost::asio::ip::tcp::socket&& socket,
								   meteodata::DbConnectionObservations& db) :
		_socket{std::move(socket)},
		_db{db}
	{
	}

	void HttpConnection::start()
	{
		readRequest();
		_timeout.expires_from_now(chrono::seconds(60));
		auto self = shared_from_this();
		_timeout.async_wait([self,this](const sys::error_code& ec) {
			checkDeadline(ec);
		});
	}

	void HttpConnection::checkDeadline(const sys::error_code& e)
	{
		/* if the timer has been cancelled, then bail out, we have nothing more
		 * to do here. It's our original caller's responsability to restart us
		 * if needs be */
		if (e == sys::errc::operation_canceled)
			return;

		// verify that the timeout is not spurious
		if (_timeout.expires_at() <= chrono::steady_clock::now()) {
			_socket.cancel();
			_socket.shutdown(tcp::socket::shutdown_send);
		} else {
			/* spurious handler call, restart the timer without changing the
			 * deadline */
			auto self = shared_from_this();
			_timeout.async_wait([self,this](const sys::error_code& ec) {
				checkDeadline(ec);
			});
		}
	}

    void HttpConnection::readRequest()
    {
        auto self = shared_from_this();

        http::async_read(
            _socket,
            _buffer,
            _request,
            [self](beast::error_code ec, std::size_t)
            {
                if(!ec)
                    self->processRequest();
            }
		);
    }

    void HttpConnection::processRequest()
    {
		// One day, we'll have to test the request to know which is the appropriate handler for the request.
		// For now, we have juste one.
		VantagePro2HttpRequestHandler handler{_db};
		handler.processRequest(_request, _response);
		writeResponse();
		_timeout.expires_from_now(chrono::seconds(60));
    }


    void HttpConnection::writeResponse()
    {
        auto self = shared_from_this();

        _response.content_length(_response.body().size());

        http::async_write(
            _socket,
            _response,
            [self,this](beast::error_code ec, std::size_t)
            {
                _socket.shutdown(tcp::socket::shutdown_send, ec);
            }
		);
    }
}
