/**
 * @file static_download_scheduler.h
 * @brief Definition of the StaticDownloadScheduler class
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

#ifndef STATIC_DOWNLOAD_SCHEDULER_H
#define STATIC_DOWNLOAD_SCHEDULER_H

#include <memory>
#include <vector>
#include <string>
#include <chrono>
#include <map>
#include <mutex>

#include <boost/system/error_code.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio.hpp>
#include <boost/asio/basic_waitable_timer.hpp>
#include <cassandra.h>
#include <cassobs/dbconnection_observations.h>

#include "time_offseter.h"
#include "curl_wrapper.h"
#include "abstract_download_scheduler.h"
#include "static/static_txt_downloader.h"

namespace meteodata
{

namespace chrono = std::chrono;

using namespace std::placeholders;

/**
 * @brief The orchestrator for all requests to websites offering StatIC-formatted files
 *
 * We normally need only one instance of this class (several can be used to
 * parallelize requests to the API). Instances of this class are responsible to
 * prepare a HTTP client and call all the individual downloaders (one per station).
 */
class StatICDownloadScheduler : public AbstractDownloadScheduler
{
public:
	/**
	 * @brief Construct the download scheduler
	 *
	 * @param ioContext the Boost object used to process asynchronous
	 * events, timers, and callbacks
	 * @param db the MétéoData observations database connector
	 * @param jobPublisher the asynchronous job scheduler
	 */
	StatICDownloadScheduler(asio::io_context& ioContext, DbConnectionObservations& db);

	/**
	 * @brief Add a station to download the data for
	 *
	 * @param station The station Météodata UUID
	 * @param fieldClimateId The Static API id
	 * @param tz The station's local timezone
	 * @param sensors All the sensors registered for that station (see the
	 * StaticApiDownloader class for details)
	 */
	void add(const CassUuid& station, const std::string& host, const std::string& url,
			 bool https, int timezone,
			 const std::map<std::string, std::string>& sensors);

private:
	/**
	 * @brief The list of all downloaders (one per station)
	 */
	std::vector<std::shared_ptr<StatICTxtDownloader>> _downloaders;

	/**
	 * @brief The mutex protecting the downloaders list
	 */
	std::recursive_mutex _downloadersMutex;

	/**
	 * @brief Reload the list of StatIC stations from the database and
	 * recreate all downloaders
	 */
	void reloadStations() override;

	/**
	 * @brief Download archive data for all stations
	 *
	 * Archive data are downloaded since the last timestamp the data is
	 * previously available for the station.
	 */
	void download() override;

	/**
	 * @brief The fixed polling period, in minutes
	 */
	static constexpr int POLLING_PERIOD = 10;
};

}

#endif
