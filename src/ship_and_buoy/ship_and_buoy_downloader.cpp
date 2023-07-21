/**
 * @file shipandbuoydownloader.cpp
 * @brief Implementation of the ShipAndBuoyDownloader class
 * @author Laurent Georget
 * @date 2019-01-16
 */
/*
 * Copyright (C) 2019  JD Environnement <contact@meteo-concept.fr>
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
#include <string>
#include <chrono>
#include <map>

#include <fstream>
#include <systemd/sd-daemon.h>
#include <boost/asio/io_context.hpp>
#include <dbconnection_observations.h>
#include <date.h>

#include "cassandra_utils.h"
#include "abstract_download_scheduler.h"
#include "ship_and_buoy/ship_and_buoy_downloader.h"
#include "ship_and_buoy/meteo_france_ship_and_buoy.h"

namespace chrono = std::chrono;

namespace meteodata
{
using namespace date;

ShipAndBuoyDownloader::ShipAndBuoyDownloader(asio::io_context& ioContext,
	DbConnectionObservations& db, AsyncJobPublisher* jobPublisher) :
		AbstractDownloadScheduler{chrono::hours(POLLING_PERIOD_HOURS), ioContext, db},
		_jobPublisher{jobPublisher}
{
	_status.shortStatus = "IDLE";
}

void ShipAndBuoyDownloader::download()
{
	std::cout << SD_NOTICE << "[SHIP] measurement: " << "Now downloading SHIP and BUOY data " << std::endl;
	auto ymd = date::year_month_day(date::floor<date::days>(chrono::system_clock::now() - date::days(1)));

	CURLcode ret = _client.download(std::string{"https://"} + HOST + date::format(URL, ymd),
								   [&](const std::string& body) {
		std::istringstream responseStream{body};

		std::string line;
		std::getline(responseStream, line);
		std::istringstream lineIterator{line};
		std::vector<std::string> fields;
		for (std::string field ; std::getline(lineIterator, field, ';') ;)
			   if (!field.empty())
				   fields.emplace_back(std::move(field));

		while (std::getline(responseStream, line)) {
			   lineIterator = std::istringstream{line};
			   MeteoFranceShipAndBuoy m{lineIterator, fields};
			   if (!m)
				   continue;
			   auto uuidIt = _icaos.find(m.getIdentifier());
			   if (uuidIt != _icaos.end()) {
				   std::cout << SD_DEBUG << "[SHIP " << uuidIt->second << "] protocol: "
							 << "UUID identified: " << uuidIt->second << std::endl;
				   auto obs = m.getObservation(uuidIt->second);
				   bool ret = _db.insertV2DataPoint(obs);
				   if (ret) {
					   std::cout << SD_DEBUG << "[SHIP " << uuidIt->second
								 << "] measurement: "
								 << "SHIP ou BUOY data inserted into database for station "
								 << uuidIt->second << std::endl;
				   } else {
					   std::cerr << SD_ERR << "[SHIP " << uuidIt->second
								 << "] measurement: "
								 << "Failed to insert SHIP ou BUOY data into database for station "
								 << uuidIt->second << std::endl;
				   }

				   if (_jobPublisher)
					   _jobPublisher->publishJobsForPastDataInsertion(uuidIt->second, obs.time, obs.time);
			   }
		}
	});

	if (ret != CURLE_OK) {
		std::string_view error = _client.getLastError();
		std::cerr << SD_ERR << "[SHIP] protocol: " << "Failed to download SHIP and BUOY data: " << error << std::endl;
	}
}

void ShipAndBuoyDownloader::reloadStations()
{
	_icaos.clear();
	std::vector<std::tuple<CassUuid, std::string>> icaos;
	_db.getAllIcaos(icaos);
	for (auto&& icao : icaos)
		_icaos.emplace(std::get<1>(icao), std::get<0>(icao));
}

}
