/**
 * @file objenious_api_download_scheduler.cpp
 * @brief Implementation of the ObjeniousApiDownloadScheduler class
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

#include <iostream>
#include <chrono>
#include <thread>

#include <systemd/sd-daemon.h>
#include <boost/system/error_code.hpp>
#include <boost/asio/basic_waitable_timer.hpp>
#include <cassandra.h>
#include <dbconnection_observations.h>

#include "objenious_api_download_scheduler.h"
#include "objenious_api_downloader.h"
#include "../abstract_download_scheduler.h"

namespace chrono = std::chrono;

namespace meteodata
{
using namespace date;

ObjeniousApiDownloadScheduler::ObjeniousApiDownloadScheduler(asio::io_context& ioContext, DbConnectionObservations& db,
	std::string apiKey) :
		AbstractDownloadScheduler{chrono::minutes{POLLING_PERIOD},ioContext, db},
		_apiKey{std::move(apiKey)}
{
}

void ObjeniousApiDownloadScheduler::add(const CassUuid& station, const std::string& fieldClimateId,
	const std::map<std::string, std::string>& variables)
{
	_downloaders.emplace_back(std::make_shared<ObjeniousApiDownloader>(station, fieldClimateId, variables, _db, _apiKey));
}

void ObjeniousApiDownloadScheduler::download()
{
	for (const auto & _downloader : _downloaders) {
		try {
			_downloader->download(_client);
			// Wait for 100ms to limit the number of requests
			// (10 per second looks fine)
			std::this_thread::sleep_for(chrono::milliseconds(100));
		} catch (const std::runtime_error& e) {
			std::cerr << SD_ERR << "[Objenious] protocol: " << "Runtime error, impossible to download " << e.what()
					  << ", moving on..." << std::endl;
		}
	}
}

void ObjeniousApiDownloadScheduler::reloadStations()
{
	_downloaders.clear();

	std::vector<std::tuple<CassUuid, std::string, std::map<std::string, std::string>>> objeniousStations;
	_db.getAllObjeniousApiStations(objeniousStations);
	for (const auto& station : objeniousStations) {
		add(std::get<0>(station), std::get<1>(station), std::get<2>(station));
	}
}

}
