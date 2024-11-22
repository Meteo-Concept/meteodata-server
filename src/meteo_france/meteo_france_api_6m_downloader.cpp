/**
 * @file meteo_france_api_6m_downloader.cpp
 * @brief Implementation of the MeteoFranceApi6mDownloader class
 * @author Laurent Georget
 * @date 2026-02-23
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

#include <chrono>
#include <iostream>
#include <ratio>
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

#include "meteo_france/meteo_france_api_6m_downloader.h"
#include "meteo_france/meteo_france_api_downloader.h"
#include "meteo_france/mf_radome_message.h"
#include "async_job_publisher.h"
#include "http_utils.h"
#include "curl_wrapper.h"
#include "cassandra_utils.h"

namespace meteodata
{

const std::string MeteoFranceApi6mDownloader::BASE_URL = std::string{"https://"} + MeteoFranceApi6mDownloader::APIHOST;

namespace chrono = std::chrono;
namespace pt = boost::property_tree;

using namespace meteodata;

MeteoFranceApi6mDownloader::MeteoFranceApi6mDownloader(
	DbConnectionObservations& db, const std::string& apiKey,
	AsyncJobPublisher* jobPublisher) :
		_db{db},
		_jobPublisher{jobPublisher},
		_apiKey{apiKey}
{
	std::cout << SD_DEBUG << "[MeteoFrance 6m] connection: initialized";
}

void MeteoFranceApi6mDownloader::download(CurlWrapper& client, date::sys_seconds d)
{
	std::cout << SD_INFO << "[MeteoFrance 6m] measurement: "
		<< "Downloading last data for MeteoFrance stations" << std::endl;

	std::vector<std::tuple<CassUuid, std::string, std::string, int, float, float, int, int>> mfStations;
	_db.getMeteoFranceStations(mfStations);

	for (auto&& s : mfStations) {
		_stations.emplace(std::move(std::get<2>(s)), std::move(std::get<0>(s)));
	}

	bool insertionOk = true;

	std::ostringstream osUrl;
	osUrl << DOWNLOAD_ROUTE << "?"
	      << "date=" << date::format("%Y-%m-%dT%H:%M:00Z", date::floor<UpdatePeriod>(d)) << "&"
	      << "format=json";
	std::string url = osUrl.str();

	std::cout << SD_DEBUG << "[MeteoFrance 6m] protocol: "
		  << "GET " << url << " HTTP/1.1\n"
		  << "Host: " << APIHOST << "\n"
		  << "Accept: application/json\n";

	bool success = false;
	CURLcode ret = CURLE_OK;
	int tries = 0;
	for (; tries < 3 && !success ; tries++) {
		client.setHeader("apikey", _apiKey);
		client.setHeader("Content-Type", "application/json");
		client.setHeader("Accept", "application/json");
		ret = client.download(std::string{BASE_URL} + url,
				[&](const std::string& body) {
			try {
				std::istringstream responseStream(body);
				pt::ptree jsonTree;
				pt::read_json(responseStream, jsonTree);

				std::vector<Observation> obs;

				for (auto&& entry : jsonTree) {
					date::sys_seconds timestamp;
					MfRadomeMessage m{UpdatePeriod{1}};
					m.parse(std::move(entry.second), timestamp);
					std::string mfId = m.getMfId();
					auto st = _stations.find(mfId);
					if (m.looksValid() && st != _stations.end()) {
						auto&& station = st->second;
						auto o = m.getObservation(station);
						obs.push_back(o);
						int ret = _db.insertV2DataPoint(o);
						if (!ret) {
							std::cerr << SD_ERR << "[MeteoFrance] measurement: "
								  << "Failed to insert archive observation in Cassandra" << std::endl;
							insertionOk = false;
						}
					}
				}

				int ret = _db.insertV2DataPointsInTimescaleDB(obs.begin(), obs.end());
				if (!ret) {
					std::cerr << SD_ERR << "[MeteoFrance] measurement: "
						  << "Failed to insert archive observation" << std::endl;
					insertionOk = false;
				}
				success = true;
			} catch (const std::exception& e) {
				std::cerr << SD_ERR << "[MeteoFrance 6m] protocol: "
					  << "Failed to receive or parse an MeteoFrance data message: " << e.what() << std::endl;
			}
		});
		std::this_thread::sleep_for(MeteoFranceApiDownloader::MIN_DELAY);
	}

	if (ret != CURLE_OK) {
		logAndThrowCurlError(client);
	} else if (tries > 1) {
		std::cout << SD_WARNING << "[MeteoFrance 6m] measurement: "
			  << "Data downloaded after " << tries << " failures" << std::endl;
	}

	if (insertionOk) {
		std::cout << SD_DEBUG << "[MeteoFrance 6m] measurement: "
			  << "Archive data stored" << std::endl;
	}
}

void MeteoFranceApi6mDownloader::logAndThrowCurlError(CurlWrapper& client)
{
	std::string_view error = client.getLastError();
	std::ostringstream errorStream;
	errorStream << "MeteoFrance 6m" << " Bad response from " << APIHOST << ": " << error;
	std::string errorMsg = errorStream.str();
	std::cerr << SD_ERR << "[MeteoFrance 6m] protocol: " << errorMsg << std::endl;
	throw std::runtime_error(errorMsg);
}

}
