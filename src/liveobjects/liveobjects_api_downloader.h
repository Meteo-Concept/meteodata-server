/**
 * @file liveobjects_api_downloader.h
 * @brief Definition of the LiveobjectsApiDownloader class
 * @author Laurent Georget
 * @date 2023-04-09
 */
/*
 * Copyright (C) 2023  SAS JD Environnement <contact@meteo-concept.fr>
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

#ifndef LIVEOBJECTS_API_DOWNLOADER_H
#define LIVEOBJECTS_API_DOWNLOADER_H

#include <map>
#include <string>
#include <tuple>
#include <memory>

#include <boost/property_tree/json_parser.hpp>
#include <boost/system/error_code.hpp>
#include <cassobs/dbconnection_observations.h>
#include <cassandra.h>
#include <date/date.h>

#include "time_offseter.h"
#include "curl_wrapper.h"
#include "async_job_publisher.h"
#include "liveobjects/liveobjects_message.h"

namespace meteodata
{

using namespace meteodata;

/**
 * @brief Connector for the devices attached to the Liveobjects network, by
 * Orange®, using the Liveobjects API
 */
class LiveobjectsApiDownloader
{
public:
	/**
	 * @brief Construct the downloader
	 *
	 * @param station the station UUID (the Météodata station identifier)
	 * @param liveobjectsUrn the Liveobjects platform station identifier
	 * @param db the observations database to insert (meta-)data into
	 * @param apiKey the Liveobjects API key with privileges DATA_R
	 * @param jobPublisher an optional component able to scheduler recomputation
	 * 	of the climatology
	 */
	LiveobjectsApiDownloader(const CassUuid& station, const std::string& liveobjectsUrn,
						   DbConnectionObservations& db, const std::string& apiKey,
						   AsyncJobPublisher* jobPublisher = nullptr);

	/**
	 * @brief Download the archive since the last archive timestamp stored
	 * in database
	 *
	 * @param client the client used to download data, already connected and
	 * ready to read/write
	 */
	void download(CurlWrapper& client);

	/**
	 * @brief Download the archive since the date in parameter
	 *
	 * @param client the client used to download data, already connected and
	 * ready to read/write
	 * @param beginDate The beginning of the time range from which data should be downloaded
	 * @param endDate The end of the time range from which data should be downloaded
	 * @param force Whether to force retrieve the data even if the last available
	 * data from Liveobjects is not newer than what we already have in database
	 * @return True if the insertion of the values in the DB worked
	 */
	void download(CurlWrapper& client, const date::sys_seconds& beginDate,
		const date::sys_seconds& endDate, bool force=false);

	/**
	 * @brief The host name of the Liveobjects API server
	 */
	static constexpr char APIHOST[] = "liveobjects.orange-business.com";

	static const std::string BASE_URL;

	// 1 message every ten minutes over a day = 144 messages
	static constexpr int PAGE_SIZE = 200;

private:
	/**
	 * @brief The station id in Météodata
	 */
	CassUuid _station;

	/**
	 * @brief The URN of the station/sensor in Liveobjects
	 */
	 std::string _liveobjectsUrn;

	/**
	 * @brief The observations database (part Cassandra, part SQL) connector
	 */
	DbConnectionObservations& _db;

	/**
	 * @brief The component able to schedule recomputations of climatology over
	 * past data once it's downloaded
	 */
	AsyncJobPublisher* _jobPublisher;

	/**
	 * @brief The Liveobjects API key
	 *
	 * Requests to the API are authenticated by a simple header "X-API-key".
	 */
	const std::string& _apiKey;

	/**
	 * @brief The human-readable name given to the station
	 */
	std::string _stationName;

	/**
	 * @brief The last datetime for which data is stored in the Météodata
	 * database
	 */
	date::sys_seconds _lastArchive;

	/**
	 * @brief The max size reserved for the buffers used in the requests
	 */
	static constexpr size_t MAXSIZE = 1024 * 1024; // 1MiB

	static constexpr char SEARCH_ROUTE[] = "/v1/data/search/hits/";

	/**
	 * @brief Get the datetime of the last datapoint available from the
	 * Liveobjects SPOT API
	 *
	 * This method may throw or leave the socket closed, it's the caller's
	 * responsibility to check what state the socket is in.
	 *
	 * @param client the client used to download data, already connected and
	 * ready to read/write
	 *
	 * @return the timestamp of the last data available from the API
	 */
	date::sys_seconds getLastDatetimeAvailable(CurlWrapper& client);

	/**
	 * @brief Display the last error message from Curl and throw an
	 * exception
	 *
	 * @param client The Curl wrapper used to do HTTP queries
	 */
	void logAndThrowCurlError(CurlWrapper& client);
};

}

#endif
