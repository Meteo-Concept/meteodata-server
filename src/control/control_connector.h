/**
 * @file control_connector.h
 * @brief Definition of the ControlConnector class
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

#ifndef CONTROL_CONNECTOR_H
#define CONTROL_CONNECTOR_H

#include <iostream>
#include <memory>
#include <string>
#include <functional>
#include <unistd.h>

#include <boost/system/error_code.hpp>
#include <boost/asio.hpp>
#include <boost/asio/basic_waitable_timer.hpp>
#include <date.h>

namespace meteodata
{

namespace asio = boost::asio;
namespace sys = boost::system;
namespace chrono = std::chrono;

using namespace std::placeholders;
using namespace meteodata;

class MeteoServer;

/**
 * @brief A connector to receive commands from external programs on the control
 * socket
 */
class ControlConnector : std::enable_shared_from_this<ControlConnector>
{
public:
	/**
	 * @brief Construct a new ControlConnector
	 *
	 * @param ioContext the Boost::Asio service to use for all asynchronous
	 * network operations
	 * @param meteoServer a reference to the owning meteoServer that will have
	 * the answer for most of the queries received on the control socket
	 */
	ControlConnector(boost::asio::io_context& ioContext, MeteoServer& meteoServer);

	//main loop
	void start();

private:
	/**
	 * @brief Represent a state in the state machine
	 */
	enum class State
	{
		STARTING,                       /*!< Initial state                                                      */
		WAITING_COMMAND,                /*!< Waiting for the timer to hit the deadline for the next measurement */
		SENDING_ANSWER,                 /*!< Waiting for the identification request to be sent                  */
		STOPPED                         /*!< Final state for cleanup operations                                 */
	};
	/* Events have type sys::error_code */

	/**
	 * @brief Check an event and handle generic errors such as timeouts and
	 * I/O errors
	 *
	 * This method can be called upon receiving an event from the socket,
	 * it does nothing in case of a success or a cancellation event. It aborts
	 * the connection on a low-level communication error or timeout.
	 *
	 * @param e the event from the station
	 */
	void handleGenericErrors(const sys::error_code& e);

	/**
	 * @brief Transition function of the state machine
	 *
	 * This method receives an event from the socket and reacts according
	 * to the current state before waiting for the next event.
	 *
	 * @param e an event from the station (success, timeout, I/O error,
	 * etc.)
	 */
	void handleEvent(const sys::error_code& e);

	/**
	 * @brief Verify that the timer that has just expired really means that
	 * the associated deadline has expired
	 *
	 * This method is associated with \a _timer .
	 *
	 * @param e an error code which is positioned to a specific value if the
	 * timer has been interrupted before the deadline
	 */
	void checkDeadline(const sys::error_code& e);

	/**
	 * @brief Cease all operations and close the socket
	 *
	 * Calling this method allows for the ControlConnector to be
	 * destroyed and its memory reclaimed because the only permanent shared
	 * pointer to it is owned by the Boost::Asio::io_context which exits
	 * when the socket is closed.
	 */
	void stop();

	/**
	 * @brief Send a buffer over to the control program
	 */
	void sendData();
	/**
	 * @brief Wait for some message, a command, on the socket
	 */
	void recvData();
	/**
	 * @brief Empty the communication buffer
	 */
	void flushSocket();

	/**
	 * Wait on the main timer for a set duration (from now) and call checkDeadline
	 * when it expires
	 * @tparam Rep The duration template parameter
	 * @tparam Period The ratio duration template parameter
	 * @param expiryTime The expiry time after which the checkDeadline will be called
	 * (unless the timer is cancelled)
	 */
	template<typename Rep, typename Period>
	void countdown(const std::chrono::duration<Rep, Period>& expiryTime)
	{
		auto self(std::static_pointer_cast<ControlConnector>(shared_from_this()));
		_timer.expires_after(expiryTime);
		_timer.async_wait([this, self](const sys::error_code& e) { checkDeadline(e); });
	}

	/**
	 * @brief A back-reference to the main orchestrator of all weather station
	 * connectors
	 */
	MeteoServer& _meteoServer;

	/**
	 * @brief The current state of the state machine
	 */
	State _currentState = State::STOPPED;
	/**
	 * @brief A Boost::Asio timer used to time out on network operations and
	 * to wait between two measurements
	 */
	asio::basic_waitable_timer<chrono::steady_clock> _timer;

	asio::local::stream_protocol::socket _sock;

	/**
	 * @brief A dummy buffer to empty the socket to reset it
	 */
	asio::streambuf _discardBuffer;

	asio::streambuf _queryBuffer;

	asio::streambuf _answerBuffer;

	constexpr static size_t QUERY_MAX_SIZE = 4096;
};

}

#endif
