/**
 * @file objenious_api_downloader.h
 * @brief Definition of the ObjeniousApiDownloader class
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

#ifndef OBJENIOUS_API_DOWNLOADER_H
#define OBJENIOUS_API_DOWNLOADER_H

#include <map>
#include <string>
#include <tuple>

#include <boost/system/error_code.hpp>
#include <dbconnection_observations.h>
#include <cassandra.h>

#include "../time_offseter.h"
#include "../curl_wrapper.h"

namespace meteodata {

using namespace meteodata;

/**
 * @brief Connector for the devices attached to the Objenious network, by
 * Bouygues®, using the Objenious SPOT API
 */
class ObjeniousApiDownloader
{
public:
	/**
	 * @brief Construct the downloader
	 *
	 * @param station the station UUID (the Météodata station identifier)
	 * @param spotId the SPOT platform station identifier
	 * @param sensors the map of sensors (it maps meteorological variable
	 * names like "temperature") to SPOT variable id in the "data" field
	 * (like "temperature", depending on the sensors installed)
	 * @param db the observations database to insert (meta-)data into
	 * @param apiId the Objenious API key public part
	 * @param apiSecret the Objenious API key private part
	 */
	ObjeniousApiDownloader(const CassUuid& station,
		const std::string& fieldclimateId,
		const std::map<std::string, std::string>& variables,
		DbConnectionObservations& db,
		const std::string& apiId);

	/**
	 * @brief Download the archive since the last archive timestamp stored
	 * in database
	 *
	 * @param client the client used to download data, already connected and
	 * ready to read/write
	 */
	void download(CurlWrapper& client);

	/**
	 * @brief The host name of the FieldClimate API server
	 */
	static constexpr char APIHOST[] = "api.objenious.com";

	static const std::string BASE_URL;

	/**
	 * @brief The page size of data
	 */
	static constexpr size_t PAGE_SIZE = 50;

private:
	/**
	 * @brief The station id in Météodata
	 */
	CassUuid _station;

	/**
	 * @brief The FieldClimate station id
	 */
	std::string _objeniousId;

	/**
	 * @brief The sensors known to be available for this station
	 *
	 * This maps meteorological variable names like "temperature") to
	 * SPOT variables id (like "temperature", used as key in the data field
	 * of the messages answers, see the documentation at
	 * https://api.objenious.com/doc/doc-swagger-v2.html#operation/getValues).
	 * The key is used in the Météodata databases, the value in the response
	 * from the SPOT API.
	 */
	std::map<std::string, std::string> _variables;

	/**
	 * @brief The observations database (part Cassandra, part SQL) connector
	 */
	DbConnectionObservations& _db;

	/**
	 * @brief The SPOT API key
	 *
	 * Requests to the API are authenticated by a simple very much
	 * non-standard and not W3C-compliant header "apikey".
	 */
	const std::string& _apiKey;

	/**
	 * @brief The human-readable name given to the station
	 */
	std::string _stationName;

	/**
	 * @brief The period at which data points are available
	 *
	 * TODO this is currently not used, we get data at a fix interval, in
	 * the ObjeniousApiDownloadScheduler class.
	 */
	int _pollingPeriod;

	/**
	 * @brief The last datetime for which data is stored in the Météodata
	 * database
	 */
	date::sys_seconds _lastArchive;

	/**
	 * @brief The max size reserved for the buffers used in the requests
	 */
	static constexpr size_t MAXSIZE = 1024 * 1024; // 1MiB

	/**
	 * @brief Get the datetime of the last datapoint available from the
	 * Objenious SPOT API
	 *
	 * This method may throw or leave the socket closed, it's the caller's
	 * responsability to check what state the socket is in.
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
