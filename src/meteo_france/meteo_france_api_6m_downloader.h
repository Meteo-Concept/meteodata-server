/**
 * @file meteo_france_api_6m_downloader.h
 * @brief Definition of the MeteoFranceApiDownloader class
 * @author Laurent Georget
 * @date 2024-02-23
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

#ifndef METEO_FRANCE_API_6M_DOWNLOADER_H
#define METEO_FRANCE_API_6M_DOWNLOADER_H

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

#include "curl_wrapper.h"
#include "async_job_publisher.h"

namespace meteodata
{

using namespace meteodata;

/**
 * @brief Connector for the Météo France stations, available thourhg their
 * observations and climatology API
 */
class MeteoFranceApi6mDownloader
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
	MeteoFranceApi6mDownloader(DbConnectionObservations& db,
		const std::string& apiKey,
		AsyncJobPublisher* jobPublisher = nullptr);

	/**
	 * @brief Download the archive since the last archive timestamp stored
	 * in database
	 *
	 * @param client the client used to download data, already connected and
	 * ready to read/write
	 */
	void download(CurlWrapper& client, date::sys_seconds d = date::floor<chrono::seconds>(std::chrono::system_clock::now()));

	/**
	 * @brief The host name of the Météo France API server
	 */
	static constexpr char APIHOST[] = "public-api.meteofrance.fr";

	static const std::string BASE_URL;

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

	/**
	 * @brief The max size reserved for the buffers used in the requests
	 */
	static constexpr size_t MAXSIZE = 10 * 1024 * 1024; // 10MiB

	static constexpr char DOWNLOAD_ROUTE[] = "/public/DPPaquetObs/v1/paquet/stations/infrahoraire-6m";

	using UpdatePeriod = std::chrono::duration<int, std::ratio_multiply<std::ratio<6>, std::chrono::minutes::period>>;

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
