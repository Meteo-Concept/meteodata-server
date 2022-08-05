/**
 * @file control_connector.cpp
 * @brief Implementation of the ControlConnector class
 * @author Laurent Georget
 * @date 2022-08-01
 */
/*
 * Copyright (C) 2022  SAS Météo Concept <contact@meteo-concept.fr>
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

#include <iostream>
#include <memory>
#include <iterator>
#include <chrono>
#include <cstring>
#include <systemd/sd-daemon.h>

#include <boost/system/error_code.hpp>
#include <boost/asio.hpp>
#include <boost/asio/basic_waitable_timer.hpp>

#include "../meteo_server.h"
#include "control_connector.h"
#include "query_handler.h"
#include "connectors_query_handler.h"
#include "general_query_handler.h"

namespace ip = boost::asio::ip;
namespace asio = boost::asio;
namespace sys = boost::system;
namespace chrono = std::chrono;

namespace meteodata
{

using namespace std::placeholders;
using namespace date;

ControlConnector::ControlConnector(boost::asio::io_context& ioContext,
								   MeteoServer& meteoServer) :
		_timer{ioContext},
		_sock{ioContext},
		_meteoServer{meteoServer}
{
	auto connectorsHandler = std::make_unique<ConnectorsQueryHandler>(meteoServer);
	auto generalHandler = std::make_unique<GeneralQueryHandler>(meteoServer);

	generalHandler->setNext(std::move(connectorsHandler));
	_queryHandlerChain = std::move(generalHandler);
}

void ControlConnector::start()
{
	_currentState = State::STARTING;
	handleEvent(sys::errc::make_error_code(sys::errc::success));
}

void ControlConnector::checkDeadline(const sys::error_code& e)
{
	/* if the timer has been cancelled, then give up, we have nothing more
	 * to do here. */
	if (e == sys::errc::operation_canceled)
		return;

	// verify that the timeout is not spurious
	if (_timer.expires_at() <= chrono::steady_clock::now()) {
		// The client might leave us before we reach this point
		if (_sock.is_open())
			_sock.cancel();
		handleEvent(sys::errc::make_error_code(sys::errc::timed_out));
	} else {
		/* spurious handler call, restart the timer without changing the
		 * deadline */
		auto self = shared_from_this();
		_timer.async_wait([this, self](const sys::error_code& e) { checkDeadline(e); });
	}
}

void ControlConnector::stop()
{
	/* hijack the state machine to force STOPPED state */
	_currentState = State::STOPPED;
	/* cancel all asynchronous event handlers */
	_timer.cancel();
	/* close the socket (but check if it's not been wrecked by the other
	 * end before)
	 */
	if (_sock.is_open()) {
		_sock.cancel();
		_sock.close();
	}

	/* After the return of this function, no one should hold a pointer to
	 * self (aka this) anymore, so this instance will be destroyed */
}

void ControlConnector::sendData()
{
	auto self = shared_from_this();
	countdown(chrono::seconds(6));

	// passing buffer to the lambda keeps the buffer alive
	asio::async_write(_sock, _answerBuffer.data(),
		[this, self](const sys::error_code& ec, std::size_t n) {
			if (ec == sys::errc::operation_canceled)
				return;
			_timer.cancel();
			if (ec == sys::errc::success)
				_answerBuffer.consume(n);
			handleEvent(ec);
		});
}

void ControlConnector::recvData()
{
	auto self = shared_from_this();
	asio::async_read_until(_sock, _queryBuffer, "\n", [this, self](const sys::error_code& ec, std::size_t n) {
		if (ec == sys::errc::operation_canceled)
			return;
		_timer.cancel();
		handleEvent(ec);
	});
}

void ControlConnector::flushSocket()
{
	auto self = shared_from_this();
	// wait before flushing in order not to leave garbage behind
	_timer.expires_from_now(chrono::seconds(10));
	_timer.async_wait([this, self](const sys::error_code& ec) {
		if (ec == sys::errc::operation_canceled)
			return;
		if (_sock.available() > 0) {
			auto bufs = _discardBuffer.prepare(512);
			std::size_t bytes = _sock.receive(bufs);
			_discardBuffer.commit(bytes);
			_discardBuffer.consume(_discardBuffer.size());
		}
		_currentState = State::WAITING_COMMAND;
	});
}

void ControlConnector::handleGenericErrors(const sys::error_code& e)
{
	/* In case of success, we are done here, the main code in the state
	 * machine will do its job
	 *
	 * if the operation has been canceled, then we let the control to the
	 * timer handler running concurrently
	 * For example:
	 * Normal execution:
	 * - the async_wait is started on the timer
	 * - the async_read starts on the socket
	 * - the async_read completes with success
	 * - the async_wait gets cancelled
	 *   => we let the async_read handler send its success event to the
	 *   state machine
	 *   (the timer's operation_canceled event has been discarded in
	 *   checkDeadline, above)
	 *
	 * Timeout execution:
	 * - the async_wait is started on the timer
	 * - the async_read starts on the socket
	 * - the async_wait completes before the async_read
	 * - the async_read gets cancelled
	 *   => we let the async_wait handler send its timed_out event to the
	 *   state machine and discard the socket's operation_canceled event
	 */
	if (e == sys::errc::success || e == sys::errc::operation_canceled) {
		return;
	} else if (e == sys::errc::timed_out) {
		std::cerr << SD_ERR << "[Control connection]: Timeout, aborting" << std::endl;
		stop();
	} else if (e == asio::error::eof) {
		std::cerr << SD_NOTICE << "[Control connection]: Client disconnected" << std::endl;
		stop();
	} else { /* TCP reset by peer, etc. */
		std::cerr << SD_ERR << "[Control connection]: " << "unknown network error: " << e.message()
				  << std::endl;
		stop();
	}
}

void ControlConnector::handleEvent(const sys::error_code& e)
{
	switch (_currentState) {
		case State::STARTING:
			_currentState = State::WAITING_COMMAND;
			recvData();
			break;

		case State::WAITING_COMMAND:
			handleGenericErrors(e);
			if (e == sys::errc::success) {
				// empty the answer buffer if needs be
				_answerBuffer.consume(_answerBuffer.in_avail());

				std::string query;
				std::istream is{&_queryBuffer};
				std::getline(is, query);
				if (!query.empty()) {
					_queryBuffer.consume(query.size());

					std::string answer = _queryHandlerChain->handleQuery(query);
					std::ostream os{&_answerBuffer};
					os << answer;
					if (answer.empty() || answer.at(answer.length() - 1) != '\n')
						os << '\n';

					_currentState = State::SENDING_ANSWER;
					sendData();
				} else {
					// client has sent an empty line, that's disconcerting, but
					// we should keep listening anyway
					recvData();
				}
			}
			break;

		case State::SENDING_ANSWER:
			handleGenericErrors(e);
			if (e == sys::errc::success) {
				// empty the query buffer if needs be
				_queryBuffer.consume(_queryBuffer.in_avail());
				flushSocket();

				_currentState = State::WAITING_COMMAND;
				recvData();
			}
			break;

		case State::STOPPED:
			/* discard everything, only spurious events from cancelled
				operations can get here */
			break;
	}
}

}

