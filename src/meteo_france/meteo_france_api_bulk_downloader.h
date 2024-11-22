/**
 * @file meteo_france_api_bulk_downloader.h
 * @brief Definition of the MeteoFranceApiDownloader class
 * @author Laurent Georget
 * @date 2024-01-16
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

#ifndef METEO_FRANCE_API_BULK_DOWNLOADER_H
#define METEO_FRANCE_API_BULK_DOWNLOADER_H

#include <chrono>
#include <map>
#include <string>
#include <tuple>
#include <memory>

#include <boost/property_tree/json_parser.hpp>
#include <boost/system/error_code.hpp>
#include <cassobs/dbconnection_observations.h>
#include <cassandra.h>
#include <date/date.h>

#include "curl_wrapper.h"
#include "async_job_publisher.h"

namespace meteodata
{

using namespace meteodata;

/**
 * @brief Connector for the Météo France stations, available thourhg their
 * observations and climatology API
 */
class MeteoFranceApiBulkDownloader
{
public:
	/**
	 * @brief Construct the downloader
	 *
	 * @param db the observations database to insert (meta-)data into
	 * @param apiKey the Météo France API key with appropriate privileges
	 * @param jobPublisher an optional component able to scheduler recomputation
	 * of the climatology
	 */
	MeteoFranceApiBulkDownloader(DbConnectionObservations& db,
		const std::string& apiKey,
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
	 * @brief The host name of the Météo France API server
	 */
	static constexpr char APIHOST[] = "public-api.meteofrance.fr";

	static const std::string BASE_URL;

	static constexpr std::chrono::milliseconds MIN_DELAY{1200};

private:
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

	std::map<std::string, CassUuid> _stations;

	static constexpr int DEPARTEMENTS[] = {
		1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18,
		19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34,
		35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50,
		51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66,
		67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82,
		83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95,
		971, 972, 973, 974, 975, 984, 985, 986, 987, 988
	};

	/**
	 * @brief The max size reserved for the buffers used in the requests
	 */
	static constexpr size_t MAXSIZE = 10 * 1024 * 1024; // 10MiB

	static constexpr char BULK_DOWNLOAD_ROUTE[] = "/public/DPPaquetObs/v1/paquet/horaire";

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
