/**
 * @file objenious_api_downloader.cpp
 * @brief Implementation of the ObjeniousApiDownloader class
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

#include <string>
#include <tuple>
#include <map>

#include <boost/system/error_code.hpp>
#include <boost/asio.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <dbconnection_observations.h>
#include <date.h>
#include <systemd/sd-daemon.h>
#include <cassandra.h>

#include "objenious_api_downloader.h"
#include "objenious_archive_message_collection.h"
#include "objenious_archive_message.h"
#include "../time_offseter.h"
#include "../http_utils.h"
#include "../curl_wrapper.h"
#include "../cassandra_utils.h"

namespace meteodata
{

constexpr char ObjeniousApiDownloader::APIHOST[];
const std::string ObjeniousApiDownloader::BASE_URL = std::string{"https://"} + ObjeniousApiDownloader::APIHOST + "/v2";
constexpr size_t ObjeniousApiDownloader::PAGE_SIZE;

namespace asio = boost::asio;
namespace chrono = std::chrono;
namespace pt = boost::property_tree;

using namespace meteodata;

ObjeniousApiDownloader::ObjeniousApiDownloader(const CassUuid& station, const std::string& objeniousId,
											   const std::map<std::string, std::string>& variables,
											   DbConnectionObservations& db, const std::string& apiKey) :
		_station(station),
		_objeniousId(objeniousId),
		_variables(variables),
		_db(db),
		_apiKey(apiKey)
{
	time_t lastArchiveDownloadTime;
	db.getStationDetails(station, _stationName, _pollingPeriod, lastArchiveDownloadTime);
	float latitude, longitude;
	int elevation;
	db.getStationLocation(station, latitude, longitude, elevation);
	_lastArchive = date::sys_seconds(chrono::seconds(lastArchiveDownloadTime));
	std::cout << SD_DEBUG << "[Objenious " << _station << "] connection: " << "Discovered Objenious station "
			  << _stationName << std::endl;
}

date::sys_seconds ObjeniousApiDownloader::getLastDatetimeAvailable(CurlWrapper& client)
{
	std::cout << SD_INFO << "[Objenious " << _station << "] management: "
			  << "Checking if new data is available for Objenious station " << _stationName << std::endl;

	using namespace date;

	std::string route = "/devices/" + _objeniousId + "/state";

	std::cout << SD_DEBUG << "[Objenious " << _station << "] protocol: " << "GET " << route << " HTTP/1.1 "
			  << "Accept: application/json ";
	client.setHeader("apikey", _apiKey);
	client.setHeader("Accept", "application/json");

	date::sys_seconds dateInUTC;

	auto ret = client.download(BASE_URL + route, [&](const std::string& body) {
		std::istringstream bodyStream{body};
		pt::ptree jsonTree;
		pt::read_json(bodyStream, jsonTree);
		// use the first variable as a marker for new data, no need to
		// do anything more complicated
		const std::string& maxDate = jsonTree.get<std::string>("last_data_at." + _variables.begin()->second);
		std::istringstream dateStream{maxDate};
		dateStream >> date::parse("%FT%T%Z", dateInUTC);
	});

	if (ret != CURLE_OK) {
		logAndThrowCurlError(client);
	}

	return dateInUTC;
}

void ObjeniousApiDownloader::download(CurlWrapper& client)
{
	std::cout << SD_INFO << "[Objenious " << _station << "] measurement: "
			  << "Downloading historical data for Objenious station " << _stationName << std::endl;

	// Form the request. We specify the "Connection: keep-alive" header so that the socket
	// will be usable by other downloaders.
	boost::asio::streambuf request;
	std::ostream requestStream(&request);

	auto date = _lastArchive;

	// may throw
	date::sys_seconds lastAvailable = getLastDatetimeAvailable(client);
	if (lastAvailable <= _lastArchive) {
		std::cout << SD_DEBUG << "[Objenious " << _station << "] management: "
				  << "No new data available for Objenious station " << _stationName << ", bailing off" << std::endl;
		return;
	}

	using namespace date;
	std::cout << SD_DEBUG << "[Objenious " << _station << "] management: " << "Last archive dates back from "
			  << _lastArchive << "; last available is " << lastAvailable << "\n" << "(approximately "
			  << date::floor<date::days>(lastAvailable - date) << " days)" << std::endl;

	std::string cursor;
	bool mayHaveMore = date < lastAvailable;
	bool insertionOk = true;
	auto newestTimestamp = _lastArchive;
	while (mayHaveMore && insertionOk) {
		std::ostringstream routeBuilder;
		routeBuilder << "/devices/" << _objeniousId << "/values?" << "since=" << date::format("%FT%TZ", date) << "&"
					 << "until=" << date::format("%FT%TZ", lastAvailable) << "&" << "limit=" << PAGE_SIZE;
		if (!cursor.empty())
			routeBuilder << "&cursor=" << cursor;

		std::string route = routeBuilder.str();

		client.setHeader("apikey", _apiKey);
		client.setHeader("Accept", "application/json");

		std::cout << SD_DEBUG << "[Objenious " << _station << "] protocol: " << "GET " << "/v2" << route << " HTTP/1.1 "
				  << "Host: " << APIHOST << " " << "Accept: application/json ";

		CURLcode ret = client.download(BASE_URL + route, [&](const std::string& body) {
			std::istringstream responseStream(body);


			try {
				ObjeniousApiArchiveMessageCollection collection{&_variables};
				collection.parse(responseStream);

				auto newestTimestampInCollection = collection.getNewestMessageTime();
				// This condition is not true if we have several
				// pages to download because the most recent
				// timestamp will be found on the first page
				if (newestTimestampInCollection > newestTimestamp)
					newestTimestamp = newestTimestampInCollection;

				for (const ObjeniousApiArchiveMessage& m : collection) {
					int ret = _db.insertV2DataPoint(m.getObservation(_station));
					if (!ret) {
						std::cerr << SD_ERR << "[Objenious " << _station << "] measurement: "
								  << "Failed to insert archive observation for station " << _stationName << std::endl;
						insertionOk = false;
					}
				}

				if (collection.mayHaveMore()) {
					cursor = collection.getPaginationCursor();
				} else {
					mayHaveMore = false;
				}
			} catch (const std::exception& e) {
				std::cerr << SD_ERR << "[Objenious " << _station << "] protocol: "
						  << "Failed to receive or parse an Objenious data message: " << e.what() << std::endl;
			}
		});

		if (ret != CURLE_OK) {
			logAndThrowCurlError(client);
		}
	}

	if (insertionOk) {
		std::cout << SD_DEBUG << "[Objenious " << _station << "] measurement: "
				  << "Archive data stored for Objenious station" << _stationName << std::endl;
		time_t lastArchiveDownloadTime = chrono::system_clock::to_time_t(newestTimestamp);
		insertionOk = _db.updateLastArchiveDownloadTime(_station, lastArchiveDownloadTime);
		if (!insertionOk) {
			std::cerr << SD_ERR << "[Objenious " << _station << "] management: "
					  << "couldn't update last archive download time for station " << _stationName << std::endl;
		} else {
			_lastArchive = newestTimestamp;
		}
	}
}

void ObjeniousApiDownloader::logAndThrowCurlError(CurlWrapper& client)
{
	std::string_view error = client.getLastError();
	std::ostringstream errorStream;
	errorStream << "Objenious station " << _stationName << " Bad response from " << APIHOST << ": " << error;
	std::string errorMsg = errorStream.str();
	std::cerr << SD_ERR << "[Objenious " << _station << "] protocol: " << errorMsg << std::endl;
	throw std::runtime_error(errorMsg);
}

}
