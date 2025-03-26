/**
 * @file weatherlink_apiv2_download_scheduler.cpp
 * @brief Implementation of the WeatherlinkApiv2DownloadScheduler class
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
#include <functional>
#include <chrono>
#include <unordered_map>
#include <mutex>

#include <boost/system/error_code.hpp>
#include <boost/asio/basic_waitable_timer.hpp>
#include <boost/property_tree/ptree.hpp>
#include <date/date.h>
#include <cassandra.h>
#include <cassobs/dbconnection_observations.h>

#include "../time_offseter.h"
#include "weatherlink_apiv2_download_scheduler.h"
#include "weatherlink_apiv2_downloader.h"
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

WeatherlinkApiv2DownloadScheduler::WeatherlinkApiv2DownloadScheduler(
	asio::io_context& ioContext, DbConnectionObservations& db,
	std::string apiId, std::string apiSecret, AsyncJobPublisher* jobPublisher) :
		AbstractDownloadScheduler{chrono::minutes{POLLING_PERIOD}, ioContext, db},
		_apiId{std::move(apiId)},
		_apiSecret{std::move(apiSecret)},
		_jobPublisher{jobPublisher}
{
}

void WeatherlinkApiv2DownloadScheduler::add(const CassUuid& station, bool archived,
		const std::map<int, CassUuid>& mapping,
		const std::map<int, std::map<std::string, std::string>>& parsers,
		const std::string& weatherlinkId, TimeOffseter&& to)
{
	std::lock_guard<std::recursive_mutex> lock{_downloadersMutex};
	_downloadersAPIv2.emplace_back(
		archived,
		std::make_shared<WeatherlinkApiv2Downloader>(station, weatherlinkId, mapping, parsers,
			_apiId, _apiSecret, _db, std::forward<TimeOffseter&&>(to), _jobPublisher)
	);
}

void WeatherlinkApiv2DownloadScheduler::download()
{
	auto now = chrono::system_clock::now();
	auto daypoint = date::floor<date::days>(now);
	auto tod = date::make_time(now - daypoint); // Yields time_of_day type
	auto minutes = tod.minutes().count();

	std::lock_guard<std::recursive_mutex> lock{_downloadersMutex};
	downloadArchives(minutes);
	downloadRealTime(minutes);
}

void WeatherlinkApiv2DownloadScheduler::downloadRealTime(int minutes)
{
	if ((minutes % UNPRIVILEGED_POLLING_PERIOD) < POLLING_PERIOD) { // only once every 15 minutes
		for (const auto& it : _downloadersAPIv2) {
			if (_mustStop)
				break;

			// Do not download real-time data for archive station under normal circumstances
			if (it.first)
				continue;

			// The actual HTTP downloads are actually done by a separate program,
			// all we have to do is retrieve them from the database
			genericDownload([&it](auto& client) { (it.second)->ingestRealTime(); });
		}
	}
}

void WeatherlinkApiv2DownloadScheduler::downloadArchives(int minutes)
{
	for (const auto& it : _downloadersAPIv2) {
		if (_mustStop)
			break;

		if (it.first && (minutes % it.second->getPollingPeriod()) < POLLING_PERIOD) {
			// only download archives from archived
			// stations and at the correct rate
			genericDownload([&it](auto& client) { (it.second)->download(client); });
		}
	}
}

void WeatherlinkApiv2DownloadScheduler::reloadStations()
{
	std::lock_guard<std::recursive_mutex> lock{_downloadersMutex};

	_downloadersAPIv2.clear();

	std::vector<std::tuple<CassUuid, bool, std::map<int, CassUuid>, std::string, std::map<int, std::map<std::string, std::string>>>> weatherlinkAPIv2Stations;
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
		add(std::get<0>(station), std::get<1>(station), std::get<2>(station),
			std::get<4>(station), std::get<3>(station),
			TimeOffseter::getTimeOffseterFor(st->second.get("time_zone", std::string{"UTC"})));
	}
}

}
