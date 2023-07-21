/**
 * @file fieldclimate_api_download_scheduler.cpp
 * @brief Implementation of the FieldClimateApiDownloadScheduler class
 * @author Laurent Georget
 * @date 2020-09-02
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

#include <iostream>
#include <map>
#include <chrono>
#include <thread>
#include <utility>

#include <systemd/sd-daemon.h>
#include <cassandra.h>
#include <dbconnection_observations.h>

#include "time_offseter.h"
#include "abstract_download_scheduler.h"
#include "async_job_publisher.h"
#include "pessl/fieldclimate_api_download_scheduler.h"
#include "pessl/fieldclimate_api_downloader.h"

namespace chrono = std::chrono;

namespace meteodata
{

using namespace date;

FieldClimateApiDownloadScheduler::FieldClimateApiDownloadScheduler(asio::io_context& ioContext,
	DbConnectionObservations& db, std::string apiId, std::string apiSecret,
	AsyncJobPublisher* jobPublisher) :
		AbstractDownloadScheduler{chrono::minutes{POLLING_PERIOD}, ioContext, db},
		_apiId{std::move(apiId)},
		_apiSecret{std::move(apiSecret)},
		_jobPublisher{jobPublisher}
{
}

void FieldClimateApiDownloadScheduler::add(const CassUuid& station, const std::string& fieldClimateId,
	TimeOffseter::PredefinedTimezone tz, const std::map<std::string, std::string>& sensors)
{
	_downloaders.emplace_back(
		std::make_shared<FieldClimateApiDownloader>(station, fieldClimateId, sensors, _db, tz, _apiId, _apiSecret, _jobPublisher)
	);
}

void FieldClimateApiDownloadScheduler::download()
{
	++_status.nbDownloads;
	_status.lastDownload = date::floor<chrono::seconds>(chrono::system_clock::now());
	for (const auto& downloader : _downloaders) {
		try {
			downloader->download(_client);
			// Wait for 100ms to limit the number of requests
			// (capped at 10 per second)
			std::this_thread::sleep_for(chrono::milliseconds(100));
		} catch (const std::runtime_error& e) {
			std::cerr << SD_ERR << "[Pessl] protocol: " << "Runtime error, impossible to download " << e.what()
					  << ", moving on..." << std::endl;
		}
	}
}

void FieldClimateApiDownloadScheduler::reloadStations()
{
	_downloaders.clear();

	std::vector<std::tuple<CassUuid, std::string, int, std::map<std::string, std::string>>> fieldClimateStations;
	_db.getAllFieldClimateApiStations(fieldClimateStations);
	for (const auto& station : fieldClimateStations) {
		add(std::get<0>(station), std::get<1>(station), TimeOffseter::PredefinedTimezone(std::get<2>(station)),
			std::get<3>(station));
	}
}

}
