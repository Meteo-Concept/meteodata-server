/**
 * @file abstract_download_scheduler.h
 * @brief Definition of the AbstractApiDownloadScheduler class
 * @author Laurent Georget
 * @date 2022-08-05
 */
/*
 * Copyright (C) 2022  SAS JD Environnement <contact@meteo-concept.fr>
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

#ifndef ABSTRACT_DOWNLOAD_SCHEDULER_H
#define ABSTRACT_DOWNLOAD_SCHEDULER_H

#include <memory>
#include <vector>
#include <string>
#include <chrono>
#include <map>

#include <boost/asio/basic_waitable_timer.hpp>
#include <cassandra.h>
#include <dbconnection_observations.h>

#include "curl_wrapper.h"
#include "connector.h"

namespace meteodata
{

namespace asio = boost::asio;
namespace sys = boost::system;
namespace chrono = std::chrono;

using namespace std::placeholders;

/**
 * @brief The orchestrator for all requests to websites offering a HTTP API (be
 * it a simple file updated every so often, a full-fledged REST API, or anything
 * in-between)
 *
 * We normally need only one instance of this class (several can be used to
 * parallelize requests to the API). Instances of this class are responsible to
 * prepare a HTTP client and call all the individual downloaders (one per station).
 */
class AbstractDownloadScheduler : public Connector
{
public:
	/**
	 * @brief Construct the download scheduler
	 *
	 * @param ioContext the Boost object used to process asynchronous
	 * events, timers, and callbacks
	 * @param db the Météodata observations database connector
	 */
	AbstractDownloadScheduler(chrono::steady_clock::duration period, asio::io_context& ioContext,
							  DbConnectionObservations& db);

	/**
	 * @brief Start the periodic downloads
	 */
	void start() override;

	/**
	 * @brief Stop the periodic downloads
	 */
	void stop() override;

	/**
	 * @brief Reload the configuration
	 */
	 void reload() override;

protected:
	/**
	 * @brief The CURL handler used to make HTTP requests
	 */
	CurlWrapper _client;

	/**
	 * @brief The default time to add to the scheduled download time, to make
	 * sure the download is ready (for instance, if data are available every ten
	 * minutes, download at minutes 02, 12, 22, etc. to make sure the data
	 * generated at 00, 10, 20, etc. is available for download)
	 */
	chrono::minutes _offset{2};

private:
	/**
	 * @brief The timer used to periodically trigger the data downloads
	 */
	asio::basic_waitable_timer<chrono::steady_clock> _timer;

	/**
	 * @brief Whether to stop collecting data
	 */
	bool _mustStop = false;

	/**
	 * @brief The time between two measurements
	 */
	chrono::steady_clock::duration _period;

	/**
	 * @brief Reload the list of StatIC stations from the database and
	 * recreate all downloaders
	 */
	virtual void reloadStations() = 0;

	/**
	 * @brief Wait for the periodic download timer to tick again
	 */
	virtual void waitUntilNextDownload();

	/**
	 * @brief Download archive data for all stations
	 *
	 * Archive data are downloaded since the last timestamp the data is
	 * previously available for the station.
	 */
	virtual void download() = 0;

	/**
	 * @brief The callback registered to react to the periodic download
	 * timer ticking
	 *
	 * This method makes sure the deadline set for the timer is actually
	 * reached (the timer could go off in case of an error, or anything).
	 *
	 * @param e The error/return code of the timer event
	 */
	void checkDeadline(const sys::error_code& e);
};

}

#endif
