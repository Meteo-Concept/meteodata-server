/**
 * @file vantagepro2connector.cpp
 * @brief Implementation of the VantagePro2Connector class
 * @author Laurent Georget
 * @date 2016-10-05
 */
/*
 * Copyright (C) 2016  SAS Météo Concept <contact@meteo-concept.fr>
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
#include <array>
#include <functional>

#include <boost/system/error_code.hpp>
#include <boost/asio.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/asio/deadline_timer.hpp>

#include <syslog.h>
#include <unistd.h>

#include <cassandra.h>

#include "vantagepro2connector.h"
#include "connector.h"
#include "vantagepro2message.h"

namespace meteodata {

namespace ip = boost::asio::ip;
namespace asio = boost::asio;
namespace sys = boost::system; //system() is a function, it cannot be redefined
//as a namespace
namespace chrono = boost::posix_time;

using namespace std::placeholders;

constexpr char VantagePro2Connector::_echoRequest[];
constexpr char VantagePro2Connector::_getStationRequest[];
constexpr char VantagePro2Connector::_getMeasureRequest[];

VantagePro2Connector::VantagePro2Connector(boost::asio::io_service& ioService,
	DbConnection& db) :
	Connector(ioService, db),
	_timer(ioService)
{
}

void VantagePro2Connector::start()
{
	auto self(std::static_pointer_cast<VantagePro2Connector>(shared_from_this()));
	_currentState = State::STARTING;

	handleEvent(sys::errc::make_error_code(sys::errc::success));
}


void VantagePro2Connector::waitForNextMeasure()
{
	auto self(std::static_pointer_cast<VantagePro2Connector>(shared_from_this()));

	// reset the counters
	_timeouts = 0;
	_transmissionErrors = 0;

	chrono::time_duration now = chrono::seconds(chrono::second_clock::local_time().time_of_day().total_seconds());
	// extract the difference between now  and now rounded up to the next multiple of 5 minutes
	// e.g.: if now = 11:27:53, then rounded = 2min 53s
	chrono::time_duration rounded = chrono::minutes(now.minutes() % _pollingPeriod) + chrono::seconds(now.seconds());
	// we wait 5 min - rounded to take the next measurement exactly
	// when the clock hits the next multiple of five minutes (tolerating
	// an error of around a couple of seconds)
	std::cerr << "Next measurement will be taken in " << (chrono::minutes(_pollingPeriod) - rounded)
		  << " at approximately " << (now + chrono::minutes(_pollingPeriod) - rounded) << std::endl;
	_timer.expires_from_now(chrono::minutes(_pollingPeriod) - rounded);
	_timer.async_wait(std::bind(&VantagePro2Connector::checkDeadline, self, _1));
}


void VantagePro2Connector::checkDeadline(const sys::error_code& e)
{
	/* if the timer has been cancelled, then bail out, we have nothing more
	 * to do here. It's our original caller's responsability to restart us
	 * if needs be */
	std::cerr << "Deadline handler hit: " << e.value() << ": " << e.message() << std::endl;
	if (e == sys::errc::operation_canceled)
		return;

	// verify that the timeout is not spurious
	if (_timer.expires_at() <= asio::deadline_timer::traits_type::now()) {
		std::cerr << "Timed out!" << std::endl;
		_timeouts++;
		_sock.cancel();
		handleEvent(sys::errc::make_error_code(sys::errc::timed_out));
	} else {
		/* spurious handler call, restart the timer without changing the
		 * deadline */
		auto self(std::static_pointer_cast<VantagePro2Connector>(shared_from_this()));
		_timer.async_wait(std::bind(&VantagePro2Connector::checkDeadline, self, _1));
	}
}

void VantagePro2Connector::stop()
{
	/* hijack the state machine to force STOPPED state */
	_currentState = State::STOPPED;
	_stopped = true;
	/* cancel all asynchronous event handlers */
	_timer.cancel();
	/* close the socket (but check if it's not been wrecked by the other
	 * end before)
	 */
	if (_sock.is_open()) {
		_sock.cancel();
		_sock.close();
	}
}

void VantagePro2Connector::sendRequest(const char *req, int reqsize)
{
	auto self(std::static_pointer_cast<VantagePro2Connector>(shared_from_this()));
	_timer.expires_from_now(chrono::seconds(6));
	_timer.async_wait(std::bind(&VantagePro2Connector::checkDeadline, self, _1));
	async_write(_sock, asio::buffer(req, reqsize),
		[this,self](const sys::error_code& ec, std::size_t) {
			if (ec == sys::errc::operation_canceled)
				return;
			_timer.cancel();
			handleEvent(ec);
		}
	);
}

void VantagePro2Connector::recvWakeUp()
{
	auto self(std::static_pointer_cast<VantagePro2Connector>(shared_from_this()));
	_timer.expires_from_now(chrono::seconds(2));
	_timer.async_wait(std::bind(&VantagePro2Connector::checkDeadline, self, _1));
	async_read_until(_sock, _discardBuffer, "\n\r",
		[this,self](const sys::error_code& ec, std::size_t n) {
			if (ec == sys::errc::operation_canceled)
				return;
			_timer.cancel();
			_discardBuffer.consume(n);
			handleEvent(ec);
		}
	);
}

void VantagePro2Connector::recvAck()
{
	auto self(std::static_pointer_cast<VantagePro2Connector>(shared_from_this()));
	_timer.expires_from_now(chrono::seconds(6));
	_timer.async_wait(std::bind(&VantagePro2Connector::checkDeadline, self, _1));
	async_read(_sock, asio::buffer(&_ackBuffer, 1),
		[this,self](const sys::error_code& ec, std::size_t) {
			if (ec == sys::errc::operation_canceled)
				return;
			_timer.cancel();
			if (_ackBuffer == '\n' || _ackBuffer == '\r') {
				//we eat some garbage, discard and carry on
				recvAck();
			} else {
				handleEvent(ec);
			}
		}
	);
}

template <typename MutableBuffer>
void VantagePro2Connector::recvData(const MutableBuffer& buffer)
{
	auto self(std::static_pointer_cast<VantagePro2Connector>(shared_from_this()));
	_timer.expires_from_now(chrono::seconds(6));
	_timer.async_wait(std::bind(&VantagePro2Connector::checkDeadline, self, _1));
	async_read(_sock, buffer,
		[this,self](const sys::error_code& ec, std::size_t) {
			if (ec == sys::errc::operation_canceled)
				return;
			_timer.cancel();
			handleEvent(ec);
		}
	);
}


void VantagePro2Connector::flushSocket()
{
	while (_sock.available() > 0) {
		auto bufs = _discardBuffer.prepare(512);
		std::size_t bytes = _sock.receive(bufs);
		_discardBuffer.commit(bytes);
		std::cerr << "Cleared " << bytes << " bytes" << std::endl;
		_discardBuffer.consume(_discardBuffer.size());
	}
}

template <typename Restarter>
void VantagePro2Connector::handleGenericErrors(const sys::error_code& e, State restartState, Restarter restart)
{
	std::cerr << "Received event " << e.value() << ": " << e.message() << std::endl;
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
		if (++_timeouts < 5) {
			_currentState = restartState;
			flushSocket();
			restart();
		} else {
			syslog(LOG_ERR, "station %s: Too many timeouts, aborting", _stationName.c_str());
			stop();
		}
	} else { /* TCP reset by peer, etc. */
		syslog(LOG_ERR, "station %s: Unknown error: %s",
			e.message().c_str(), _stationName.c_str());
		stop();
	}
}

void VantagePro2Connector::handleEvent(const sys::error_code& e)
{
	switch (_currentState) {
	case State::STARTING:
		_currentState = State::SENDING_WAKE_UP_STATION;
		std::cerr << "A new station is connected" << std::endl;
		sendRequest(_echoRequest, sizeof(_echoRequest));
		break;

	case State::WAITING_NEXT_MEASURE_TICK:
		flushSocket();
		if (e == sys::errc::timed_out) {
			_currentState = State::SENDING_WAKE_UP_MEASURE;
			std::cerr << "Time to wake up! We need a new measurement" << std::endl;
			sendRequest(_echoRequest, sizeof(_echoRequest));
		} else {
			waitForNextMeasure();
		}
		break;

	case State::SENDING_WAKE_UP_STATION:
		handleGenericErrors(e, State::SENDING_WAKE_UP_STATION,
			std::bind(&VantagePro2Connector::sendRequest, this,
				_echoRequest, sizeof(_echoRequest)));
		if (e == sys::errc::success) {
			_currentState = State::WAITING_ECHO_STATION;
			std::cerr << "Sent wake up" << std::endl;
			recvWakeUp();
		}
		break;

	case State::WAITING_ECHO_STATION:
		handleGenericErrors(e, State::SENDING_WAKE_UP_STATION,
			std::bind(&VantagePro2Connector::sendRequest, this,
				_echoRequest, sizeof(_echoRequest)));
		if (e == sys::errc::success) {
			_currentState = State::SENDING_REQ_STATION;
			flushSocket();
			std::cerr << "Station has woken up" << std::endl;
			sendRequest(_getStationRequest, sizeof(_getStationRequest));
		}
		break;

	case State::SENDING_REQ_STATION:
		handleGenericErrors(e, State::SENDING_REQ_STATION,
			std::bind(&VantagePro2Connector::sendRequest, this,
				_getStationRequest, sizeof(_getStationRequest)));
		if (e == sys::errc::success) {
			_currentState = State::WAITING_ACK_STATION;
			std::cerr << "Sent identification request" << std::endl;
			recvAck();
		}
		break;

	case State::WAITING_ACK_STATION:
		handleGenericErrors(e, State::SENDING_REQ_STATION,
			std::bind(&VantagePro2Connector::sendRequest, this,
				_getStationRequest, sizeof(_getStationRequest)));
		if (e == sys::errc::success) {
			if (_ackBuffer != 0x06) {
				syslog(LOG_ERR, "station %s : Was waiting for acknowledgement, got %i", _stationName.c_str(), _ackBuffer);
				if (++_transmissionErrors < 5) {
					_currentState = State::SENDING_REQ_STATION;
					flushSocket();
					sendRequest(_getStationRequest, sizeof(_getStationRequest));
				} else {
					syslog(LOG_ERR, "station %s : Cannot get the station to acknowledge the identification request", _stationName.c_str());
					stop();
				}
			} else {
				_currentState = State::WAITING_DATA_STATION;
				std::cerr << "Identification request acked by station" << std::endl;
				recvData(asio::buffer(_coords));
			}
		}
		break;

	case State::WAITING_DATA_STATION:
		handleGenericErrors(e, State::SENDING_REQ_STATION,
			std::bind(&VantagePro2Connector::sendRequest, this,
				_getStationRequest, sizeof(_getStationRequest)));
		if (e == sys::errc::success) {
			if (!VantagePro2Message::validateCRC(_coords, sizeof(_coords))) {
				if (++_transmissionErrors < 5) {
					_currentState = State::SENDING_REQ_STATION;
					flushSocket();
					sendRequest(_getStationRequest, sizeof(_getStationRequest));
				} else {
					syslog(LOG_ERR, "station %s: Too many transmissions errors on station identification CRC validation, aborting", _stationName.c_str());
					stop();
				}
			} else {
				// From documentation, latitude, longitude and elevation are stored contiguously
				// in this order in the station's EEPROM
				_currentState = State::WAITING_NEXT_MEASURE_TICK;
				bool found = _db.getStationByCoords(_coords[2], _coords[0], _coords[1], _station, _stationName, _pollingPeriod);
				flushSocket();
				if (found) {
					std::cerr << "Received correct identification from station " << _stationName << std::endl;
					syslog(LOG_INFO, "station %s is connected", _stationName.c_str());
					waitForNextMeasure();
				} else {
					std::cerr << "Unknown station! Aborting" << std::endl;
					syslog(LOG_ERR, "An unknown station has attempted a connection");
					stop();
				}
			}
		}
		break;

	case State::SENDING_WAKE_UP_MEASURE:
		handleGenericErrors(e, State::SENDING_WAKE_UP_MEASURE,
			std::bind(&VantagePro2Connector::sendRequest, this,
				_echoRequest, sizeof(_echoRequest)));
		if (e == sys::errc::success) {
			_currentState = State::WAITING_ECHO_MEASURE;
			std::cerr << "Waking up station for next request" << std::endl;
			recvWakeUp();
		}
		break;

	case State::WAITING_ECHO_MEASURE:
		handleGenericErrors(e, State::SENDING_WAKE_UP_MEASURE,
			std::bind(&VantagePro2Connector::sendRequest, this,
				_echoRequest, sizeof(_echoRequest)));
		if (e == sys::errc::success) {
			_currentState = State::SENDING_REQ_MEASURE;
			flushSocket();
			std::cerr << "Station is woken up, ready to send a measurement" << std::endl;
			sendRequest(_getMeasureRequest, sizeof(_getMeasureRequest));
		}
		break;

	case State::SENDING_REQ_MEASURE:
		handleGenericErrors(e, State::SENDING_REQ_MEASURE,
			std::bind(&VantagePro2Connector::sendRequest, this,
				_getMeasureRequest, sizeof(_getMeasureRequest)));
		if (e == sys::errc::success) {
			_currentState = State::WAITING_ACK_MEASURE;
			std::cerr << "Sent measurement request" << std::endl;
			recvAck();
		}
		break;

	case State::WAITING_ACK_MEASURE:
		handleGenericErrors(e, State::SENDING_REQ_MEASURE,
			std::bind(&VantagePro2Connector::sendRequest, this,
				_getMeasureRequest, sizeof(_getMeasureRequest)));
		if (e == sys::errc::success) {
			if (_ackBuffer != 0x06) {
				syslog(LOG_ERR, "station %s : Was waiting for acknowledgement, got %i", _stationName.c_str(), _ackBuffer);
				if (++_transmissionErrors < 5) {
					_currentState = State::SENDING_REQ_MEASURE;
					flushSocket();
					sendRequest(_getMeasureRequest, sizeof(_getMeasureRequest));
				} else {
					syslog(LOG_ERR, "station %s : Cannot get the station to acknowledge the measurement request", _stationName.c_str());
					stop();
				}
			} else {
				_currentState = State::WAITING_DATA_MEASURE;
				std::cerr << "Station has acked measurement request" << std::endl;
				recvData(_message.getBuffer());
			}
		}
		break;

	case State::WAITING_DATA_MEASURE:
		handleGenericErrors(e, State::SENDING_REQ_MEASURE,
			std::bind(&VantagePro2Connector::sendRequest, this,
				_getMeasureRequest, sizeof(_getMeasureRequest)));
		if (e == sys::errc::success) {
			if (!_message.isValid()) {
				if (++_transmissionErrors < 5) {
					_currentState = State::SENDING_REQ_MEASURE;
					flushSocket();
					sendRequest(_getMeasureRequest, sizeof(_getMeasureRequest));
				} else {
					syslog(LOG_ERR, "station %s: Too many transmissions errors in measurement CRC, aborting", _stationName.c_str());
					stop();
				}
			} else {
				_currentState = State::WAITING_NEXT_MEASURE_TICK;
				flushSocket();
				std::cerr << "Got measurement, storing it" << std::endl;
				bool ret = _db.insertDataPoint(_station, _message);
				if (ret) {
					std::cerr << "Measurement stored\n"
						  << "Now sleeping until next measurement" << std::endl;
					waitForNextMeasure();
				} else {
					std::cerr << "Failed to store measurement! Aborting" << std::endl;
					syslog(LOG_ERR, "station %s: Couldn't store measurement", _stationName.c_str());
					stop();
				}
			}
		}
		break;

	case State::STOPPED:
		/* discard everything, only spurious events from cancelled
		    operations can get here */
		;
		break;

		break;
	}
}

}