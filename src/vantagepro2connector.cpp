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

	/* Start the timer once and for all */
	_timer.expires_at(chrono::pos_infin);
	_timer.async_wait(std::bind(&VantagePro2Connector::checkDeadline, self, _1));

	handleEvent(sys::errc::make_error_code(sys::errc::success));
}


void VantagePro2Connector::waitForNextMeasure()
{
	auto self(std::static_pointer_cast<VantagePro2Connector>(shared_from_this()));
	chrono::time_duration now = chrono::seconds(chrono::second_clock::local_time().time_of_day().total_seconds());
	// extract the difference between now  and now rounded up to the next multiple of 5 minutes
	// e.g.: if now = 11:27:53, then rounded = 2min 53s
	chrono::time_duration rounded = chrono::minutes(now.minutes() % 5) + chrono::seconds(now.seconds());
	// we wait 5 min - rounded to take the next measurement exactly
	// when the clock hits the next multiple of five minutes (tolerating
	// an error of around a couple of seconds)
	std::cerr << "Next measurement will be taken in " << (chrono::minutes(5) - rounded)
		  << " at approximately " << (now + chrono::minutes(5) - rounded) << std::endl;
	_timer.expires_from_now(chrono::minutes(5) - rounded);
}


void VantagePro2Connector::checkDeadline(const sys::error_code& e)
{
	/* if the timer has been cancelled, then bail out, we have nothing more
	 * to do here. It's our original caller's responsability to restart us
	 * if needs be */
	if (e == sys::errc::operation_canceled)
		return;

	// verify that the timeout is not spurious
	if (_timer.expires_at() <= asio::deadline_timer::traits_type::now()) {
		std::cerr << "Timed out!" << std::endl;
		_timeouts++;
		/* Trigger an operation_aborted event on the current
		 * asynchronous I/O */
		_sock.cancel();
	} else {
		/* spurious handler call, restart the timer without changing the
		 * same deadline */
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
	_sock.close();
	/* at this point, all shared pointers to *this should be about to be
	 * destroyed, eventually leading to the VantagePro2Connector to be
	 * destroyed */
}

void VantagePro2Connector::sendRequest(const char *req, int reqsize)
{
	auto self(std::static_pointer_cast<VantagePro2Connector>(shared_from_this()));
	_timer.expires_from_now(chrono::seconds(6));
	async_write(_sock, asio::buffer(req, reqsize),
		[this,self](const sys::error_code& ec, std::size_t) {
			_timer.expires_at(chrono::pos_infin);
			handleEvent(ec);
		}
	);
}

void VantagePro2Connector::recvWakeUp()
{
	auto self(std::static_pointer_cast<VantagePro2Connector>(shared_from_this()));
	_timer.expires_from_now(chrono::seconds(6));
	async_read_until(_sock, _discardBuffer, "\n\r",
		[this,self](const sys::error_code& ec, std::size_t) {
			_timer.expires_at(chrono::pos_infin);
			handleEvent(ec);
		}
	);
}

template <typename MutableBuffer>
void VantagePro2Connector::waitForData(const MutableBuffer& buffer)
{
	auto self(std::static_pointer_cast<VantagePro2Connector>(shared_from_this()));
	_timer.expires_from_now(chrono::seconds(6));
	async_read(_sock, buffer,
		[this,self](const sys::error_code& ec, std::size_t) {
			_timer.expires_at(chrono::pos_infin);
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
	if (e == sys::errc::success) {
		return;
	} else if (e == sys::errc::timed_out || e == sys::errc::operation_canceled) {
		if (++_timeouts < 5) {
			_currentState = restartState;
			restart();
		} else {
			stop();
		}
	} else if (e == sys::errc::io_error) {
		if (++_transmissionErrors < 5) {
			_currentState = restartState;
			restart();
		} else {
			stop();
		}
	} else {
		syslog(LOG_ERR, "unknown error: %s",
			e.message().c_str());
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
		_currentState = State::SENDING_WAKE_UP_MEASURE;
		std::cerr << "Time to wake up! We need a new measurement" << std::endl;
		sendRequest(_echoRequest, sizeof(_echoRequest));
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
			waitForData(asio::buffer(&_ackBuffer,1));
		}
		break;

	case State::WAITING_ACK_STATION:
		handleGenericErrors(e, State::SENDING_REQ_STATION,
			std::bind(&VantagePro2Connector::sendRequest, this,
				_getStationRequest, sizeof(_getStationRequest)));
		if (e == sys::errc::success) {
			if (_ackBuffer != 0x06) {
				if (++_transmissionErrors < 5) {
					_currentState = State::SENDING_REQ_STATION;
					flushSocket();
					sendRequest(_getStationRequest, sizeof(_getStationRequest));
				} else {
					syslog(LOG_ERR, "Too many transmissions errors, aborting");
					stop();
				}
			} else {
				_currentState = State::WAITING_DATA_STATION;
				std::cerr << "Identification request acked by station" << std::endl;
				waitForData(asio::buffer(_coords));
			}
		}
		break;

	case State::WAITING_DATA_STATION:
		handleGenericErrors(e, State::SENDING_REQ_STATION,
			std::bind(&VantagePro2Connector::sendRequest, this,
				_getMeasureRequest, sizeof(_getMeasureRequest)));
		if (e == sys::errc::success) {
			if (!VantagePro2Message::validateCRC(_coords, sizeof(_coords))) {
				if (++_transmissionErrors < 5) {
					_currentState = State::SENDING_REQ_STATION;
					flushSocket();
					sendRequest(_getMeasureRequest, sizeof(_getMeasureRequest));
				} else {
					syslog(LOG_ERR, "Too many transmissions errors, aborting");
					stop();
				}
			} else {
				// From documentation, latitude, longitude and elevation are stored contiguously
				// in this order in the station's EEPROM
				_currentState = State::WAITING_NEXT_MEASURE_TICK;
				bool found = _db.getStationByCoords(_coords[2], _coords[0], _coords[1], _station);
				flushSocket();
				if (found) {
					std::cerr << "Received correct identification" << std::endl;
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
			waitForData(asio::buffer(&_ackBuffer, 1));
		}
		break;

	case State::WAITING_ACK_MEASURE:
		handleGenericErrors(e, State::SENDING_REQ_MEASURE,
			std::bind(&VantagePro2Connector::sendRequest, this,
				_getMeasureRequest, sizeof(_getMeasureRequest)));
		if (e == sys::errc::success) {
			if (_ackBuffer != 0x06) {
				if (++_transmissionErrors < 5) {
					_currentState = State::SENDING_REQ_MEASURE;
					flushSocket();
					sendRequest(_getMeasureRequest, sizeof(_getMeasureRequest));
				} else {
					syslog(LOG_ERR, "Too many transmissions errors, aborting");
					stop();
				}
			} else {
				_currentState = State::WAITING_DATA_MEASURE;
				std::cerr << "Station has acked measurement request" << std::endl;
				waitForData(_message.getBuffer());
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
					syslog(LOG_ERR, "Too many transmissions errors, aborting");
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
					syslog(LOG_ERR, "Couldn't store measurement");
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

	}
}

}
