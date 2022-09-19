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

WeatherlinkDownloadScheduler::WeatherlinkDownloadScheduler(asio::io_context& ioContext, DbConnectionObservations& db,
							   std::string apiId, std::string apiSecret) :
		AbstractDownloadScheduler{chrono::minutes{POLLING_PERIOD}, ioContext, db},
		_apiId{std::move(apiId)},
		_apiSecret{std::move(apiSecret)}
{
}

void WeatherlinkDownloadScheduler::add(const CassUuid& station, const std::string& auth, const std::string& apiToken,
				       TimeOffseter::PredefinedTimezone tz)
{
	_downloaders.emplace_back(std::make_shared<WeatherlinkDownloader>(station, auth, apiToken, _db, tz));
}

void
WeatherlinkDownloadScheduler::addAPIv2(const CassUuid& station, bool archived, const std::map<int, CassUuid>& mapping,
					   const std::map<int, std::map<std::string, std::string>>& parsers,
				       const std::string& weatherlinkId, TimeOffseter&& to)
{
	_downloadersAPIv2.emplace_back(archived,
		std::make_shared<WeatherlinkApiv2Downloader>(station, weatherlinkId, mapping, parsers,
				_apiId, _apiSecret, _db, std::forward<TimeOffseter&&>(to))
	);
}

void WeatherlinkDownloadScheduler::download()
{
	downloadRealTime();

	auto now = chrono::system_clock::now();
	auto daypoint = date::floor<date::days>(now);
	auto tod = date::make_time(now - daypoint); // Yields time_of_day type

	if (tod.minutes().count() < POLLING_PERIOD) //will trigger once per hour
		downloadArchives();
}

void WeatherlinkDownloadScheduler::downloadRealTime()
{
	auto now = date::floor<chrono::minutes>(chrono::system_clock::now()).time_since_epoch().count();

	for (const auto& downloader : _downloaders) {
		if (downloader->getPollingPeriod() <= POLLING_PERIOD || now % UNPRIVILEGED_POLLING_PERIOD < POLLING_PERIOD)
			genericDownload([&downloader](auto& client) { downloader->downloadRealTime(client); });
	}

	for (const auto& it : _downloadersAPIv2) {
		// This function is called every POLLING_PERIOD minutes but
		// stations have varying polling periods of their own, and we
		// shouldn't download every POLLING_PERIOD minutes in some
		// cases.

		bool shouldDownload;
		if (it.first) {
			// If the station has access to archives, only download
			// the realtime data if it doesn't make us download more
			// frequently than the station polling period (e.g.
			// stations programmed with a polling period of 1h
			// meanwhile realtime data is normally downloaded every
			// UNPRIVILEGED_POLLING_PERIOD minutes).
			shouldDownload = it.second->getPollingPeriod() <= UNPRIVILEGED_POLLING_PERIOD;

			// Also, take care of not downloading more frequently
			// than the station polling period (e.g. if the station
			// has a polling period of 10min, only download during
			// the first POLLING_PERIOD minutes of every period of
			// 10 minutes).
			shouldDownload = shouldDownload && now % it.second->getPollingPeriod() < POLLING_PERIOD;
		} else {
			// If the station doesn't have access to archives, only
			// download data at the basic rate of
			// UNPRIVILEGED_POLLING_PERIOD minutes (download only
			// during the first POLLING_PERIOD minutes of every
			// period of UNPRIVILEGED_POLLING_PERIOD minutes).
			shouldDownload = now % UNPRIVILEGED_POLLING_PERIOD < POLLING_PERIOD;
		}

		if (shouldDownload)
			genericDownload([&it](auto& client) { (it.second)->downloadRealTime(client); });
	}
}

void WeatherlinkDownloadScheduler::downloadArchives()
{
	for (const auto& downloader : _downloaders) {
		genericDownload([&downloader](auto& client) { downloader->download(client); });
	}

	for (const auto& it : _downloadersAPIv2) {
		if (it.first) { // only download archives from archived stations
			genericDownload([&it](auto& client) { (it.second)->download(client); });
		}
	}
}

void WeatherlinkDownloadScheduler::reloadStations()
{
	_downloaders.clear();
	_downloadersAPIv2.clear();

	std::vector<std::tuple<CassUuid, std::string, std::string, int>> weatherlinkStations;
	_db.getAllWeatherlinkStations(weatherlinkStations);
	for (const auto& station : weatherlinkStations) {
		add(std::get<0>(station), std::get<1>(station), std::get<2>(station),
			TimeOffseter::PredefinedTimezone(std::get<3>(station)));
	}

	std::vector<std::tuple<CassUuid, bool, std::map<int, CassUuid>, std::string, std::map<int, std::map<std::string, std::string>> > > weatherlinkAPIv2Stations;
	_db.getAllWeatherlinkAPIv2Stations(weatherlinkAPIv2Stations);

	CurlWrapper client;
	std::unordered_map<std::string, pt::ptree> stations =
		WeatherlinkApiv2Downloader::downloadAllStations(client, _apiId, _apiSecret);

	for (const auto& station : weatherlinkAPIv2Stations) {
		auto st = stations.find(std::get<3>(station));
		if (st == stations.end()) {
			std::cout << SD_ERR << "[Weatherlink_v2 " << std::get<0>(station) << "] management: "
					  << "station is absent from the list of stations available in the API, is it unlinked?"
					  << std::endl;
			continue;
		}
		addAPIv2(std::get<0>(station), std::get<1>(station), std::get<2>(station),
			std::get<4>(station), std::get<3>(station),
			TimeOffseter::getTimeOffseterFor(st->second.get("time_zone", std::string{"UTC"})));
	}
}

}
