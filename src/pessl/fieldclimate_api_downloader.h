/**
 * @file pessl_apiv2_downloader.h
 * @brief Definition of the FieldClimateApiDownloader class
 * @author Laurent Georget
 * @date 2020-09-01
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

#ifndef FIELDCLIMATE_API_DOWNLOADER_H
#define FIELDCLIMATE_API_DOWNLOADER_H

#include <map>
#include <string>
#include <tuple>

#include <boost/system/error_code.hpp>
#include <dbconnection_observations.h>
#include <cassandra.h>

#include "../time_offseter.h"
#include "../curl_wrapper.h"

namespace meteodata
{

using namespace meteodata;

/**
 * @brief Connector for the Pessl® stations, using the FieldClimate API
 *
 * The API version implemented by this connector is the v2.
 */
class FieldClimateApiDownloader
{
public:
	/**
	 * @brief Construct the downloader
	 *
	 * @param station the station UUID (the Météodata station identifier)
	 * @param fieldclimateId the FieldClimate station identifier
	 * @param sensors the map of sensors (it maps meteorological variable
	 * names like "temperature") to FieldClimate sensors id (like
	 * "1_X_X_143", see the documentation)
	 * @param db the observations database to insert (meta-)data into
	 * @param tz the timezone identifier (the values are stored in the
	 * database and listed in the TimeOffseter class)
	 * @param apiId the FieldClimate API key public part
	 * @param apiSecret the FieldClimate API key private part
	 */
	FieldClimateApiDownloader(const CassUuid& station, std::string fieldclimateId,
		std::map<std::string, std::string> sensors, DbConnectionObservations& db,
		TimeOffseter::PredefinedTimezone tz, std::string apiId, std::string apiSecret);

	/**
	 * @brief Download the archive since the last archive timestamp stored
	 * in database
	 *
	 * @param client the client used to download data, already connected and
	 * ready to read/write
	 */
	void download(CurlWrapper& client);

	/**
	 * @brief Download the last data packet available in FieldClimate
	 *
	 * @param client the client used to download data, already connected and
	 * ready to read/write
	 */
	void downloadRealTime(CurlWrapper& client);

	/**
	 * @brief The host name of the FieldClimate API server
	 */
	static constexpr char APIHOST[] = "api.fieldclimate.com";
	static constexpr char BASE_URL[] = "https://api.fieldclimate.com/v2";

private:
	/**
	 * @brief The station id in Météodata
	 */
	CassUuid _station;

	/**
	 * @brief The FieldClimate station id
	 */
	std::string _fieldclimateId;

	/**
	 * @brief The sensors known to be available for this station
	 *
	 * This maps meteorological variable names like "temperature") to
	 * FieldClimate sensors id (like "1_X_X_143", see the documentation like
	 * https://api.fieldclimate.com/v2/docs/#data-get-data-between-period).
	 * The key is used in the Météodata databases, the value in the response
	 * from the FieldClimate API.
	 */
	std::map<std::string, std::string> _sensors;

	/**
	 * @brief The observations database (part Cassandra, part SQL) connector
	 */
	DbConnectionObservations& _db;

	/**
	 * @brief A convenient object to perform datetime conversions because
	 * the FieldClimate API returns times in the station's local timezone)
	 */
	TimeOffseter _timeOffseter;

	/**
	 * @brief The public part of the FieldClimate API key
	 *
	 * Requests to the API are authenticated by a HMAC signature, computed
	 * from an public id string and a private key.
	 *
	 * @see _apiSecret
	 */
	const std::string& _apiKey;

	/**
	 * @brief The secret part of the FieldClimate API key
	 * @see _apiKey
	 */
	const std::string& _apiSecret;

	/**
	 * @brief The human-readable name given to the station
	 */
	std::string _stationName;

	/**
	 * @brief The period at which data points are available
	 *
	 * TODO this is currently not used, we get data at a fix interval, in
	 * the FieldClimateApiDownloadScheduler class.
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
	 * @brief Compute the HTTP field that will authenticate the query
	 *
	 * Together with the API pair of keys and the current time, the two
	 * parameters allow to compute the Authorization: and Date: HTTP headers
	 * used by the FieldClimate server to authenticate the query.
	 *
	 * @param method The HTTP method (GET, POST, etc.)
	 * @param route The exact path to the resource (with all fields
	 * populated)
	 *
	 * @return A tuple of two strings, the first one being the
	 * Authorization: header and the second one the Date: header.
	 */
	std::tuple<std::string, std::string>
	computeAuthorizationAndDateFields(const std::string& method, const std::string& route);

	/**
	 * @brief Get the datetime of the last datapoint available from the
	 * FieldClimate API
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
