/**
 * @file weatherlinkdownloadscheduler.cpp
 * @brief Implementation of the WeatherlinkDownloadScheduler class
 * @author Laurent Georget
 * @date 2019-03-08
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
#include <functional>
#include <chrono>
#include <unordered_map>

#include <boost/system/error_code.hpp>
#include <boost/asio/basic_waitable_timer.hpp>
#include <boost/property_tree/ptree.hpp>
#include <date.h>
#include <cassandra.h>
#include <dbconnection_observations.h>

#include "../time_offseter.h"
#include "weatherlink_download_scheduler.h"
#include "weatherlink_downloader.h"
#include "../http_utils.h"
#include "../abstract_download_scheduler.h"

namespace asio = boost::asio;
namespace ip = boost::asio::ip;
namespace sys = boost::system;
namespace chrono = std::chrono;
namespace args = std::placeholders;
namespace pt = boost::property_tree;

namespace meteodata
{
using namespace date;

WeatherlinkDownloadScheduler::WeatherlinkDownloadScheduler(
	asio::io_context& ioContext, DbConnectionObservations& db,
	AsyncJobPublisher* jobPublisher) :
		AbstractDownloadScheduler{chrono::minutes{POLLING_PERIOD}, ioContext, db},
		_jobPublisher{jobPublisher}
{
}

void WeatherlinkDownloadScheduler::add(const CassUuid& station, const std::string& auth, const std::string& apiToken,
				       TimeOffseter::PredefinedTimezone tz)
{
	_downloaders.emplace_back(std::make_shared<WeatherlinkDownloader>(station, auth, apiToken, _db, tz, _jobPublisher));
}

void WeatherlinkDownloadScheduler::download()
{
	auto now = chrono::system_clock::now();
	auto daypoint = date::floor<date::days>(now);
	auto tod = date::make_time(now - daypoint); // Yields time_of_day type
	auto minutes = tod.minutes().count();

	downloadRealTime(minutes);
	downloadArchives(minutes);
}

void WeatherlinkDownloadScheduler::downloadRealTime(int minutes)
{
	for (const auto& downloader : _downloaders) {
		if (downloader->getPollingPeriod() <= POLLING_PERIOD || minutes % UNPRIVILEGED_POLLING_PERIOD < POLLING_PERIOD)
			genericDownload([&downloader](auto& client) { downloader->downloadRealTime(client); });
	}
}

void WeatherlinkDownloadScheduler::downloadArchives(int minutes)
{
	if (minutes < POLLING_PERIOD) { // will trigger once per hour
		for (const auto& downloader : _downloaders) {
			genericDownload([&downloader](auto& client) { downloader->download(client); });
		}
	}
}

void WeatherlinkDownloadScheduler::reloadStations()
{
	_downloaders.clear();

	std::vector<std::tuple<CassUuid, std::string, std::string, int>> weatherlinkStations;
	_db.getAllWeatherlinkStations(weatherlinkStations);
	for (const auto& station : weatherlinkStations) {
		add(std::get<0>(station), std::get<1>(station), std::get<2>(station),
			TimeOffseter::PredefinedTimezone(std::get<3>(station)));
	}
}

}
