/**
 * @file meteo_france_api_downloader.h
 * @brief Definition of the MeteoFranceApiDownloader class
 * @author Laurent Georget
 * @date 2024-01-15
 */
/*
 * Copyright (C) 2024  SAS JD Environnement <contact@meteo-concept.fr>
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

#ifndef METEO_FRANCE_API_DOWNLOADER_H
#define METEO_FRANCE_API_DOWNLOADER_H

#include <chrono>
#include <map>
#include <string>
#include <tuple>
#include <memory>

#include <boost/property_tree/json_parser.hpp>
#include <boost/system/error_code.hpp>
#include <dbconnection_observations.h>
#include <cassandra.h>
#include <date.h>

#include "time_offseter.h"
#include "curl_wrapper.h"
#include "async_job_publisher.h"
#include "liveobjects/liveobjects_message.h"

namespace meteodata
{

using namespace meteodata;

/**
 * @brief Connector for the Météo France stations, available thourhg their
 * observations and climatology API
 */
class MeteoFranceApiDownloader
{
public:
	/**
	 * @brief Construct the downloader
	 *
	 * @param station the station UUID (the MétéoData station identifier)
	 * @param mfId the Météo France identifier (DDCCCNNN where DD is the
	 * department number, CCC the city INSEE number and NNN the station
	 * serial number inside the city)
	 * @param db the observations database to insert (meta-)data into
	 * @param apiKey the Météo France API key with appropriate privileges
	 * @param jobPublisher an optional component able to scheduler recomputation
	 * of the climatology
	 */
	MeteoFranceApiDownloader(const CassUuid& station, const std::string& mfId,
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
	 * @brief The host name of the Météo France API server
	 */
	static constexpr char APIHOST[] = "public-api.meteofrance.fr";

	static const std::string BASE_URL;

	static constexpr std::chrono::milliseconds MIN_DELAY{2500};

private:
	/**
	 * @brief The station id in Météodata
	 */
	CassUuid _station;

	/**
	 * @brief The Météo France identifier of the station
	 */
	 std::string _mfId;

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
	 * @brief The Météo France API key
	 *
	 * Requests to the API are authenticated by a token passed by a apikey
	 * header (or alternatively a OAuth2 token but for read-only queries, is
	 * it useful?)
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
	static constexpr size_t MAXSIZE = 10 * 1024 * 1024; // 10MiB

	static constexpr char SEARCH_ROUTE[] = "/public/DPObs/v1/station/horaire";

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
