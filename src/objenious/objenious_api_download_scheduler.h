/**
 * @file objenious_api_download_scheduler.h
 * @brief Definition of the ObjeniousApiDownloadScheduler class
 * @author Laurent Georget
 * @date 2021-02-23
 */
/*
 * Copyright (C) 2021  SAS JD Environnement <contact@meteo-concept.fr>
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

#ifndef OBJENIOUS_API_DOWNLOAD_SCHEDULER_H
#define OBJENIOUS_API_DOWNLOAD_SCHEDULER_H

#include <memory>
#include <vector>
#include <string>
#include <chrono>
#include <map>

#include <boost/system/error_code.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio.hpp>
#include <boost/asio/basic_waitable_timer.hpp>
#include <cassandra.h>
#include <dbconnection_observations.h>

#include "objenious_api_downloader.h"
#include "../time_offseter.h"
#include "../curl_wrapper.h"

namespace meteodata
{

namespace asio = boost::asio;
namespace sys = boost::system;
namespace chrono = std::chrono;

using namespace std::placeholders;

/**
 * @brief The orchestrator for all requests to the Objenious API
 *
 * We normally need only one instance of this class (several can be used to
 * paralellize requests to the API). Instances of this class are responsible to
 * prepare a HTTP client, connect it to the API server and call all the
 * individual downloaders (one per station) on the client.
 */
class ObjeniousApiDownloadScheduler : public std::enable_shared_from_this<ObjeniousApiDownloadScheduler>
{
public:
	/**
	 * @brief Construct the download scheduler
	 *
	 * @param ioService the Boost object used to process asynchronous
	 * events, timers, and callbacks
	 * @param db the Météodata observations database connector
	 * @param apiKey the Objenious API key
	 */
	ObjeniousApiDownloadScheduler(asio::io_service& ioService, DbConnectionObservations& db, const std::string& apiKey);

	/**
	 * @brief Start the periodic downloads
	 */
	void start();

	/**
	 * @brief Stop the periodic downloads
	 */
	void stop();

	/**
	 * @brief Add a station to download the data for
	 *
	 * @param station The station Météodata UUID
	 * @param fieldClimateId The Objenious API id
	 * @param variables All the variables registered for that station (see
	 * the ObjeniousApiDownloader class for details)
	 */
	void
	add(const CassUuid& station, const std::string& fieldClimateId, const std::map<std::string, std::string> variables);

private:
	/**
	 * @brief The Boost service that processes asynchronous events, timers,
	 * etc.
	 */
	asio::io_service& _ioService;

	/**
	 * @brief The observations database connector
	 *
	 * We use this just to hand it over to the downloaders.
	 */
	DbConnectionObservations& _db;

	/**
	 * @brief The Objenious API key
	 */
	const std::string _apiKey;

	/**
	 * @brief The timer used to periodically trigger the data downloads
	 */
	asio::basic_waitable_timer<chrono::steady_clock> _timer;

	/**
	 * @brief The list of all downloaders (one per station)
	 */
	std::vector<std::shared_ptr<ObjeniousApiDownloader>> _downloaders;

	CurlWrapper _client;

	bool _mustStop = false;

public:
	/**
	 * @brief The type of the const iterators through the downloaders
	 */
	using DownloaderIterator = decltype(_downloaders)::const_iterator;

private:
	/**
	 * @brief Reload the list of Pessl stations from the database and
	 * recreate all downloaders
	 */
	void reloadStations();

	/**
	 * @brief Wait for the periodic download timer to tick again
	 */
	void waitUntilNextDownload();

	/**
	 * @brief Download archive data for all stations
	 *
	 * Archive data are downloaded since the last timestamp the data is
	 * previously available for the station.
	 */
	void downloadArchives();

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

	/**
	 * @brief The fixed polling period, for stations authorized to get
	 * realtime data more frequently than others, in minutes
	 */
	static constexpr int POLLING_PERIOD = 15;
};

}

#endif
