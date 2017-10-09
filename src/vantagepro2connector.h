/**
 * @file vantagepro2connector.h
 * @brief Definition of the VantagePro2Connector class
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

#ifndef VANTAGEPRO2CONNECTOR_H
#define VANTAGEPRO2CONNECTOR_H

#include <iostream>
#include <memory>
#include <array>
#include <vector>
#include <functional>

#include <boost/system/error_code.hpp>
#include <boost/asio.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/asio/deadline_timer.hpp>

#include <syslog.h>
#include <unistd.h>

#include <cassandra.h>

#include "connector.h"
#include "vantagepro2message.h"


namespace meteodata {

namespace ip = boost::asio::ip;
namespace asio = boost::asio;
namespace sys = boost::system; //system() is a function, it cannot be redefined
			       //as a namespace
namespace chrono = boost::posix_time;

using namespace std::placeholders;
using namespace meteodata;

/**
 * @brief A Connector designed for the VantagePro2 (R) station by Davis
 * Instruments (R)
 */
class VantagePro2Connector : public Connector
{
public:
	/**
	 * @brief Construct a new Connector for a VantagePro2 station
	 *
	 * @param ioService the Boost::Asio service to use for all aynchronous
	 * network operations
	 * @param db The handle to the database
	 */
	VantagePro2Connector(boost::asio::io_service& ioService, DbConnection& db);

	//main loop
	virtual void start() override;

private:
	/**
	 * @brief Represent a state in the state machine
	 */
	enum class State {
		STARTING, /*!< Initial state */
		WAITING_NEXT_MEASURE_TICK, /*!< Waiting for the timer to hit the deadline for the next measurement */
		SENDING_WAKE_UP_STATION, /*!< Waiting for the wake up request to be sent */
		WAITING_ECHO_STATION, /*!< Waiting for the station to answer the wake up request */
		SENDING_REQ_STATION, /*!< Waiting for the identification request to be sent */
		WAITING_ACK_STATION, /*!< Waiting for the identification request to be acknowledgement */
		WAITING_DATA_STATION, /*!< Waiting for the station to answer the identification */
		SENDING_WAKE_UP_MEASURE, /*!< Waiting for the wake up request to be sent (before measurement) */
		WAITING_ECHO_MEASURE, /*!< Waiting for the station to answer the wake up request */
		SENDING_REQ_MEASURE, /*!< Waiting for a measurement request to be sent */
		WAITING_ACK_MEASURE, /*!< Waiting for the station to acknowledge the measurement request */
		WAITING_DATA_MEASURE, /*!< Waiting for the station to answer the measurement request */
		SENDING_REQ_ARCHIVE, /*!< Waiting for the archive request to be sent */
		WAITING_ACK_ARCHIVE, /*!< Waiting for the archive request acknowledgement */
		SENDING_ARCHIVE_PARAMS, /*!< Waiting for the archive download parameters to be sent */
		WAITING_ACK_ARCHIVE_PARAMS, /*!< Waiting for the archive download parameters acknowledgement */
		WAITING_ARCHIVE_PAGE, /*!< Waiting for the next page of archive */
		SENDING_ARCHIVE_PAGE_ANSWER, /*!< Waiting for the archive page confirmation to be sent */
		STOPPED /*!< Final state for cleanup operations */
	};
	/* Events have type sys::error_code */


	/**
	 * @brief Check an event and handle generic errors such as timeouts and
	 * I/O errors
	 *
	 * This method can be called upon receiving an event from the station,
	 * it does nothing in case of a success or a cancelation event. It calls
	 * the restart function in case of a timeout or an I/O error if the
	 * maximum number of (respectively) timeouts and transmission errors has
	 * not reached a predefined threshold and aborts the connection in any
	 * other case.
	 *
	 * @tparam Restarter the type of the function to call to retry the
	 * operation
	 * @param e the event from the station
	 * @param restartState the state in which the state machine must jump
	 * before calling the restart function
	 * @param restart a function to call to retry the current operation
	 * (which is about to fail)
	 */
	template <typename Restarter>
	void handleGenericErrors(const sys::error_code& e, State restartState,
		Restarter restart);

	/**
	 * @brief Transition function of the state machine
	 *
	 * Thi smethod receives an event from the station and reacts according
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
	 * @param e an error code which is positioned to a specific value if the
	 * timer has been interrupted before the deadline
	 */
	void checkDeadline(const sys::error_code& e);
	/**
	 * @brief Cease all operations and close the socket
	 *
	 * Calling this method allows for the VantagePro2Connector to be
	 * destroyed and its memory reclaimed because the only permanent shared
	 * pointer to it is owned by the Boost::Asio::io_service which exits
	 * when the socket is closed.
	 */
	void stop();

	/**
	 * @brief Program the timer to send a timeout event when the clock hit
	 * the next multiple of five minutes
	 */
	void waitForNextMeasure();

	/**
	 * @brief Send a message to the station
	 *
	 * @param req the request
	 * @param reqsize the request size
	 */
	void sendRequest(const char* req, int reqsize);
	/**
	 * @brief Wait for the station to answer a wake up request
	 */
	void recvWakeUp();
	/**
	 * @brief Wait for the station to acknowledge last request
	 */
	void recvAck();
	/**
	 * @brief Wait for the station to answer the request
	 *
	 * @tparam MutableBuffer the type of the buffer in which the answer
	 * must be stored
	 * @param buffer the buffer in which the answere must be stored, this
	 * function will not send the success event until the buffer is full
	 */
	template <typename MutableBuffer>
	void recvData(const MutableBuffer& buffer);
	/**
	 * @brief Empty the communication buffer
	 *
	 * @tparam Restarter the type of the function to call to retry the
	 * operation
	 * @param restartState the state in which the state machine must jump
	 * before calling the restart function
	 * @param restart a function to call to retry the current operation
	 * (which is about to fail)
	 */
	template <typename Restarter>
	void flushSocketAndRetry(State restartState, Restarter restart);

	/**
	 * @brief Count the number of missing data points since last insertion
	 * in the database
	 *
	 * \a countMissingDataPoints does some bookkeeping and records
	 * \a newTimestamp as the last time an entry was registered in the
	 * database. If that insertion is older than \a _period then it returns
	 * the number of data points that should have been collected since then
	 * and set previousTimestamp to the time of the last insertion.
	 *
	 * @param newTimestamp the new timestamp to be recorded as the last
	 * insertion time
	 * @param[out] previousTimestamp the time of previous insertion
	 * @return the number of data points that should have been collected
	 * between \a previousTimestamp et \a newTimestamp.
	 */
	int countMissingDataPoints(chrono::ptime newTimestamp, chrono::ptime& previousTimestamp);

	/**
	 * @brief Inserts the data points reconstructed from the archive into
	 * the database
	 *
	 * @return true if, and only if, everything went all right
	 */
	bool insertArchivePoints();

	/**
	 * @brief The current state of the state machine
	 */
	State _currentState;
	/**
	 * @brief A Boost::Asio timer used to time out on network operations and
	 * to wait between two measurements
	 */
	asio::deadline_timer _timer;
	/**
	 * @brief A dummy buffer to store acknowledgements and the like from
	 * the station
	 */
	asio::streambuf _discardBuffer;
	/**
	 * @brief A message in which one data point from the station can be
	 * received
	 */
	VantagePro2Message _message;

	/**
	 * @brief A boolean that tells whether the connector has stopped all
	 * communications with the station
	 *
	 * This boolean is necessary because the VantagePro2Connector will not
	 * die immediately when the socket is closed if the _timer is still in
	 * activity for example. If this happens, then the _timer knows thanks
	 * to this boolean that it should not reset itself but rather stop.
	 */
	bool _stopped = false;
	/**
	 * @brief The number of timeouts registered since the last successful
	 * communication
	 *
	 * When this counter hits a given threshold, the VantagePro2Connector
	 * will consider that the connection to the station is lost and exit.
	 */
	int _timeouts = 0;

	/**
	 * @brief The number of transmission errors registered since the last
	 * successful communication
	 *
	 * When this counter hits a given threshold, the VantagePro2Connector
	 * will abort the connection and exit.
	 */
	int _transmissionErrors = 0;
	/**
	 * @brief The connected station's identifier in the database
	 */
	CassUuid _station;
	/**
	 * @brief The station's name
	 */
	std::string _stationName;
	/**
	 * @brief The amount of time between two queries for data to the stations
	 */
	int _pollingPeriod;
	/**
	 * @brief A one-character buffer to receive acknowledgements from the
	 * station
	 *
	 * When receiving a request, the station usually answers with 0x06, this
	 * buffer stores this acknowledgement.
	 */
	char _ackBuffer;
	/**
	 * @brief A buffer to receive the coordinates from the station
	 */
	int16_t _coords[4];
	/**
	 * @brief An echo request, typically used for the wake up procedure
	 */
	static constexpr char _echoRequest[] = "\n";
	/**
	 * @brief An identification request, querying the coordinates of the
	 * station
	 */
	static constexpr char _getStationRequest[] = "EEBRD 0B 06\n";
	/**
	 * @brief A measurement request, querying one data point
	 */
	static constexpr char _getMeasureRequest[] = "LPS 3 2\n";
	/**
	 * @brief An archive request, for a range an archived data points
	 */
	static constexpr char _getArchiveRequest[] = "DMPAFT\n";
};

}

#endif
