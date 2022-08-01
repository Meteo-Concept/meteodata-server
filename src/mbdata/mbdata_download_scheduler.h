/**
 * @file mbdata_download_scheduler.h
 * @brief Definition of the MBDataDownloadScheduler class
 * @author Laurent Georget
 * @date 2022-08-01
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

#ifndef MBDATA_DOWNLOAD_SCHEDULER_H
#define MBDATA_DOWNLOAD_SCHEDULER_H

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

#include "mbdata_txt_downloader.h"
#include "../time_offseter.h"
#include "../curl_wrapper.h"

namespace meteodata
{

namespace asio = boost::asio;
namespace sys = boost::system;
namespace chrono = std::chrono;

using namespace std::placeholders;

/**
 * @brief The orchestrator for all requests to websites offering MBData-formatted files
 *
 * We normally need only one instance of this class (several can be used to
 * parallelize requests to the API). Instances of this class are responsible to
 * prepare a HTTP client and call all the individual downloaders (one per station).
 */
class MBDataDownloadScheduler : public std::enable_shared_from_this<MBDataDownloadScheduler>
{
public:
	/**
	 * @brief Construct the download scheduler
	 *
	 * @param ioService the Boost object used to process asynchronous
	 * events, timers, and callbacks
	 * @param db the Météodata observations database connector
	 */
	MBDataDownloadScheduler(asio::io_service& ioService, DbConnectionObservations& db);

	/**
	 * @brief Start the periodic downloads
	 */
	void start();

	/**
	 * @brief Stop the periodic downloads
	 */
	void stop();

	/**
	 * @brief Reload the configuration
	 */
	 void reload();

	/**
	 * @brief Add a station to download the data for
	 */
	void add(const std::tuple<CassUuid, std::string, std::string, bool, int, std::string>& downloadDetails);

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
	 * @brief The timer used to periodically trigger the data downloads
	 */
	asio::basic_waitable_timer<chrono::steady_clock> _timer;

	/**
	 * @brief The list of all downloaders (one per station)
	 */
	std::vector<std::shared_ptr<MBDataTxtDownloader>> _downloaders;

	CurlWrapper _client;

	/**
	 * @brief Whether to stop collecting data
	 */
	bool _mustStop = false;

public:
	/**
	 * @brief The type of the const iterators through the downloaders
	 */
	using DownloaderIterator = decltype(_downloaders)::const_iterator;

private:
	/**
	 * @brief Reload the list of MBData stations from the database and
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
	static constexpr int POLLING_PERIOD = 10;
};

}

#endif
