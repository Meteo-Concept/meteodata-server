/**
 * @file meteo_france_api_downloader.cpp
 * @brief Implementation of the MeteoFranceApiDownloader class
 * @author Laurent Georget
 * @date 2023-04-10
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

#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <tuple>
#include <map>

#include <boost/system/error_code.hpp>
#include <boost/asio.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/json.hpp>
#include <dbconnection_observations.h>
#include <date.h>
#include <systemd/sd-daemon.h>
#include <cassandra.h>

#include "meteo_france/meteo_france_api_downloader.h"
#include "meteo_france/mf_radome_message.h"
#include "async_job_publisher.h"
#include "time_offseter.h"
#include "http_utils.h"
#include "curl_wrapper.h"
#include "cassandra_utils.h"

namespace meteodata
{

constexpr char MeteoFranceApiDownloader::APIHOST[];
constexpr char MeteoFranceApiDownloader::SEARCH_ROUTE[];
const std::string MeteoFranceApiDownloader::BASE_URL = std::string{"https://"} + MeteoFranceApiDownloader::APIHOST;

namespace asio = boost::asio;
namespace chrono = std::chrono;
namespace pt = boost::property_tree;
namespace json = boost::json;

using namespace meteodata;

MeteoFranceApiDownloader::MeteoFranceApiDownloader(const CassUuid& station, const std::string& mfId,
	DbConnectionObservations& db, const std::string& apiKey, AsyncJobPublisher* jobPublisher) :
		_station{station},
		_mfId{mfId},
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
	std::cout << SD_DEBUG << "[MF " << _station << "] connection: "
		  << "Discovered MF station " << _stationName << std::endl;
}

void MeteoFranceApiDownloader::download(CurlWrapper& client)
{
	download(client, _lastArchive, date::floor<chrono::seconds>(chrono::system_clock::now()));
}

void MeteoFranceApiDownloader::download(CurlWrapper& client, const date::sys_seconds& beginDate, const date::sys_seconds& endDate, bool force)
{
	std::cout << SD_INFO << "[MeteoFrance " << _station << "] measurement: "
			  << "Downloading historical data for MeteoFrance station " << _stationName << std::endl;

	// Form the request. We specify the "Connection: keep-alive" header so that the socket
	// will be usable by other downloaders.
	boost::asio::streambuf request;
	std::ostream requestStream(&request);

	using namespace date;
	std::cout << SD_DEBUG << "[MeteoFrance " << _station << "] management: " << "Last archive dates back from "
			  << _lastArchive << std::endl;

	bool insertionOk = true;
	date::sys_seconds oldest = _lastArchive;
	date::sys_seconds newest = _lastArchive;
	date::sys_seconds date = beginDate;
	do {
		client.setHeader("apikey", _apiKey);
		client.setHeader("Content-Type", "application/json");
		client.setHeader("Accept", "application/json");

		std::ostringstream osUrl;
		osUrl << SEARCH_ROUTE << "?"
		      << "id_station=" << client.escape(_mfId).get() << "&"
		      << "date=" << date::format("%FT%TZ", date) << "&"
		      << "format=json";

		std::cout << SD_DEBUG << "[MeteoFrance " << _station << "] protocol: "
				  << "GET " << osUrl.str() << " HTTP/1.1\n"
				  << "Host: " << APIHOST << "\n"
				  << "Accept: application/json\n";

		auto tick = chrono::system_clock::now();

		CURLcode ret = client.download(std::string{BASE_URL} + osUrl.str(),
				[&](const std::string& body) {
			try {
				std::istringstream responseStream(body);
				pt::ptree jsonTree;
				pt::read_json(responseStream, jsonTree);

				for (auto&& entry : jsonTree) { // we expect only one
					date::sys_seconds timestamp;
					MfRadomeMessage m;
					m.parse(std::move(entry.second), timestamp);
					if (m.looksValid()) {
						Observation o = m.getObservation(_station);
						int ret = _db.insertV2DataPoint(o) && _db.insertV2DataPointInTimescaleDB(o);
						if (!ret) {
							std::cerr << SD_ERR << "[MeteoFrance " << _station << "] measurement: "
									  << "Failed to insert archive observation for station " << _stationName << std::endl;
							insertionOk = false;
						} else {
							if (timestamp > newest) {
								newest = timestamp;
							}
						}
					}
				}
			} catch (const std::exception& e) {
				std::cerr << SD_ERR << "[MeteoFrance " << _station << "] protocol: "
						  << "Failed to receive or parse an MeteoFrance data message: " << e.what() << std::endl;
			}
		});

		if (ret != CURLE_OK) {
			logAndThrowCurlError(client);
		}

		auto elapsed = chrono::system_clock::now() - tick;
		if (elapsed < MIN_DELAY) {
			// cap at 50 requests / minute
			std::this_thread::sleep_for(MIN_DELAY - elapsed);
		}

		date += chrono::hours{1};
	} while (insertionOk && date < endDate);

	if (insertionOk) {
		std::cout << SD_DEBUG << "[MeteoFrance " << _station << "] measurement: "
				  << "Archive data stored for MeteoFrance station " << _stationName << std::endl;
		time_t lastArchiveDownloadTime = chrono::system_clock::to_time_t(newest);
		insertionOk = _db.updateLastArchiveDownloadTime(_station, lastArchiveDownloadTime);
		if (!insertionOk) {
			std::cerr << SD_ERR << "[MeteoFrance " << _station << "] management: "
					  << "couldn't update last archive download time for station " << _stationName << std::endl;
		} else {
			_lastArchive = newest;
		}

		if (_jobPublisher && newest > oldest) {
			_jobPublisher->publishJobsForPastDataInsertion(_station, oldest, newest);
		}
	}
}

void MeteoFranceApiDownloader::logAndThrowCurlError(CurlWrapper& client)
{
	std::string_view error = client.getLastError();
	std::ostringstream errorStream;
	errorStream << "MeteoFrance station " << _stationName << " Bad response from " << APIHOST << ": " << error;
	std::string errorMsg = errorStream.str();
	std::cerr << SD_ERR << "[MeteoFrance " << _station << "] protocol: " << errorMsg << std::endl;
	throw std::runtime_error(errorMsg);
}

}
