/**
 * @file meteo_france_api_bulk_downloader.cpp
 * @brief Implementation of the MeteoFranceApiBulkDownloader class
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
#include <cassobs/dbconnection_observations.h>
#include <date/date.h>
#include <systemd/sd-daemon.h>
#include <cassandra.h>

#include "meteo_france/meteo_france_api_bulk_downloader.h"
#include "meteo_france/mf_radome_message.h"
#include "async_job_publisher.h"
#include "http_utils.h"
#include "curl_wrapper.h"
#include "cassandra_utils.h"

namespace meteodata
{

const std::string MeteoFranceApiBulkDownloader::BASE_URL = std::string{"https://"} + MeteoFranceApiBulkDownloader::APIHOST;

namespace chrono = std::chrono;
namespace pt = boost::property_tree;

using namespace meteodata;

MeteoFranceApiBulkDownloader::MeteoFranceApiBulkDownloader(
	DbConnectionObservations& db, const std::string& apiKey,
	AsyncJobPublisher* jobPublisher) :
		_db{db},
		_jobPublisher{jobPublisher},
		_apiKey{apiKey}
{
	std::cout << SD_DEBUG << "[MeteoFrance Bulk] connection: initialized";
}

void MeteoFranceApiBulkDownloader::download(CurlWrapper& client)
{
	std::cout << SD_INFO << "[MeteoFrance Bulk] measurement: "
		<< "Downloading historical data for MeteoFrance stations" << std::endl;

	std::vector<std::tuple<CassUuid, std::string, std::string, int, float, float, int, int>> mfStations;
	_db.getMeteoFranceStations(mfStations);

	for (auto&& s : mfStations) {
		_stations.emplace(std::move(std::get<2>(s)), std::move(std::get<0>(s)));
	}

	// Form the request. We specify the "Connection: keep-alive" header so that the socket
	// will be usable by other downloaders.
	boost::asio::streambuf request;
	std::ostream requestStream(&request);

	bool insertionOk = true;

	for (int departement : DEPARTEMENTS) {
		client.setHeader("apikey", _apiKey);
		client.setHeader("Content-Type", "application/json");
		client.setHeader("Accept", "application/json");

		std::ostringstream osUrl;
		osUrl << BULK_DOWNLOAD_ROUTE << "?"
		      << "id-departement=" << departement << "&"
		      << "format=json";

		std::cout << SD_DEBUG << "[MeteoFrance Bulk] protocol: "
				  << "GET " << osUrl.str() << " HTTP/1.1\n"
				  << "Host: " << APIHOST << "\n"
				  << "Accept: application/json\n";

		std::vector<Observation> allObs;

		CURLcode ret = client.download(std::string{BASE_URL} + osUrl.str(),
				[&](const std::string& body) {
			try {
				std::istringstream responseStream(body);
				pt::ptree jsonTree;
				pt::read_json(responseStream, jsonTree);

				for (auto&& entry : jsonTree) {
					date::sys_seconds timestamp;
					MfRadomeMessage m;
					m.parse(std::move(entry.second), timestamp);
					std::string mfId = m.getMfId();
					auto st = _stations.find(mfId);
					if (m.looksValid() && st != _stations.end()) {
						auto&& station = st->second;
						Observation o = m.getObservation(station);
						allObs.push_back(o);
						int ret = _db.insertV2DataPoint(o);
						if (!ret) {
							std::cerr << SD_ERR << "[MeteoFrance " << station << "] measurement: "
								  << "Failed to insert archive observation for station " << station << std::endl;
							insertionOk = false;
						}
					}
				}
			} catch (const std::exception& e) {
				std::cerr << SD_ERR << "[MeteoFrance Bulk] protocol: "
					  << "Failed to receive or parse an MeteoFrance data message: " << e.what() << std::endl;
			}
		});


		if (ret != CURLE_OK) {
			logAndThrowCurlError(client);
		}

		bool inserted = _db.insertV2DataPointsInTimescaleDB(allObs.begin(), allObs.end());
		if (!inserted) {
			std::cerr << SD_ERR << "[MeteoFrance Bulk] measurement: "
				  << "Failed to insert entries in TimescaleDB" << std::endl;
		}

		// cap at 50 requests / minute
		std::this_thread::sleep_for(MIN_DELAY);
	}

	if (insertionOk) {
		std::cout << SD_INFO << "[MeteoFrance Bulk] measurement: "
			  << "Archive data stored" << std::endl;
	}
}

void MeteoFranceApiBulkDownloader::logAndThrowCurlError(CurlWrapper& client)
{
	std::string_view error = client.getLastError();
	std::ostringstream errorStream;
	errorStream << "MeteoFrance Bulk" << " Bad response from " << APIHOST << ": " << error;
	std::string errorMsg = errorStream.str();
	std::cerr << SD_ERR << "[MeteoFrance Bulk] protocol: " << errorMsg << std::endl;
	throw std::runtime_error(errorMsg);
}

}
