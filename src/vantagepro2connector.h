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
	 * @brief Perform the WakeUp sequence described in the documentation by
	 * Davis Instrument
	 *
	 * @return a Boost::Asio error code telling if the communication with
	 * the station was successful
	 */
	sys::error_code wakeUp();
	/**
	 * @brief Empty the communication buffer
	 */
	void flushSocket();
	/**
	 * @brief Store one data point (a measurement) to the database
	 */
	void storeData();

	/**
	 * @brief Ask the station for something (a data point, informations
	 * about the station status, etc.)
	 *
	 * @tparam MutableBuffer the type of the communication buffer
	 * @param req the request sent to the station
	 * @param reqSize the size of the request
	 * @param buffer the buffer in which the answer from the station is to
	 * be stored
	 *
	 * @return a Boost::Asio error code telling if the communication with
	 * the station was successful
	 */
	template <typename MutableBuffer>
	sys::error_code askForData(const char* req, int reqSize, const MutableBuffer& buffer);

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
	 * @brief The connected station's identifier in the database
	 */
	CassUuid _station;
};

}

#endif
