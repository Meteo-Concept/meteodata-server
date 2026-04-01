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
#include <iterator>
#include <chrono>
#include <cstring>
#include <systemd/sd-daemon.h>
#include <systemd/sd-journal.h>

#include <boost/system/error_code.hpp>
#include <boost/asio.hpp>
#include <boost/asio/basic_waitable_timer.hpp>

#include "cassandra_utils.h"
#include "connector.h"
#include "time_offseter.h"
#include "async_job_publisher.h"
#include "davis/vantagepro2_connector.h"
#include "davis/vantagepro2_message.h"
#include "davis/vantagepro2_archive_page.h"

namespace ip = boost::asio::ip;
namespace asio = boost::asio;
namespace sys = boost::system;
namespace chrono = std::chrono;

namespace meteodata
{

using namespace std::placeholders;
using namespace date;

VantagePro2Connector::VantagePro2Connector(boost::asio::io_context& ioContext,
	DbConnectionObservations& db,
	const std::shared_ptr<AsyncJobPublisher>& jobPublisher) :
		Connector{ioContext, db},
		_sock{ioContext},
		_timer{ioContext},
		_setTimeTimer{ioContext},
		_jobPublisher{jobPublisher}
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

	_currentState = State::STARTING;

	_status.shortStatus = "Starting";
	_status.activeSince = date::floor<chrono::seconds>(chrono::system_clock::now());
	_status.lastReloaded = _status.activeSince;
	_status.nbDownloads = 0;

	handleEvent(sys::errc::make_error_code(sys::errc::success));
}

std::shared_ptr<VantagePro2Connector::ArchiveRequestParams>
VantagePro2Connector::buildArchiveRequestParams(const date::sys_seconds& time)
{
	auto buffer = std::make_shared<VantagePro2Connector::ArchiveRequestParams>();
	auto timeStation = _timeOffseter.convertToLocalTime(time);
	auto daypoint = date::floor<date::days>(timeStation);
	auto ymd = date::year_month_day(daypoint);   // calendar date
	auto tod = date::make_time(timeStation - daypoint); // Yields time_of_day type

	// Obtain individual components as integers
	auto y = int(ymd.year());
	auto m = unsigned(ymd.month());
	auto d = unsigned(ymd.day());
	auto h = tod.hours().count();
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
	buffer->hours = tod.hours().count();
	buffer->day = unsigned(ymd.day());
	buffer->month = unsigned(ymd.month());
	buffer->year = int(ymd.year()) - 1900;
	VantagePro2Message::computeCRC(buffer.get(), sizeof(VantagePro2Connector::SettimeRequestParams));

	return buffer;
}

void VantagePro2Connector::waitForNextMeasure()
{
	auto self(std::static_pointer_cast<VantagePro2Connector>(shared_from_this()));

	// reset the counters
	_timeouts = 0;
	_transmissionErrors = 0;

	auto now = chrono::system_clock::now();
	auto tp = chrono::minutes(_pollingPeriod) -
		(now.time_since_epoch() % chrono::minutes(_pollingPeriod));
	sd_journal_send("MESSAGE=Next measurement scheduled will be taken in %dmin %02ds",
			chrono::duration_cast<chrono::minutes>(tp).count(),
			chrono::duration_cast<chrono::seconds>(tp % chrono::minutes(1)).count(),
			"PRIORITY=%i", LOG_INFO,
			"STATION=%s", _stationUuid,
			"CATEGORY=schedule",
			"CONNECTOR_TYPE=vp2_directconnect",
			NULL);
	_timer.expires_after(tp);
	_status.nextDownload = date::floor<chrono::seconds>(now + tp);
	_status.shortStatus = "Waiting for the next measure";
	_timer.async_wait([this, self](const sys::error_code& e) { checkDeadline(e); });
}


void VantagePro2Connector::checkDeadline(const sys::error_code& e)
{
	/* if the timer has been cancelled, then give up, we have nothing more
	 * to do here. It's our original caller's responsability to restart us
	 * if needs be */
	if (e == sys::errc::operation_canceled)
		return;

	// verify that the timeout is not spurious
	if (_timer.expires_at() <= chrono::steady_clock::now()) {
		_timeouts++;
		// The client might leave us before we reach this point
		if (_sock.is_open())
			_sock.cancel();
		handleEvent(sys::errc::make_error_code(sys::errc::timed_out));
	} else {
		/* spurious handler call, restart the timer without changing the
		 * deadline */
		auto self(std::static_pointer_cast<VantagePro2Connector>(shared_from_this()));
		_timer.async_wait([this, self](const sys::error_code& e) { checkDeadline(e); });
	}
}

void VantagePro2Connector::handleSetTimeDeadline(const sys::error_code& e)
{
	if (e == sys::errc::operation_canceled)
		return;

	// verify that the timeout is not spurious
	if (_setTimeTimer.expires_at() <= chrono::steady_clock::now()) {
		sd_journal_send("MESSAGE=Station clock due to be set ASAP",
			"PRIORITY=%i", LOG_DEBUG,
			"STATION=%s", _stationUuid,
			"CATEGORY=source",
			"CONNECTOR_TYPE=vp2_directconnect",
			NULL);
		/* This timer does not interrupt the normal handling of events
		 * but signals with a flag that the time should be set */
		_setTimeRequested = true;
	} else {
		/* spurious handler call, restart the timer without changing the
		 * deadline */
		auto self(std::static_pointer_cast<VantagePro2Connector>(shared_from_this()));
		_setTimeTimer.async_wait([this, self](const sys::error_code& e) { handleSetTimeDeadline(e); });
	}
}

void VantagePro2Connector::stop()
{
	/* hijack the state machine to force STOPPED state */
	_currentState = State::STOPPED;
	/* cancel all asynchronous event handlers */
	_timer.cancel();
	_setTimeTimer.cancel();
	_status.shortStatus = "Stopped";
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

void VantagePro2Connector::reload()
{
	// reset the state machine, disregarding what was going on
	_timer.cancel();
	_setTimeTimer.cancel();
	flushSocketAndRetry(State::SENDING_WAKE_UP_STATION, [this]() { sendRequest(_echoRequest, sizeof(_echoRequest) - 1); });
	_status.lastReloaded = date::floor<chrono::seconds>(chrono::system_clock::now());
}

void VantagePro2Connector::sendRequest(const char* req, int reqsize)
{
	auto self(std::static_pointer_cast<VantagePro2Connector>(shared_from_this()));
	countdown(chrono::seconds(6));
	async_write(_sock, asio::buffer(req, reqsize), [this, self](const sys::error_code& ec, std::size_t) {
		if (ec == sys::errc::operation_canceled)
			return;
		_timer.cancel();
		handleEvent(ec);
	});
}

template<typename T>
void VantagePro2Connector::sendBuffer(const std::shared_ptr<T>& buffer, int reqsize)
{
	auto self(std::static_pointer_cast<VantagePro2Connector>(shared_from_this()));
	countdown(chrono::seconds(6));

	// passing buffer to the lambda keeps the buffer alive
	async_write(_sock, asio::buffer(buffer.get(), reqsize),
				[this, self, buffer](const sys::error_code& ec, std::size_t) {
					if (ec == sys::errc::operation_canceled)
						return;
					_timer.cancel();
					handleEvent(ec);
				});
}

void VantagePro2Connector::recvWakeUp()
{
	auto self(std::static_pointer_cast<VantagePro2Connector>(shared_from_this()));
	countdown(chrono::seconds(2));
	async_read_until(_sock, _discardBuffer, "\n\r", [this, self](const sys::error_code& ec, std::size_t n) {
		if (ec == sys::errc::operation_canceled)
			return;
		_timer.cancel();
		_discardBuffer.consume(n);
		handleEvent(ec);
	});
}

void VantagePro2Connector::recvOk()
{
	auto self(std::static_pointer_cast<VantagePro2Connector>(shared_from_this()));
	countdown(chrono::seconds(6));
	async_read_until(_sock, _discardBuffer, "OK\n\r", [this, self](const sys::error_code& ec, std::size_t n) {
		if (ec == sys::errc::operation_canceled)
			return;
		_timer.cancel();
		_discardBuffer.consume(n);
		handleEvent(ec);
	});
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
	countdown(chrono::seconds(6));
	async_read(_sock, asio::buffer(&_ackBuffer, 1), [this, self](const sys::error_code& ec, std::size_t) {
		if (ec == sys::errc::operation_canceled)
			return;
		_timer.cancel();
		if (_ackBuffer == '\n' || _ackBuffer == '\r') {
			//we eat some garbage, discard and carry on
			recvAck();
		} else {
			handleEvent(ec);
		}
	});
}

template<typename MutableBuffer>
void VantagePro2Connector::recvData(const MutableBuffer& buffer)
{
	auto self(std::static_pointer_cast<VantagePro2Connector>(shared_from_this()));
	countdown(chrono::seconds(6));
	async_read(_sock, buffer, [this, self](const sys::error_code& ec, std::size_t) {
		if (ec == sys::errc::operation_canceled)
			return;
		_timer.cancel();
		handleEvent(ec);
	});
}

template<typename Restarter>
void VantagePro2Connector::flushSocketAndRetry(State restartState, Restarter restart)
{
	auto self(std::static_pointer_cast<VantagePro2Connector>(shared_from_this()));
	// wait before flushing in order not to leave garbage behind
	_timer.expires_from_now(chrono::seconds(10));
	_timer.async_wait([restartState, restart, this, self](const sys::error_code& ec) {
		if (ec == sys::errc::operation_canceled)
			return;
		if (_sock.available() > 0) {
			auto bufs = _discardBuffer.prepare(512);
			std::size_t bytes = _sock.receive(bufs);
			_discardBuffer.commit(bytes);
			_discardBuffer.consume(_discardBuffer.size());
		}
		_currentState = restartState;
		restart();
	});
}

template<typename Restarter>
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
			sd_journal_send("MESSAGE=Too many timeouts, aborting connection",
				"PRIORITY=%i", LOG_CRIT,
				"STATION=%s", _stationUuid,
				"CATEGORY=source",
				"CONNECTOR_TYPE=vp2_directconnect",
				NULL);
			stop();
		}
	} else { /* TCP reset by peer, etc. */
		sd_journal_send("MESSAGE=Unknown network error, aborting connection",
			"PRIORITY=%i", LOG_CRIT,
			"STATION=%s", _stationUuid,
			"CATEGORY=source",
			"CONNECTOR_TYPE=vp2_directconnect",
			NULL);
		stop();
	}
}

void VantagePro2Connector::handleEvent(const sys::error_code& e)
{
	auto requestEcho = [this]() { sendRequest(_echoRequest, sizeof(_echoRequest) - 1); };
	auto requestStation = [this]() { sendRequest(_getStationRequest, sizeof(_getStationRequest) - 1); };
	auto requestMainMode = [this]() { sendRequest(_mainModeRequest, sizeof(_mainModeRequest) - 1); };
	auto requestTimezone = [this]() { sendRequest(_getTimezoneRequest, sizeof(_getTimezoneRequest) - 1); };
	auto requestArchive = [this]() { sendRequest(_getArchiveRequest, sizeof(_getArchiveRequest) - 1); };
	auto requestSetTime = [this]() { sendRequest(_settimeRequest, sizeof(_settimeRequest) - 1); };

	switch (_currentState) {
		case State::STARTING:
			_currentState = State::SENDING_WAKE_UP_STATION;
			requestEcho();
			_status.shortStatus = "Waking up station";
			break;

		case State::WAITING_NEXT_MEASURE_TICK:
			if (e == sys::errc::timed_out) {
				_currentState = State::SENDING_WAKE_UP_ARCHIVE;
				sd_journal_send("MESSAGE=New measurement due",
					"PRIORITY=%i", LOG_DEBUG,
					"STATION=%s", _stationUuid,
					"CATEGORY=schedule",
					"CONNECTOR_TYPE=vp2_directconnect",
					NULL);
				requestEcho();
				_status.shortStatus = "Waking up station";
			} else {
				waitForNextMeasure();
				_status.shortStatus = "Waiting for next measurement time";
			}
			break;

		case State::SENDING_WAKE_UP_STATION:
			handleGenericErrors(e, State::SENDING_WAKE_UP_STATION, requestEcho);
			if (e == sys::errc::success) {
				_currentState = State::WAITING_ECHO_STATION;
				recvWakeUp();
			}
			break;

		case State::WAITING_ECHO_STATION:
			handleGenericErrors(e, State::SENDING_WAKE_UP_STATION, requestEcho);
			if (e == sys::errc::success) {
				_currentState = State::SENDING_REQ_STATION;
				requestStation();
				_status.shortStatus = "Waiting for station identification";
			}
			break;

		case State::SENDING_REQ_STATION:
			handleGenericErrors(e, State::SENDING_REQ_STATION, requestStation);
			if (e == sys::errc::success) {
				_currentState = State::WAITING_ACK_STATION;
				recvAck();
			}
			break;

		case State::WAITING_ACK_STATION:
			handleGenericErrors(e, State::SENDING_REQ_STATION, requestStation);
			if (e == sys::errc::success) {
				if (_ackBuffer != 0x06) {
					if (++_transmissionErrors < 5) {
						flushSocketAndRetry(State::SENDING_REQ_STATION, requestStation);
					} else {
						sd_journal_send("MESSAGE=Station did not acknowledge the identification request, aborting",
							"PRIORITY=%i", LOG_CRIT,
							"CATEGORY=communication",
							"CONNECTOR_TYPE=vp2_directconnect",
							NULL);
						stop();
					}
				} else {
					_currentState = State::WAITING_DATA_STATION;
					recvData(asio::buffer(_coords));
				}
			}
			break;

		case State::WAITING_DATA_STATION:
			handleGenericErrors(e, State::SENDING_REQ_STATION, requestStation);
			if (e == sys::errc::success) {
				if (!VantagePro2Message::validateCRC(_coords, sizeof(_coords))) {
					if (++_transmissionErrors < 5) {
						flushSocketAndRetry(State::SENDING_REQ_STATION, requestStation);
					} else {
						sd_journal_send("MESSAGE=Transmission error during the identification request, aborting",
							"PRIORITY=%i", LOG_CRIT,
							"CATEGORY=communication",
							"CONNECTOR_TYPE=vp2_directconnect",
							NULL);
						stop();
					}
				} else {
					// From documentation, latitude, longitude and elevation are stored contiguously
					// in this order in the station's EEPROM
					time_t lastArchiveDownloadTime;
					bool storeInsideMeasurements;
					bool found = _db.getStationByCoords(_coords[2], _coords[0], _coords[1], _station, _stationName,
						_pollingPeriod, lastArchiveDownloadTime, &storeInsideMeasurements);
					cass_uuid_string(_station, _stationUuid);
					_timeOffseter.setLatitude(_coords[0]);
					_timeOffseter.setLongitude(_coords[1]);
					_timeOffseter.setElevation(_coords[2]);
					_timeOffseter.setMeasureStep(_pollingPeriod);
					_timeOffseter.setMayStoreInsideMeasurements(storeInsideMeasurements);
					_lastArchive = date::sys_seconds(chrono::seconds(lastArchiveDownloadTime));
					if (found) {
						sd_journal_send("MESSAGE=Station %s is connected", _stationName,
							"PRIORITY=%i", LOG_INFO,
							"STATION=%s", _stationUuid,
							"CATEGORY=source",
							"CONNECTOR_TYPE=vp2_directconnect",
							NULL);
						_currentState = State::SENDING_REQ_MAIN_MODE;
						requestMainMode();
						_status.shortStatus = "Waiting for station ack to main configuration";
					} else {
						sd_journal_send("MESSAGE=Unidentifiable station (%.2f,%.2f,%d) has showed up", _coords[0], _coords[1], _coords[2],
							"PRIORITY=%i", LOG_CRIT,
							"CATEGORY=configuration",
							"CONNECTOR_TYPE=vp2_directconnect",
							NULL);
						stop();
					}
				}
			}
			break;

		case State::SENDING_REQ_MAIN_MODE:
			handleGenericErrors(e, State::SENDING_REQ_MAIN_MODE, requestMainMode);
			if (e == sys::errc::success) {
				_currentState = State::WAITING_ACK_MAIN_MODE;
				recvOk();
			}
			break;

		case State::WAITING_ACK_MAIN_MODE:
			handleGenericErrors(e, State::SENDING_REQ_MAIN_MODE, requestMainMode);
			if (e == sys::errc::success) {
				_currentState = State::SENDING_REQ_TIMEZONE;
				requestTimezone();
			} else {
				sd_journal_send("MESSAGE=Transmission error during ACK_MAIN_MODE command, aborting",
					"PRIORITY=%i", LOG_CRIT,
					"STATION=%s", _stationUuid,
					"CATEGORY=communication",
					"CONNECTOR_TYPE=vp2_directconnect",
					NULL);
				stop();
			}
			break;

		case State::SENDING_REQ_TIMEZONE:
			handleGenericErrors(e, State::SENDING_REQ_TIMEZONE, requestTimezone);
			if (e == sys::errc::success) {
				_currentState = State::WAITING_ACK_TIMEZONE;
				recvAck();
				_status.shortStatus = "Waiting for station timezone";
			}
			break;

		case State::WAITING_ACK_TIMEZONE:
			handleGenericErrors(e, State::SENDING_REQ_TIMEZONE, requestTimezone);
			if (e == sys::errc::success) {
				if (_ackBuffer != 0x06) {
					if (++_transmissionErrors < 5) {
						flushSocketAndRetry(State::SENDING_REQ_TIMEZONE, requestTimezone);
					} else {
						sd_journal_send("MESSAGE=Transmission error at ACK_TIMEZONE, aborting",
							"PRIORITY=%i", LOG_CRIT,
							"STATION=%s", _stationUuid,
							"CATEGORY=communication",
							"CONNECTOR_TYPE=vp2_directconnect",
							NULL);
						stop();
					}
				} else {
					_currentState = State::WAITING_DATA_TIMEZONE;
					recvData(asio::buffer(&_timezoneBuffer, sizeof(_timezoneBuffer)));
				}
			}
			break;

		case State::WAITING_DATA_TIMEZONE:
			handleGenericErrors(e, State::SENDING_REQ_TIMEZONE, requestTimezone);
			if (e == sys::errc::success) {
				if (!VantagePro2Message::validateCRC(_coords, sizeof(_coords))) {
					if (++_transmissionErrors < 5) {
						flushSocketAndRetry(State::SENDING_REQ_TIMEZONE, requestTimezone);
					} else {
						sd_journal_send("MESSAGE=Transmission error at DATA_TIMEZONE, aborting",
							"PRIORITY=%i", LOG_CRIT,
							"STATION=%s", _stationUuid,
							"CATEGORY=communication",
							"CONNECTOR_TYPE=vp2_directconnect",
							NULL);
						stop();
					}
				} else {
					_timeOffseter.prepare(_timezoneBuffer);
					_archivePage.prepare(_lastArchive, &_timeOffseter);
					chrono::system_clock::time_point now = chrono::system_clock::now();
					sd_journal_send("MESSAGE=Last data from station dates back from %s", _lastArchive,
						"PRIORITY=%i", LOG_DEBUG,
						"STATION=%s", _stationUuid,
						"CATEGORY=storage",
						"CONNECTOR_TYPE=vp2_directconnect",
						NULL);
					if ((now - _lastArchive) > chrono::minutes(_pollingPeriod)) {
						sd_journal_send("MESSAGE=Station disconnected for too long, retrieving the archives...",
							"PRIORITY=%i", LOG_NOTICE,
							"STATION=%s", _stationUuid,
							"CATEGORY=source",
							"CONNECTOR_TYPE=vp2_directconnect",
							NULL);
						_currentState = State::SENDING_WAKE_UP_ARCHIVE;
						requestEcho();
					} else {
						_currentState = State::SENDING_WAKE_UP_SETTIME;
						requestEcho();
					}
				}
			}
			break;

		case State::SENDING_WAKE_UP_ARCHIVE:
			handleGenericErrors(e, State::SENDING_WAKE_UP_ARCHIVE, requestEcho);
			if (e == sys::errc::success) {
				_currentState = State::WAITING_ECHO_ARCHIVE;
				recvWakeUp();
				_status.shortStatus = "Waiting for archive download";
			}
			break;

		case State::WAITING_ECHO_ARCHIVE:
			handleGenericErrors(e, State::SENDING_WAKE_UP_ARCHIVE, requestEcho);
			if (e == sys::errc::success) {
				_currentState = State::SENDING_REQ_ARCHIVE;
				requestArchive();
			}
			break;

		case State::SENDING_REQ_ARCHIVE:
			handleGenericErrors(e, State::SENDING_REQ_ARCHIVE, requestArchive);
			if (e == sys::errc::success) {
				_currentState = State::WAITING_ACK_ARCHIVE;
				recvAck();
			}
			break;

		case State::WAITING_ACK_ARCHIVE:
			handleGenericErrors(e, State::SENDING_REQ_ARCHIVE, requestArchive);
			if (e == sys::errc::success) {
				if (_ackBuffer != 0x06) {
					if (++_transmissionErrors < 5) {
						flushSocketAndRetry(State::SENDING_REQ_ARCHIVE, requestArchive);
					} else {
						sd_journal_send("MESSAGE=Transmission error at REQ_ARCHIVE, aborting",
							"PRIORITY=%i", LOG_CRIT,
							"STATION=%s",
							"CATEGORY=communication",
							"CONNECTOR_TYPE=vp2_directconnect",
							NULL);
						stop();
					}
				} else {
					_currentState = State::SENDING_ARCHIVE_PARAMS;
					auto buffer = buildArchiveRequestParams(_lastArchive);
					sendBuffer(buffer, sizeof(ArchiveRequestParams));
				}
			}
			break;

		case State::SENDING_ARCHIVE_PARAMS:
			// We cannot retry anything here, we are in the middle of a request, just give up
			if (e != sys::errc::success) {
				sd_journal_send("MESSAGE=Transmission error at SENDING_ARCHIVE_PARAMS, aborting",
						"PRIORITY=%i", LOG_CRIT,
						"STATION=%s", _stationUuid,
						"CATEGORY=communication",
						"CONNECTOR_TYPE=vp2_directconnect",
						NULL);
				stop();
			} else {
				_currentState = State::WAITING_ACK_ARCHIVE_PARAMS;
				recvAck();
			}
			break;

		case State::WAITING_ACK_ARCHIVE_PARAMS:
			// We cannot retry anything here, we are in the middle of a request, just give up
			if (e != sys::errc::success) {
				sd_journal_send("MESSAGE=Transmission error at ACK_ARCHIVE_PARAMS, aborting",
						"PRIORITY=%i", LOG_CRIT,
						"STATION=%s", _stationUuid,
						"CATEGORY=communication",
						"CONNECTOR_TYPE=vp2_directconnect",
						NULL);
				stop();
			} else {
				if (_ackBuffer != 0x06) {
					sd_journal_send("MESSAGE=Transmission error at ACK_ARCHIVE_PARAMS, aborting",
							"PRIORITY=%i", LOG_CRIT,
							"STATION=%s", _stationUuid,
							"CATEGORY=communication",
							"CONNECTOR_TYPE=vp2_directconnect",
							NULL);
					stop();
				} else {
					_currentState = State::WAITING_ARCHIVE_NB_PAGES;
					recvData(asio::buffer(&_archiveSize, sizeof(_archiveSize)));
				}
			}
			break;

		case State::WAITING_ARCHIVE_NB_PAGES:
			// We cannot retry anything here, we are in the middle of a request, just give up
			if (e != sys::errc::success) {
				sd_journal_send("MESSAGE=Transmission error at ARCHIVE_NB_PAGES, aborting",
						"PRIORITY=%i", LOG_CRIT,
						"STATION=%s", _stationUuid,
						"CATEGORY=communication",
						"CONNECTOR_TYPE=vp2_directconnect",
						NULL);
				stop();
			} else {
				if (VantagePro2Message::validateCRC(&_archiveSize, sizeof(_archiveSize))) {
					_currentState = State::SENDING_ACK_ARCHIVE_DOWNLOAD;
					sd_journal_send("MESSAGE=Expecting %d pages, first record at %d", _archiveSize.pagesLeft, _archiveSize.index,
							"PRIORITY=%i", LOG_DEBUG,
							"STATION=%s", _stationUuid,
							"CATEGORY=communication",
							"CONNECTOR_TYPE=vp2_directconnect",
							NULL);
					sendAck();
					_status.shortStatus = "Downloading " + std::to_string(_archiveSize.pagesLeft) + " archive pages";
				} else {
					_currentState = State::SENDING_ABORT_ARCHIVE_DOWNLOAD;
					sd_journal_send("MESSAGE=Wrong CRC at ARCHIVE_NB_PAGES",
							"PRIORITY=%i", LOG_ERR,
							"STATION=%s", _stationUuid,
							"CATEGORY=communication",
							"CONNECTOR_TYPE=vp2_directconnect",
							NULL);
					sendAbort();
				}
			}
			break;

		case State::SENDING_ABORT_ARCHIVE_DOWNLOAD:
			// We cannot retry anything here, we are in the middle of a request, just give up
			if (e != sys::errc::success) {
				sd_journal_send("MESSAGE=Transmission error at ABORT_ARCHIVE_DOWNLOAD, aborting",
						"PRIORITY=%i", LOG_CRIT,
						"STATION=%s", _stationUuid,
						"CATEGORY=communication",
						"CONNECTOR_TYPE=vp2_directconnect",
						NULL);
				stop();
			} else {
				_currentState = State::WAITING_NEXT_MEASURE_TICK;
				waitForNextMeasure();
			}
			break;

		case State::WAITING_ARCHIVE_PAGE:
			// We cannot retry anything here, we are in the middle of a request, just give up
			if (e != sys::errc::success) {
				sd_journal_send("MESSAGE=Transmission error at WAITING_ARCHIVE_PAGE, aborting",
						"PRIORITY=%i", LOG_CRIT,
						"STATION=%s", _stationUuid,
						"CATEGORY=communication",
						"CONNECTOR_TYPE=vp2_directconnect",
						NULL);
				stop();
			} else {
				if (_archivePage.isValid()) {
					bool ret = _archivePage.store(_db, _station);
					if (ret) {
						sd_journal_send("MESSAGE=Archive page stored, now updating the last archive timestamp",
								"PRIORITY=%i", LOG_INFO,
								"STATION=%s", _stationUuid,
								"CATEGORY=storage",
								"CONNECTOR_TYPE=vp2_directconnect",
								NULL);

						if (_archivePage.lastArchiveRecordDateTime() > _newestArchive) {
							_newestArchive = _archivePage.lastArchiveRecordDateTime();
							time_t lastArchiveDownloadTime = chrono::system_clock::to_time_t(_archivePage.lastArchiveRecordDateTime());
							ret = _db.updateLastArchiveDownloadTime(_station, lastArchiveDownloadTime);
						}
						if (_archivePage.lastArchiveRecordDateTime() < _oldestArchive) {
							_oldestArchive = _archivePage.lastArchiveRecordDateTime();
						}
						if (!ret) {
							sd_journal_send("MESSAGE=Couldn't update last archive download time",
									"PRIORITY=%i", LOG_ERR,
									"STATION=%s", _stationUuid,
									"CATEGORY=storage",
									"CONNECTOR_TYPE=vp2_directconnect",
									NULL);
						}
					} else {
						sd_journal_send("MESSAGE=Couldn't store the archive page",
								"PRIORITY=%i", LOG_CRIT,
								"STATION=%s", _stationUuid,
								"CATEGORY=storage",
								"CONNECTOR_TYPE=vp2_directconnect",
								NULL);
						stop();
					}
					_archiveSize.pagesLeft--;
					_currentState = State::SENDING_ARCHIVE_PAGE_ANSWER;
					sendAck();
				} else {
					_currentState = State::SENDING_ARCHIVE_PAGE_ANSWER;
					_transmissionErrors++;
					if (_transmissionErrors > 100) {
						sd_journal_send("MESSAGE=Received too many incorrect archive data, aborting",
								"PRIORITY=%i", LOG_CRIT,
								"STATION=%s", _stationUuid,
								"CATEGORY=communication",
								"CONNECTOR_TYPE=vp2_directconnect",
								NULL);
						stop();
					}
					sd_journal_send("MESSAGE=Received incorrect archive data, retrying",
							"PRIORITY=%i", LOG_ERR,
							"STATION=%s", _stationUuid,
							"CATEGORY=communication",
							"CONNECTOR_TYPE=vp2_directconnect",
							NULL);
					sendNak();
				}
			}
			break;

		case State::SENDING_ACK_ARCHIVE_DOWNLOAD:
		case State::SENDING_ARCHIVE_PAGE_ANSWER:
			// We cannot retry anything here, we are in the middle of a request, just give up
			if (e != sys::errc::success) {
				sd_journal_send("MESSAGE=Transmission error at ARCHIVE_DOWNLOAD, aborting",
						"PRIORITY=%i", LOG_CRIT,
						"STATION=%s", _stationUuid,
						"CATEGORY=communication",
						"CONNECTOR_TYPE=vp2_directconnect",
						NULL);
				stop();
			} else {
				if (_archiveSize.pagesLeft) {
					_currentState = State::WAITING_ARCHIVE_PAGE;
					recvData(_archivePage.getBuffer());
				} else {
					_status.nbDownloads++;
					_status.lastDownload = date::floor<chrono::seconds>(chrono::system_clock::now());
					_lastArchive = _newestArchive;
					sd_journal_send("MESSAGE=Data collected and stored succesfully",
							"PRIORITY=%i", LOG_INFO,
							"STATION=%s", _stationUuid,
							"CATEGORY=measurement",
							"CONNECTOR_TYPE=vp2_directconnect",
							NULL);

					if (_jobPublisher && date::floor<date::days>(_oldestArchive) < date::floor<date::days>(chrono::system_clock::now())) {
						_jobPublisher->publishJobsForPastDataInsertion(_station, _oldestArchive, _newestArchive);
					}

					if (_setTimeRequested) {
						_currentState = State::SENDING_WAKE_UP_SETTIME;
						requestEcho();
						_status.shortStatus = "Setting the station clock";
					} else {
						_currentState = State::WAITING_NEXT_MEASURE_TICK;
						waitForNextMeasure();
					}
				}
			}
			break;

		case State::SENDING_WAKE_UP_SETTIME:
			handleGenericErrors(e, State::SENDING_WAKE_UP_SETTIME, requestEcho);
			if (e == sys::errc::success) {
				_currentState = State::WAITING_ECHO_SETTIME;
				recvWakeUp();
			}
			break;

		case State::WAITING_ECHO_SETTIME:
			handleGenericErrors(e, State::SENDING_WAKE_UP_SETTIME, requestEcho);
			if (e == sys::errc::success) {
				_currentState = State::SENDING_SETTIME;
				requestSetTime();
			}
			break;

		case State::SENDING_SETTIME:
			handleGenericErrors(e, State::SENDING_SETTIME, requestSetTime);
			if (e == sys::errc::success) {
				_currentState = State::WAITING_ACK_SETTIME;
				recvAck();
			}
			break;

		case State::WAITING_ACK_SETTIME:
			handleGenericErrors(e, State::SENDING_SETTIME, requestSetTime);
			if (e == sys::errc::success) {
				if (_ackBuffer != 0x06) {
					if (++_transmissionErrors < 5) {
						flushSocketAndRetry(State::SENDING_SETTIME, requestSetTime);
					} else {
						sd_journal_send("MESSAGE=Transmission error at SETTIME, aborting",
								"PRIORITY=%i", LOG_CRIT,
								"STATION=%s", _stationUuid,
								"CATEGORY=communication",
								"CONNECTOR_TYPE=vp2_directconnect",
								NULL);
						stop();
					}
				} else {
					_currentState = State::SENDING_SETTIME_PARAMS;
					auto buffer = buildSettimeParams();
					sendBuffer(buffer, sizeof(SettimeRequestParams));
				}
			}
			break;

		case State::SENDING_SETTIME_PARAMS:
			// We cannot retry anything here, we are in the middle of a request, just give up
			if (e != sys::errc::success) {
				sd_journal_send("MESSAGE=Transmission error at SETTIME_PARAMS, aborting",
						"PRIORITY=%i", LOG_CRIT,
						"STATION=%s", _stationUuid,
						"CATEGORY=communication",
						"CONNECTOR_TYPE=vp2_directconnect",
						NULL);
				stop();
			} else {
				_currentState = State::WAITING_ACK_TIME_SET;
				recvAck();
			}
			break;

		case State::WAITING_ACK_TIME_SET:
			// We cannot retry anything here, we are in the middle of a request, just give up
			if (e != sys::errc::success || _ackBuffer != 0x06) {
				sd_journal_send("MESSAGE=Transmission error at TIME_SET, aborting",
						"PRIORITY=%i", LOG_ERR,
						"STATION=%s", _stationUuid,
						"CATEGORY=communication",
						"CONNECTOR_TYPE=vp2_directconnect",
						NULL);
			} else {
				sd_journal_send("MESSAGE=Time set",
						"PRIORITY=%i", LOG_INFO,
						"STATION=%s", _stationUuid,
						"CATEGORY=configuration",
						"CONNECTOR_TYPE=vp2_directconnect",
						NULL);
			}

			_setTimeRequested = false;
			_setTimeTimer.expires_from_now(chrono::hours(1));
			{
				auto self(std::static_pointer_cast<VantagePro2Connector>(shared_from_this()));
				_setTimeTimer.async_wait([this, self](const sys::error_code& e) {
					handleSetTimeDeadline(e);
				});
				_currentState = State::WAITING_NEXT_MEASURE_TICK;
				waitForNextMeasure();
			}
			break;

		case State::STOPPED:
			/* discard everything, only spurious events from cancelled
				operations can get here */
			break;
	}
}

std::string VantagePro2Connector::getStatus() const
{
	std::ostringstream os;
	os << _stationName
	   << " [" << _station << "]\n"
	   << Connector::getStatus();
	return os.str();
}

}
