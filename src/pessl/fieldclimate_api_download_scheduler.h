/**
 * @file fieldclimate_api_download_scheduler.h
 * @brief Definition of the FieldClimateApiDownloadScheduler class
 * @author Laurent Georget
 * @date 2020-09-02
 */
/*
 * Copyright (C) 2020  SAS JD Environnement <contact@meteo-concept.fr>
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

#ifndef FIELDCLIMATE_API_DOWNLOAD_SCHEDULER_H
#define FIELDCLIMATE_API_DOWNLOAD_SCHEDULER_H

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

#include "async_job_publisher.h"
#include "time_offseter.h"
#include "curl_wrapper.h"
#include "abstract_download_scheduler.h"
#include "pessl/fieldclimate_api_downloader.h"

namespace meteodata
{

namespace asio = boost::asio;
namespace sys = boost::system;
namespace chrono = std::chrono;

using namespace std::placeholders;

/**
 * @brief The orchestrator for all requests to the FieldClimate API
 *
 * We normally need only one instance of this class (several can be used to
 * parallelize requests to the API). Instances of this class are responsible to
 * prepare a HTTP client, connect it to the API server and call all the
 * individual downloaders (one per station) on the client.
 */
class FieldClimateApiDownloadScheduler : public AbstractDownloadScheduler
{
public:
	/**
	 * @brief Construct the download scheduler
	 *
	 * @param ioContext the Boost object used to process asynchronous
	 * events, timers, and callbacks
	 * @param db the Météodata observations database connector
	 * @param apiId the public part of the FieldClimate API key
	 * @param apiSecret the private part of the FieldClimate API key
	 */
	FieldClimateApiDownloadScheduler(asio::io_context& ioContext,
		DbConnectionObservations& db, std::string apiId, std::string apiSecret,
		AsyncJobPublisher* jobPublisher = nullptr);

	/**
	 * @brief Add a station to download the data for
	 *
	 * @param station The station Météodata UUID
	 * @param fieldClimateId The FieldClimate API id
	 * @param tz The station's local timezone
	 * @param sensors All the sensors registered for that station (see the
	 * FieldClimateApiDownloader class for details)
	 */
	void add(const CassUuid& station, const std::string& fieldClimateId, TimeOffseter::PredefinedTimezone tz,
			 const std::map<std::string, std::string>& sensors);

private:
	/**
	 * @brief The public part of the FieldClimate API key
	 */
	const std::string _apiId;

	/**
	 * @brief The private part of the FieldClimate API key
	 */
	const std::string _apiSecret;

	/**
	 * @brief The component able to schedule computations of climatology and
	 * monitoring indices
	 */
	AsyncJobPublisher* _jobPublisher;

	/**
	 * @brief The list of all downloaders (one per station)
	 */
	std::vector<std::shared_ptr<FieldClimateApiDownloader>> _downloaders;

	std::recursive_mutex _downloadersMutex;

private:
	/**
	 * @brief Reload the list of Pessl stations from the database and
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
	 * @brief The fixed polling period, for stations authorized to get
	 * realtime data more frequently than others, in minutes
	 */
	static constexpr int POLLING_PERIOD = 15;
};

}

#endif
