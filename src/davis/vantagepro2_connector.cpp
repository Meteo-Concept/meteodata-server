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
#include <iomanip>
#include <memory>
#include <array>
#include <functional>
#include <iterator>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <systemd/sd-daemon.h>
#include <unistd.h>

#include <boost/system/error_code.hpp>
#include <boost/asio.hpp>
#include <boost/asio/basic_waitable_timer.hpp>

#include <cassandra.h>

#include "../connector.h"
#include "../time_offseter.h"
#include "vantagepro2_connector.h"
#include "vantagepro2_message.h"
#include "vantagepro2_archive_page.h"

namespace ip = boost::asio::ip;
namespace asio = boost::asio;
namespace sys = boost::system; //system() is a function, it cannot be redefined
//as a namespace
namespace chrono = std::chrono;

namespace meteodata {

using namespace std::placeholders;
using namespace date;

constexpr char VantagePro2Connector::_echoRequest[];
constexpr char VantagePro2Connector::_getStationRequest[];
constexpr char VantagePro2Connector::_getMeasureRequest[];
constexpr char VantagePro2Connector::_getArchiveRequest[];
constexpr char VantagePro2Connector::_getTimezoneRequest[];
constexpr char VantagePro2Connector::_settimeRequest[];
constexpr char VantagePro2Connector::_mainModeRequest[];
constexpr char VantagePro2Connector::_ack[];
constexpr char VantagePro2Connector::_nak[];
constexpr char VantagePro2Connector::_abort[];

VantagePro2Connector::VantagePro2Connector(boost::asio::io_service& ioService,
	DbConnectionObservations& db) :
	Connector(ioService, db),
	_timer(ioService),
	_setTimeTimer(ioService)
{
}

void VantagePro2Connector::start()
{
	boost::asio::socket_base::keep_alive keepalive(true);
	boost::asio::detail::socket_option::integer<SOL_TCP, TCP_KEEPIDLE> keepaliveIdleTime(30);
	boost::asio::detail::socket_option::integer<SOL_TCP, TCP_KEEPINTVL> keepaliveInterval(10);
	boost::asio::detail::socket_option::integer<SOL_TCP, TCP_KEEPCNT> keepaliveProbes(2);
	_sock.set_option(keepalive);
	_sock.set_option(keepaliveIdleTime);
	_sock.set_option(keepaliveInterval);
	_sock.set_option(keepaliveProbes);

	auto self(std::static_pointer_cast<VantagePro2Connector>(shared_from_this()));
	_currentState = State::STARTING;

	handleEvent(sys::errc::make_error_code(sys::errc::success));
}

std::shared_ptr<VantagePro2Connector::ArchiveRequestParams> VantagePro2Connector::buildArchiveRequestParams(const date::sys_seconds& time)
{
	auto buffer = std::make_shared<VantagePro2Connector::ArchiveRequestParams>();
	auto timeStation = _timeOffseter.convertToLocalTime(time);
	auto daypoint = date::floor<date::days>(timeStation);
	auto ymd = date::year_month_day(daypoint);   // calendar date
	auto tod = date::make_time(timeStation - daypoint); // Yields time_of_day type

	// Obtain individual components as integers
	auto y   = int(ymd.year());
	auto m   = unsigned(ymd.month());
	auto d   = unsigned(ymd.day());
	auto h   = tod.hours().count();
	auto min = tod.minutes().count();

	buffer->date = ((y - 2000) << 9) + (m << 5) + d;
	buffer->time = h * 100 + min;
	VantagePro2Message::computeCRC(buffer.get(), 6);

	return buffer;
}

std::shared_ptr<VantagePro2Connector::SettimeRequestParams> VantagePro2Connector::buildSettimeParams()
{
	auto buffer = std::make_shared<VantagePro2Connector::SettimeRequestParams>();

	auto nowLocal = date::floor<chrono::seconds>(_timeOffseter.convertToLocalTime(chrono::system_clock::now()));
	auto daypoint = date::floor<date::days>(nowLocal);
	auto ymd = date::year_month_day(daypoint);   // calendar date
	auto tod = date::make_time(nowLocal - daypoint); // Yields time_of_day type

	buffer->seconds = tod.seconds().count();
	buffer->minutes = tod.minutes().count();
	buffer->hours   = tod.hours().count();
	buffer->day     = unsigned(ymd.day());
	buffer->month   = unsigned(ymd.month());
	buffer->year    = int(ymd.year()) - 1900;
	VantagePro2Message::computeCRC(buffer.get(), sizeof(VantagePro2Connector::SettimeRequestParams));

	return buffer;
}

void VantagePro2Connector::waitForNextMeasure()
{
	auto self(std::static_pointer_cast<VantagePro2Connector>(shared_from_this()));

	// reset the counters
	_timeouts = 0;
	_transmissionErrors = 0;

	 auto tp = chrono::minutes(_pollingPeriod) -
	       (chrono::system_clock::now().time_since_epoch() % chrono::minutes(_pollingPeriod));
	std::cout << SD_INFO
		  << "Next measurement will be taken in "
		  << chrono::duration_cast<chrono::minutes>(tp).count() << "min "
		  << chrono::duration_cast<chrono::seconds>(tp % chrono::minutes(1)).count() << "s " << std::endl;
	_timer.expires_from_now(tp);
	_timer.async_wait(std::bind(&VantagePro2Connector::checkDeadline, self, _1));
}


void VantagePro2Connector::checkDeadline(const sys::error_code& e)
{
	/* if the timer has been cancelled, then bail out, we have nothing more
	 * to do here. It's our original caller's responsability to restart us
	 * if needs be */
	if (e == sys::errc::operation_canceled)
		return;

	// verify that the timeout is not spurious
	if (_timer.expires_at() <= chrono::steady_clock::now()) {
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

void VantagePro2Connector::handleSetTimeDeadline(const sys::error_code& e)
{
	std::cout << SD_DEBUG
		  << "SetTime time deadline handler hit for station " << _stationName
		  << ": " << e.value() << ": " << e.message()
		  << std::endl;
	if (e == sys::errc::operation_canceled)
		return;

	// verify that the timeout is not spurious
	if (_setTimeTimer.expires_at() <= chrono::steady_clock::now()) {
		std::cout << SD_DEBUG << "Timed out! We have to reset the station clock ASAP" << std::endl;
		/* This timer does not interrupt the normal handling of events
		 * but signals with a flag that the time should be set */
		_setTimeRequested = true;
	} else {
		/* spurious handler call, restart the timer without changing the
		 * deadline */
		auto self(std::static_pointer_cast<VantagePro2Connector>(shared_from_this()));
		_setTimeTimer.async_wait(std::bind(&VantagePro2Connector::checkDeadline, self, _1));
	}
}

void VantagePro2Connector::stop()
{
	/* hijack the state machine to force STOPPED state */
	_currentState = State::STOPPED;
	_stopped = true;
	/* cancel all asynchronous event handlers */
	_timer.cancel();
	_setTimeTimer.cancel();
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

template<typename T>
void VantagePro2Connector::sendBuffer(const std::shared_ptr<T>& buffer, int reqsize)
{
	auto self(std::static_pointer_cast<VantagePro2Connector>(shared_from_this()));
	_timer.expires_from_now(chrono::seconds(6));
	_timer.async_wait(std::bind(&VantagePro2Connector::checkDeadline, self, _1));

	// passing buffer to the lambda keeps the buffer alive
	async_write(_sock, asio::buffer(buffer.get(), reqsize),
		[this,self,buffer](const sys::error_code& ec, std::size_t) {
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

void VantagePro2Connector::recvOk()
{
	auto self(std::static_pointer_cast<VantagePro2Connector>(shared_from_this()));
	_timer.expires_from_now(chrono::seconds(6));
	_timer.async_wait(std::bind(&VantagePro2Connector::checkDeadline, self, _1));
	async_read_until(_sock, _discardBuffer, "OK\n\r",
		[this,self](const sys::error_code& ec, std::size_t n) {
			if (ec == sys::errc::operation_canceled)
				return;
			_timer.cancel();
			_discardBuffer.consume(n);
			handleEvent(ec);
		}
	);
}

void VantagePro2Connector::sendAck()
{
	sendRequest(_ack, 1);
}

void VantagePro2Connector::sendNak()
{
	sendRequest(_nak, 1);
}

void VantagePro2Connector::sendAbort()
{
	sendRequest(_abort, 1);
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

template <typename Restarter>
void VantagePro2Connector::flushSocketAndRetry(State restartState, Restarter restart)
{
	auto self(std::static_pointer_cast<VantagePro2Connector>(shared_from_this()));
	// wait before flushing in order not to leave garbage behind
	_timer.expires_from_now(chrono::seconds(10));
	_timer.async_wait(
		[restartState,restart,this,self](const sys::error_code& ec) {
			if (ec == sys::errc::operation_canceled)
				return;
			if (_sock.available() > 0) {
				auto bufs = _discardBuffer.prepare(512);
				std::size_t bytes = _sock.receive(bufs);
				_discardBuffer.commit(bytes);
				std::cout << SD_DEBUG << "station " << _stationName << ": Cleared " << bytes << " bytes" << std::endl;
				_discardBuffer.consume(_discardBuffer.size());
			}
			_currentState = restartState;
			restart();
		}
	);
}

template <typename Restarter>
void VantagePro2Connector::handleGenericErrors(const sys::error_code& e, State restartState, Restarter restart)
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
		if (++_timeouts < 5) {
			_currentState = restartState;
			flushSocketAndRetry(restartState, restart);
		} else {
			std::cerr << SD_ERR << "Directly connected station " << _stationName << ": too many timeouts, aborting" << std::endl;
			stop();
		}
	} else { /* TCP reset by peer, etc. */
		std::cerr << SD_ERR << "Directly connected station " << _stationName << ": unknown error: " << e.message() << std::endl;
		stop();
	}
}

void VantagePro2Connector::handleEvent(const sys::error_code& e)
{
	switch (_currentState) {
	case State::STARTING:
		_currentState = State::SENDING_WAKE_UP_STATION;
		std::cout << SD_NOTICE << "A new station is connected" << std::endl;
		sendRequest(_echoRequest, std::strlen(_echoRequest));
		break;

	case State::WAITING_NEXT_MEASURE_TICK:
		if (e == sys::errc::timed_out) {
			_currentState = State::SENDING_WAKE_UP_ARCHIVE;
			std::cout << SD_INFO << "Time to wake up! We need a new measurement" << std::endl;
			sendRequest(_echoRequest, std::strlen(_echoRequest));
		} else {
			waitForNextMeasure();
		}
		break;

	case State::SENDING_WAKE_UP_STATION:
		handleGenericErrors(e, State::SENDING_WAKE_UP_STATION,
			std::bind(&VantagePro2Connector::sendRequest, this,
				_echoRequest, std::strlen(_echoRequest)));
		if (e == sys::errc::success) {
			_currentState = State::WAITING_ECHO_STATION;
			std::cout << SD_DEBUG << "Sent wake up" << std::endl;
			recvWakeUp();
		}
		break;

	case State::WAITING_ECHO_STATION:
		handleGenericErrors(e, State::SENDING_WAKE_UP_STATION,
			std::bind(&VantagePro2Connector::sendRequest, this,
				_echoRequest, std::strlen(_echoRequest)));
		if (e == sys::errc::success) {
			_currentState = State::SENDING_REQ_STATION;
			std::cout << SD_DEBUG << "Station has woken up" << std::endl;
			sendRequest(_getStationRequest, std::strlen(_getStationRequest));
		}
		break;

	case State::SENDING_REQ_STATION:
		handleGenericErrors(e, State::SENDING_REQ_STATION,
			std::bind(&VantagePro2Connector::sendRequest, this,
				_getStationRequest, std::strlen(_getStationRequest)));
		if (e == sys::errc::success) {
			_currentState = State::WAITING_ACK_STATION;
			std::cout << SD_DEBUG << "Sent identification request" << std::endl;
			recvAck();
		}
		break;

	case State::WAITING_ACK_STATION:
		handleGenericErrors(e, State::SENDING_REQ_STATION,
			std::bind(&VantagePro2Connector::sendRequest, this,
				_getStationRequest, std::strlen(_getStationRequest)));
		if (e == sys::errc::success) {
			if (_ackBuffer != 0x06) {
				std::cerr << SD_WARNING
					  << "Directly connected station " << _stationName
					  << ": was waiting for acknowledgement, got " << _ackBuffer
					  << std::endl;
				if (++_transmissionErrors < 5) {
					flushSocketAndRetry(State::SENDING_REQ_STATION,
						std::bind(&VantagePro2Connector::sendRequest, this,
							_getStationRequest, std::strlen(_getStationRequest)));
				} else {
					std::cerr << SD_ERR << "Directly connected station " << _stationName
						  << " : Cannot get the station to acknowledge the identification request"
						  << std::endl;
					stop();
				}
			} else {
				_currentState = State::WAITING_DATA_STATION;
				std::cout << SD_DEBUG << "Identification request acked by station" << std::endl;
				recvData(asio::buffer(_coords));
			}
		}
		break;

	case State::WAITING_DATA_STATION:
		handleGenericErrors(e, State::SENDING_REQ_STATION,
			std::bind(&VantagePro2Connector::sendRequest, this,
				_getStationRequest, std::strlen(_getStationRequest)));
		if (e == sys::errc::success) {
			if (!VantagePro2Message::validateCRC(_coords, sizeof(_coords))) {
				if (++_transmissionErrors < 5) {
					flushSocketAndRetry(State::SENDING_REQ_STATION,
						std::bind(&VantagePro2Connector::sendRequest, this,
							_getStationRequest, std::strlen(_getStationRequest)));
				} else {
					std::cerr << SD_ERR << "Directly connected station " << _stationName
						  << " : Too many transmissions errors on station identification CRC validation, aborting"
						  << std::endl;
					stop();
				}
			} else {
				// From documentation, latitude, longitude and elevation are stored contiguously
				// in this order in the station's EEPROM
				time_t lastArchiveDownloadTime;
				time_t lastDataInsertionTime;
				bool found = _db.getStationByCoords(_coords[2], _coords[0], _coords[1], _station, _stationName, _pollingPeriod, lastArchiveDownloadTime, lastDataInsertionTime);
				_timeOffseter.setLatitude(_coords[0]);
				_timeOffseter.setLongitude(_coords[1]);
				_timeOffseter.setElevation(_coords[2]);
				_timeOffseter.setMeasureStep(_pollingPeriod);
				_lastArchive = date::sys_seconds(chrono::seconds(lastArchiveDownloadTime));
				_lastData    = date::sys_seconds(chrono::seconds(lastDataInsertionTime));
				if (found) {
					std::cout << SD_INFO << "station " << _stationName << " is connected" << std::endl;
					std::cout << SD_DEBUG << "Now making sure station " << _stationName << " is not stuck in setup mode" << std::endl;
					_currentState = State::SENDING_REQ_MAIN_MODE;
					sendRequest(_mainModeRequest, std::strlen(_mainModeRequest));
				} else {
					std::cerr << SD_ERR << "Unknown station (" << _coords[0] << ", " << _coords[1] << ", " << _coords[2] << ") ! Aborting" << std::endl;
					stop();
				}
			}
		}
		break;

	case State::SENDING_REQ_MAIN_MODE:
		handleGenericErrors(e, State::SENDING_REQ_MAIN_MODE,
			std::bind(&VantagePro2Connector::sendRequest, this,
				_mainModeRequest, std::strlen(_mainModeRequest)));
		if (e == sys::errc::success) {
			_currentState = State::WAITING_ACK_MAIN_MODE;
			std::cout << SD_DEBUG << "Sent switch to main mode request" << std::endl;
			recvOk();
		}
		break;

	case State::WAITING_ACK_MAIN_MODE:
		handleGenericErrors(e, State::SENDING_REQ_MAIN_MODE,
			std::bind(&VantagePro2Connector::sendRequest, this,
				_mainModeRequest, std::strlen(_mainModeRequest)));
		if (e == sys::errc::success) {
			_currentState = State::SENDING_REQ_TIMEZONE;
			std::cout << SD_DEBUG << "Now fetching timezone information for station " << _stationName << std::endl;
			sendRequest(_getTimezoneRequest, std::strlen(_getTimezoneRequest));
		} else {
			std::cerr << SD_ERR
				  << "Cannot get the directly connected station " << _stationName
				  << " to acknowledge the switch to main mode command ! Aborting"
				  << std::endl;
			stop();
		}
		break;

	case State::SENDING_REQ_TIMEZONE:
		handleGenericErrors(e, State::SENDING_REQ_TIMEZONE,
			std::bind(&VantagePro2Connector::sendRequest, this,
				_getTimezoneRequest, std::strlen(_getTimezoneRequest)));
		if (e == sys::errc::success) {
			_currentState = State::WAITING_ACK_TIMEZONE;
			std::cout << SD_DEBUG << "Sent identification request" << std::endl;
			recvAck();
		}
		break;

	case State::WAITING_ACK_TIMEZONE:
		handleGenericErrors(e, State::SENDING_REQ_TIMEZONE,
			std::bind(&VantagePro2Connector::sendRequest, this,
				_getTimezoneRequest, std::strlen(_getTimezoneRequest)));
		if (e == sys::errc::success) {
			if (_ackBuffer != 0x06) {
				std::cerr << SD_WARNING << "Directly connected station " << _stationName
					  << " : Was waiting for acknowledgement, got " << _ackBuffer
					  << std::endl;
				if (++_transmissionErrors < 5) {
					flushSocketAndRetry(State::SENDING_REQ_TIMEZONE,
						std::bind(&VantagePro2Connector::sendRequest, this,
							_getTimezoneRequest, std::strlen(_getTimezoneRequest)));
				} else {
					std::cerr << SD_ERR << "Directly connected station " << _stationName
						  << ": Cannot get the station to acknowledge the timezone request"
						  << std::endl;
					stop();
				}
			} else {
				_currentState = State::WAITING_DATA_TIMEZONE;
				std::cout << SD_DEBUG << "Timezone request acked by station" << std::endl;
				recvData(asio::buffer(&_timezoneBuffer,sizeof(_timezoneBuffer)));
			}
		}
		break;

	case State::WAITING_DATA_TIMEZONE:
		handleGenericErrors(e, State::SENDING_REQ_TIMEZONE,
			std::bind(&VantagePro2Connector::sendRequest, this,
				_getTimezoneRequest, std::strlen(_getTimezoneRequest)));
		if (e == sys::errc::success) {
			if (!VantagePro2Message::validateCRC(_coords, sizeof(_coords))) {
				if (++_transmissionErrors < 5) {
					flushSocketAndRetry(State::SENDING_REQ_TIMEZONE,
						std::bind(&VantagePro2Connector::sendRequest, this,
							_getTimezoneRequest, std::strlen(_getTimezoneRequest)));
				} else {
					std::cerr << SD_ERR << "Directly connected station " << _stationName
						  << ": Too many transmissions errors on station identification CRC validation, aborting"
						  << std::endl;
					stop();
				}
			} else {
				_timeOffseter.prepare(_timezoneBuffer);
				chrono::system_clock::time_point now = chrono::system_clock::now();
				std::cout << SD_DEBUG
					  << "Last data received from station " << _stationName << " dates back from "
					  << _lastData << std::endl;
				if ((now - _lastData) > chrono::minutes(_pollingPeriod)) {
					std::cout << SD_INFO << "Directly connected station " << _stationName
						  << " has been disconnected for too long, retrieving the archives..."
						  << std::endl;
					std::cout << SD_DEBUG
						  << "Retrieving archived data for " << _stationName
						  << std::endl;
					_archivePage.prepare(_lastArchive, &_timeOffseter);
					_currentState = State::SENDING_WAKE_UP_ARCHIVE;
					sendRequest(_echoRequest, std::strlen(_echoRequest));
				} else {
					std::cout << SD_INFO
						  << "Station " << _stationName << "'s clock has to be set"
						  << std::endl;
					_currentState = State::SENDING_WAKE_UP_SETTIME;
					sendRequest(_echoRequest, std::strlen(_echoRequest));
				}
			}
		}
		break;

	case State::SENDING_WAKE_UP_ARCHIVE:
		handleGenericErrors(e, State::SENDING_WAKE_UP_ARCHIVE,
			std::bind(&VantagePro2Connector::sendRequest, this,
				_echoRequest, std::strlen(_echoRequest)));
		if (e == sys::errc::success) {
			_currentState = State::WAITING_ECHO_ARCHIVE;
			std::cout << SD_DEBUG << "Waking up station for archive request" << std::endl;
			recvWakeUp();
		}
		break;

	case State::WAITING_ECHO_ARCHIVE:
		handleGenericErrors(e, State::SENDING_WAKE_UP_ARCHIVE,
			std::bind(&VantagePro2Connector::sendRequest, this,
				_echoRequest, std::strlen(_echoRequest)));
		if (e == sys::errc::success) {
			_currentState = State::SENDING_REQ_ARCHIVE;
			std::cout << SD_DEBUG << "Station is woken up, ready to send archives" << std::endl;
			sendRequest(_getArchiveRequest, std::strlen(_getArchiveRequest));
		}
		break;

	case State::SENDING_REQ_ARCHIVE:
		handleGenericErrors(e, State::SENDING_REQ_ARCHIVE,
			std::bind(&VantagePro2Connector::sendRequest, this,
				_getArchiveRequest, std::strlen(_getArchiveRequest)));
		if (e == sys::errc::success) {
			_currentState = State::WAITING_ACK_ARCHIVE;
			std::cout << SD_DEBUG << "Sent archive request" << std::endl;
			recvAck();
		}
		break;

	case State::WAITING_ACK_ARCHIVE:
		handleGenericErrors(e, State::SENDING_REQ_ARCHIVE,
			std::bind(&VantagePro2Connector::sendRequest, this,
				_getArchiveRequest, std::strlen(_getArchiveRequest)));
		if (e == sys::errc::success) {
			if (_ackBuffer != 0x06) {
				std::cout << SD_WARNING << "Directly connected station " << _stationName
					  << " : Was waiting for acknowledgement, got " << _ackBuffer
					  << std::endl;
				if (++_transmissionErrors < 5) {
					flushSocketAndRetry(State::SENDING_REQ_ARCHIVE,
						std::bind(&VantagePro2Connector::sendRequest, this,
							_getArchiveRequest, std::strlen(_getArchiveRequest)));
				} else {
					std::cerr << SD_ERR << "Directly connected station " << _stationName
						  << " : Cannot get the station to acknowledge the archive request"
						  << std::endl;
					stop();
				}
			} else {
				_currentState = State::SENDING_ARCHIVE_PARAMS;
				std::cout << SD_DEBUG << "Archive download request acked by station" << std::endl;
				auto buffer = buildArchiveRequestParams(_lastArchive);
				sendBuffer(std::move(buffer), sizeof(ArchiveRequestParams));
			}
		}
		break;

	case State::SENDING_ARCHIVE_PARAMS:
		// We cannot retry anything here, we are in the middle of a request, just bail out
		if (e != sys::errc::success) {
			std::cerr << SD_ERR << "Directly connected station " << _stationName
				  << " : Connection to the station lost while requesting archive"
				  << std::endl;
			stop();
		} else {
			_currentState = State::WAITING_ACK_ARCHIVE_PARAMS;
			std::cout << SD_DEBUG << "Sent archive request parameters" << std::endl;
			recvAck();
		}
		break;

	case State::WAITING_ACK_ARCHIVE_PARAMS:
		// We cannot retry anything here, we are in the middle of a request, just bail out
		if (e != sys::errc::success) {
			std::cerr << SD_ERR << "Directly connected station " << _stationName
				  << " : Connection to the station lost while requesting archive"
				  << std::endl;
			stop();
		} else {
			if (_ackBuffer != 0x06) {
				std::cout << SD_DEBUG << "Directly connected station " << _stationName
					  << " : Was waiting for acknowledgement, got " << _ackBuffer << " (NAK?)"
					  << std::endl;
				std::cerr << SD_ERR << "Directly connected station " << _stationName
					  << " : Cannot get the station to acknowledge the archive request"
					  << std::endl;
				stop();
			} else {
				_currentState = State::WAITING_ARCHIVE_NB_PAGES;
				std::cout << SD_DEBUG << "Archive dowload parameters acked by station" << std::endl;
				recvData(asio::buffer(&_archiveSize,sizeof(_archiveSize)));
			}
		}
		break;

	case State::WAITING_ARCHIVE_NB_PAGES:
		// We cannot retry anything here, we are in the middle of a request, just bail out
		if (e != sys::errc::success) {
			std::cerr << SD_ERR << "Directly connected station " << _stationName
				  << " : Connection to the station lost while requesting archive"
				  << std::endl;
			std::cout << SD_DEBUG << "Directly connected station " << _stationName
				  << " has not sent the archive size"
				  << std::endl;
			stop();
		} else {
			if (VantagePro2Message::validateCRC(&_archiveSize,sizeof(_archiveSize))) {
				_currentState = State::SENDING_ACK_ARCHIVE_DOWNLOAD;
				std::cout << SD_DEBUG << "Archive size has a valid CRC, continueing" << std::endl;

				std::cout << SD_DEBUG << "Parameters received from the station: " << std::hex << std::setfill('0') << std::setw(2);
				uint8_t *data = reinterpret_cast<uint8_t*>(&_archiveSize);
				std::copy(data, data+6, std::ostream_iterator<unsigned int>(std::cout,"|"));
				std::cout << std::dec << std::endl;
				std::cout << SD_DEBUG << "We will receive " << _archiveSize.pagesLeft << " pages, first record at " << _archiveSize.index << std::endl;
				sendAck();
			} else {
				_currentState = State::SENDING_ABORT_ARCHIVE_DOWNLOAD;
				std::cerr << SD_WARNING << "Archive size does not have a valid CRC, aborting for now but will retry" << std::endl;
				sendAbort();
			}
		}
		break;

	case State::SENDING_ABORT_ARCHIVE_DOWNLOAD:
		// We cannot retry anything here, we are in the middle of a request, just bail out
		if (e != sys::errc::success) {
			std::cerr << SD_ERR << "Directly connected station " << _stationName
				  << " : connection to the station lost while requesting archive"
				  << std::endl;
			std::cerr << SD_ERR << "Failed to abort the download, bailing out"
				  << std::endl;
			stop();
		} else {
			_currentState = State::WAITING_NEXT_MEASURE_TICK;
			std::cerr << SD_ERR << "Directly connected station " << _stationName
				  << " : failed to receive correct archive download parameters, will retry at next download"
				  << std::endl;
			waitForNextMeasure();
		}
		break;

	case State::WAITING_ARCHIVE_PAGE:
		// We cannot retry anything here, we are in the middle of a request, just bail out
		if (e != sys::errc::success) {
			std::cerr << SD_ERR << "Directly connected station " << _stationName
				  << " : connection to the station lost while requesting archive"
				  << std::endl;
			stop();
		} else {
			if (_archivePage.isValid()) {
				bool ret = _archivePage.store(_db, _station);
				if (ret) {
					std::cout << SD_DEBUG << "Archive data page stored, updating the archive download time" << std::endl;

					time_t lastArchiveDownloadTime = chrono::system_clock::to_time_t(_archivePage.lastArchiveRecordDateTime());
					ret = _db.updateLastArchiveDownloadTime(_station, lastArchiveDownloadTime);
					if (!ret)
						std::cerr << SD_ERR << "Directly connected station " << _stationName
							  << " : couldn't update last archive download time"
							  << std::endl;
				} else {
					std::cerr << SD_ERR << "Directly connected station " << _stationName
						  << " : couldn't store archive"
						  << std::endl;
					stop();
				}
				_archiveSize.pagesLeft--;
				_currentState = State::SENDING_ARCHIVE_PAGE_ANSWER;
				std::cout << SD_DEBUG << "Received correct archive data" << std::endl;
				sendAck();
			} else {
				_currentState = State::SENDING_ARCHIVE_PAGE_ANSWER;
				_transmissionErrors++;
				if (_transmissionErrors > 100) {
					std::cerr << SD_ERR << "Directly connected station " << _stationName
						  << " : received too many incorrect archive data, bailing out"
						  << std::endl;
					stop();
				}
				std::cerr << SD_WARNING << "Directly connected station " << _stationName
					  << " : received incorrect archive data, retrying"
					  << std::endl;
				sendNak();
			}
		}
		break;

	case State::SENDING_ACK_ARCHIVE_DOWNLOAD:
	case State::SENDING_ARCHIVE_PAGE_ANSWER:
		// We cannot retry anything here, we are in the middle of a request, just bail out
		if (e != sys::errc::success) {
			std::cerr << SD_ERR << "Directly connected station " << _stationName
				  << " : connection to the station lost while acknowledgeing archive page"
				  << std::endl;
			stop();
		} else {
			std::cout << SD_DEBUG << "Sent answer to station" << std::endl;
			if (_archiveSize.pagesLeft) {
				_currentState = State::WAITING_ARCHIVE_PAGE;
				std::cout << SD_DEBUG << _archiveSize.pagesLeft << " pages left to download" << std::endl;
				recvData(_archivePage.getBuffer());
			} else {
				std::cout << SD_DEBUG << "Archive data stored" << std::endl;
				if (_setTimeRequested) {
					std::cout << SD_DEBUG << "Station " << _stationName << "'s clock has to be set" << std::endl;
					_currentState = State::SENDING_WAKE_UP_SETTIME;
					sendRequest(_echoRequest, std::strlen(_echoRequest));
				} else {
					std::cout << SD_DEBUG << "Now sleeping until next measurement" << std::endl;
					_currentState = State::WAITING_NEXT_MEASURE_TICK;
					waitForNextMeasure();
				}
			}
		}
		break;

	case State::SENDING_WAKE_UP_SETTIME:
		handleGenericErrors(e, State::SENDING_WAKE_UP_SETTIME,
			std::bind(&VantagePro2Connector::sendRequest, this,
				_echoRequest, std::strlen(_echoRequest)));
		if (e == sys::errc::success) {
			_currentState = State::WAITING_ECHO_SETTIME;
			std::cout << SD_DEBUG << "Waking up station for clock setting" << std::endl;
			recvWakeUp();
		}
		break;

	case State::WAITING_ECHO_SETTIME:
		handleGenericErrors(e, State::SENDING_WAKE_UP_SETTIME,
			std::bind(&VantagePro2Connector::sendRequest, this,
				_echoRequest, std::strlen(_echoRequest)));
		if (e == sys::errc::success) {
			_currentState = State::SENDING_SETTIME;
			std::cout << SD_DEBUG << "Station is woken up, ready to receive clock setting" << std::endl;
			sendRequest(_settimeRequest, std::strlen(_settimeRequest));
		}
		break;

	case State::SENDING_SETTIME:
		handleGenericErrors(e, State::SENDING_SETTIME,
			std::bind(&VantagePro2Connector::sendRequest, this,
				_settimeRequest, std::strlen(_settimeRequest)));
		if (e == sys::errc::success) {
			_currentState = State::WAITING_ACK_SETTIME;
			std::cout << SD_DEBUG << "Sent settime request" << std::endl;
			recvAck();
		}
		break;

	case State::WAITING_ACK_SETTIME:
		handleGenericErrors(e, State::SENDING_SETTIME,
			std::bind(&VantagePro2Connector::sendRequest, this,
				_settimeRequest, std::strlen(_settimeRequest)));
		if (e == sys::errc::success) {
			if (_ackBuffer != 0x06) {
				std::cout << SD_DEBUG << "Directly connected station " << _stationName
					  << " : was waiting for acknowledgement, got " << _ackBuffer
					  << std::endl;
				if (++_transmissionErrors < 5) {
					flushSocketAndRetry(State::SENDING_SETTIME,
						std::bind(&VantagePro2Connector::sendRequest, this,
							_settimeRequest, std::strlen(_settimeRequest)));
				} else {
					std::cerr << SD_ERR << "Directly connected station " << _stationName
						  << " : Cannot get the station to acknowledge the archive request"
						  << std::endl;
					stop();
				}
			} else {
				_currentState = State::SENDING_SETTIME_PARAMS;
				std::cout << SD_DEBUG << "Archive download request acked by station" << std::endl;
				auto buffer = buildSettimeParams();
				sendBuffer(std::move(buffer), sizeof(SettimeRequestParams));
			}
		}
		break;

	case State::SENDING_SETTIME_PARAMS:
		// We cannot retry anything here, we are in the middle of a request, just bail out
		if (e != sys::errc::success) {
			std::cerr << SD_ERR << "Directly connected station " << _stationName
				  << " : Connection to the station lost while setting clock"
				  << std::endl;
			stop();
		} else {
			_currentState = State::WAITING_ACK_TIME_SET;
			std::cout << SD_DEBUG << "Sent time parameters" << std::endl;
			recvAck();
		}
		break;

	case State::WAITING_ACK_TIME_SET:
		// We cannot retry anything here, we are in the middle of a request, just bail out
		if (e != sys::errc::success) {
			std::cerr << SD_ERR << "Directly connected station " << _stationName
				  << " : connection to the station lost while setting clock"
				  << std::endl;
			std::cout << SD_DEBUG << "Station " << _stationName << " has not acked the clock setting" << std::endl;
		} else if (_ackBuffer != 0x06) {
				std::cout << SD_DEBUG << "Directly connected station " << _stationName
					  << " : was waiting for acknowledgement, got " << _ackBuffer
					  << std::endl;
		}

		std::cout << SD_DEBUG << "Time set for station " << _stationName << std::endl;
		_setTimeRequested = false;
		_setTimeTimer.expires_from_now(chrono::hours(1));
		{
			auto self(std::static_pointer_cast<VantagePro2Connector>(shared_from_this()));
			_setTimeTimer.async_wait(std::bind(&VantagePro2Connector::handleSetTimeDeadline, self, _1));
			_currentState = State::WAITING_NEXT_MEASURE_TICK;
			waitForNextMeasure();
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
