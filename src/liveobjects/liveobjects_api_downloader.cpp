/**
 * @file liveobjects_api_downloader.cpp
 * @brief Implementation of the LiveobjectsApiDownloader class
 * @author Laurent Georget
 * @date 2023-04-10
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

#include <string>
#include <tuple>
#include <map>

#include <boost/system/error_code.hpp>
#include <boost/asio.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/json.hpp>
#include <cassobs/dbconnection_observations.h>
#include <date/date.h>
#include <systemd/sd-daemon.h>
#include <cassandra.h>

#include "liveobjects_api_downloader.h"
#include "liveobjects_message.h"
#include "time_offseter.h"
#include "http_utils.h"
#include "curl_wrapper.h"
#include "cassandra_utils.h"

namespace meteodata
{

constexpr char LiveobjectsApiDownloader::APIHOST[];
constexpr char LiveobjectsApiDownloader::SEARCH_ROUTE[];
const std::string LiveobjectsApiDownloader::BASE_URL = std::string{"https://"} + LiveobjectsApiDownloader::APIHOST + "/api";
constexpr int LiveobjectsApiDownloader::PAGE_SIZE;

namespace asio = boost::asio;
namespace chrono = std::chrono;
namespace pt = boost::property_tree;
namespace json = boost::json;

using namespace meteodata;

LiveobjectsApiDownloader::LiveobjectsApiDownloader(const CassUuid& station, const std::string& liveobjectsId,
	DbConnectionObservations& db, const std::string& apiKey, AsyncJobPublisher* jobPublisher) :
		_station{station},
		_liveobjectsUrn{liveobjectsId},
		_db{db},
		_jobPublisher{jobPublisher},
		_apiKey{apiKey}
{
	time_t lastArchiveDownloadTime;
	int pollingPeriod;
	db.getStationDetails(station, _stationName, pollingPeriod, lastArchiveDownloadTime);
	float latitude, longitude;
	int elevation;
	db.getStationLocation(station, latitude, longitude, elevation);
	_lastArchive = date::sys_seconds(chrono::seconds(lastArchiveDownloadTime));
	std::cout << SD_DEBUG << "[Liveobjects " << _station << "] connection: "
			  << "Discovered Liveobjects station " << _stationName << std::endl;
}

date::sys_seconds LiveobjectsApiDownloader::getLastDatetimeAvailable(CurlWrapper& client)
{
	std::cout << SD_INFO << "[Liveobjects " << _station << "] management: "
			  << "Checking if new data is available for Liveobjects station " << _stationName << std::endl;

	using namespace date;

	std::string route = "/v1/deviceMgt/devices/" + _liveobjectsUrn + "/data/streams";

	std::cout << SD_DEBUG << "[Liveobjects " << _station << "] protocol: " << "GET " << route << " HTTP/1.1 "
			  << "Accept: application/json\n";
	client.setHeader("X-API-Key", _apiKey);
	client.setHeader("Accept", "application/json");

	date::sys_seconds dateInUTC;

	auto ret = client.download(BASE_URL + route, [&](const std::string& body) {
		std::istringstream bodyStream{body};
		pt::ptree jsonTree;
		pt::read_json(bodyStream, jsonTree);

		if (jsonTree.begin() == jsonTree.end())
			return;

		// use the first entry (there should be one in the general case anyway)
		const std::string& maxDate = jsonTree.begin()->second.get<std::string>("lastUpdate", "1970-01-01T00:00:00.000Z");
		std::istringstream dateStream{maxDate};
		std::cerr << "last available: " << maxDate << std::endl;
		// the date library won't parse the decimal part of the seconds on its
		// own since we store the result in a date::sys_seconds variable
		// so force parse the decimal part
		dateStream >> date::parse("%FT%H:%M:%6S%Z", dateInUTC);
	});

	if (ret != CURLE_OK) {
		logAndThrowCurlError(client);
	}

	return dateInUTC;
}

void LiveobjectsApiDownloader::download(CurlWrapper& client)
{
	download(client, _lastArchive, date::floor<chrono::seconds>(chrono::system_clock::now()));
}

void LiveobjectsApiDownloader::download(CurlWrapper& client, const date::sys_seconds& beginDate, const date::sys_seconds& endDate, bool force)
{
	std::cout << SD_INFO << "[Liveobjects " << _station << "] measurement: "
			  << "Downloading historical data for Liveobjects station " << _stationName << std::endl;

	// Form the request. We specify the "Connection: keep-alive" header so that the socket
	// will be usable by other downloaders.
	boost::asio::streambuf request;
	std::ostream requestStream(&request);

	// may throw
	date::sys_seconds lastAvailable = getLastDatetimeAvailable(client);
	if (lastAvailable <= _lastArchive && !force) {
		std::cout << SD_DEBUG << "[Liveobjects " << _station << "] management: "
				  << "No new data available for Liveobjects station " << _stationName << ", bailing off" << std::endl;
		return;
	}

	using namespace date;
	std::cout << SD_DEBUG << "[Liveobjects " << _station << "] management: " << "Last archive dates back from "
			  << _lastArchive << "; last available is " << lastAvailable << "\n" << "(approximately "
			  << date::floor<date::days>(lastAvailable - _lastArchive) << " days)" << std::endl;

	bool insertionOk = true;
	date::sys_seconds newest = _lastArchive;
	date::sys_seconds oldest = date::floor<chrono::seconds>(chrono::system_clock::now());
	date::sys_seconds date = beginDate;
	do {
		date::sys_seconds datep1 = date + chrono::hours{24}; // about right

		std::ostringstream osDate;
		osDate << date::format("%FT%TZ", date);
		std::ostringstream osDatep1;
		osDatep1 << date::format("%FT%TZ", datep1);

		json::object body{
			{ "size", PAGE_SIZE },
			{ "query", json::object{
				{ "bool", json::object{
					{ "must", json::array{ {
						{ "term", {
							{ "streamId", _liveobjectsUrn }
						} }
					} } },
					{ "filter", json::array{ {
						{ "range", {
							{ "timestamp", {
								{ "gt", osDate.str() },
								{ "lte", osDatep1.str() }
							} }
						} }
					} } }
				} }
			} },
			{ "sort", json::array{ {
				{ "timestamp", {
					{ "order", "asc" }
				} }
			} } }
		};


		client.setHeader("X-API-Key", _apiKey);
		client.setHeader("Content-Type", "application/json");
		client.setHeader("Accept", "application/json");

		std::cout << SD_DEBUG << "[Liveobjects " << _station << "] protocol: "
				  << "POST " << SEARCH_ROUTE << " HTTP/1.1\n"
				  << "Host: " << APIHOST << "\n"
				  << "Accept: application/json\n"
				  << body << "\n";

		CURLcode ret = client.post(std::string{BASE_URL} + SEARCH_ROUTE,
				json::serialize(body),
				[&](const std::string& body) {
			try {
				std::istringstream responseStream(body);
				pt::ptree jsonTree;
				pt::read_json(responseStream, jsonTree);

				for (auto&& entry : jsonTree) {
					date::sys_seconds timestamp;
					auto m = LiveobjectsMessage::parseMessage(_db, entry.second, _station, timestamp);
					if (m && m->looksValid()) {
						auto o = m->getObservation(_station);
						int ret = _db.insertV2DataPoint(o)
						       && _db.insertV2DataPointInTimescaleDB(o);
						if (!ret) {
							std::cerr << SD_ERR << "[Liveobjects " << _station << "] measurement: "
									  << "Failed to insert archive observation for station " << _stationName << std::endl;
							insertionOk = false;
						} else {
							m->cacheValues(_station);
							if (timestamp > newest) {
								newest = timestamp;
							}
							if (timestamp < oldest) {
								oldest = timestamp;
							}
						}
					}
				}
				if (date >= newest) {
					// we've not made any progress, force advance the date in
					// order not to keep stuck
					date = datep1;
				} else {
					date = newest;
				}
			} catch (const std::exception& e) {
				std::cerr << SD_ERR << "[Liveobjects " << _station << "] protocol: "
						  << "Failed to receive or parse an Liveobjects data message: " << e.what() << std::endl;
			}
		});

		if (ret != CURLE_OK) {
			logAndThrowCurlError(client);
		}
	} while (insertionOk && date < endDate);

	if (insertionOk) {
		std::cout << SD_DEBUG << "[Liveobjects " << _station << "] measurement: "
				  << "Archive data stored for Liveobjects station" << _stationName << std::endl;
		time_t lastArchiveDownloadTime = chrono::system_clock::to_time_t(newest);
		insertionOk = _db.updateLastArchiveDownloadTime(_station, lastArchiveDownloadTime);
		if (!insertionOk) {
			std::cerr << SD_ERR << "[Liveobjects " << _station << "] management: "
					  << "couldn't update last archive download time for station " << _stationName << std::endl;
		} else {
			_lastArchive = newest;
		}

		if (_jobPublisher) {
			_jobPublisher->publishJobsForPastDataInsertion(_station, oldest, newest);
		}
	}
}

void LiveobjectsApiDownloader::logAndThrowCurlError(CurlWrapper& client)
{
	std::string_view error = client.getLastError();
	std::ostringstream errorStream;
	errorStream << "Liveobjects station " << _stationName << " Bad response from " << APIHOST << ": " << error;
	std::string errorMsg = errorStream.str();
	std::cerr << SD_ERR << "[Liveobjects " << _station << "] protocol: " << errorMsg << std::endl;
	throw std::runtime_error(errorMsg);
}

}
