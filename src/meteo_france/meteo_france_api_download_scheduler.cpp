/**
 * @file meteo_france_download_scheduler.cpp
 * @brief Implementation of the MeteoFranceDownloadScheduler class
 * @author Laurent Georget
 * @date 2023-11-16
 */
/*
 * Copyright (C) 2019  SAS JD Environnement <contact@meteo-concept.fr>
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
#include <memory>
#include <tuple>
#include <unordered_map>
#include <vector>

#include <boost/asio/basic_waitable_timer.hpp>
#include <date.h>
#include <cassandra.h>
#include <dbconnection_observations.h>

#include "meteo_france/meteo_france_api_download_scheduler.h"
#include "meteo_france/meteo_france_api_downloader.h"
#include "meteo_france/meteo_france_api_bulk_downloader.h"
#include "meteo_france/meteo_france_api_6m_downloader.h"
#include "http_utils.h"
#include "abstract_download_scheduler.h"

namespace chrono = std::chrono;

namespace meteodata
{
using namespace date;

MeteoFranceApiDownloadScheduler::MeteoFranceApiDownloadScheduler(
	asio::io_context& ioContext, DbConnectionObservations& db,
	std::string apiKey, AsyncJobPublisher* jobPublisher) :
		AbstractDownloadScheduler{chrono::minutes{POLLING_PERIOD}, ioContext, db},
		_apiKey{std::move(apiKey)},
		_jobPublisher{jobPublisher}
{
}

void MeteoFranceApiDownloadScheduler::add(const CassUuid& station, const std::string& mfId)
{
	_downloaders.emplace_back(std::make_shared<MeteoFranceApiDownloader>(station, mfId, _db, _apiKey, _jobPublisher));
}

void MeteoFranceApiDownloadScheduler::download()
{
	auto now = chrono::system_clock::now();
	auto daypoint = date::floor<date::days>(now);
	auto tod = date::make_time(now - daypoint); // Yields time_of_day type
	auto minutes = tod.minutes().count();

	// will trigger every POLLING_PERIOD
	MeteoFranceApi6mDownloader downloader6m{_db, _apiKey, _jobPublisher};
	downloader6m.download(_client);

	// will trigger once per hour, 2*POLLING_PERIOD after the hour
	if (minutes > 2 * POLLING_PERIOD && minutes <= 3 * POLLING_PERIOD) {
		MeteoFranceApiBulkDownloader downloader{_db, _apiKey, _jobPublisher};
		downloader.download(_client);
	}

	for (const auto& it : _downloaders) {
		it->download(_client);
	}
}

void MeteoFranceApiDownloadScheduler::reloadStations()
{
	_downloaders.clear();
	// There are no mechanisms yet to load specific stations automatically
}

}
