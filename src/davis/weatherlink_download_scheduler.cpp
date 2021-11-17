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
#include <iterator>
#include <chrono>
#include <unordered_map>
#include <unistd.h>

#include <boost/system/error_code.hpp>
#include <boost/asio/basic_waitable_timer.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/property_tree/ptree.hpp>
#include <date/date.h>
#include <cassandra.h>
#include <dbconnection_observations.h>

#include "../time_offseter.h"
#include "weatherlink_download_scheduler.h"
#include "weatherlink_downloader.h"
#include "../http_utils.h"
#include "../curl_wrapper.h"

namespace asio = boost::asio;
namespace ip = boost::asio::ip;
namespace sys = boost::system;
namespace chrono = std::chrono;
namespace args = std::placeholders;
namespace pt = boost::property_tree;

namespace meteodata {

constexpr char WeatherlinkDownloadScheduler::HOST[];
constexpr char WeatherlinkDownloadScheduler::APIHOST[];
constexpr int WeatherlinkDownloadScheduler::POLLING_PERIOD;
constexpr int WeatherlinkDownloadScheduler::UNPRIVILEGED_POLLING_PERIOD;


using namespace date;

WeatherlinkDownloadScheduler::WeatherlinkDownloadScheduler(asio::io_service& ioService, DbConnectionObservations& db,
		const std::string& apiId, const std::string& apiSecret) :
	_ioService{ioService},
	_db{db},
	_apiId{apiId},
	_apiSecret{apiSecret},
	_timer{ioService}
{
}

void WeatherlinkDownloadScheduler::add(const CassUuid& station, const std::string& auth,
	const std::string& apiToken, TimeOffseter::PredefinedTimezone tz)
{
	_downloaders.emplace_back(std::make_shared<WeatherlinkDownloader>(station, auth, apiToken, _db, tz));
}

void WeatherlinkDownloadScheduler::addAPIv2(const CassUuid& station, bool archived,
		const std::map<int, CassUuid>& mapping,
		const std::string& weatherlinkId,
		TimeOffseter&& to)
{
	_downloadersAPIv2.emplace_back(archived, std::make_shared<WeatherlinkApiv2Downloader>(station, weatherlinkId,
		mapping, _apiId, _apiSecret, _db, std::forward<TimeOffseter&&>(to)));
}

void WeatherlinkDownloadScheduler::start()
{
    _mustStop = false;
	reloadStations();
	waitUntilNextDownload();
}

void WeatherlinkDownloadScheduler::stop()
{
    _mustStop = true;
    _timer.cancel();
}

void WeatherlinkDownloadScheduler::downloadRealTime()
{
	auto now = date::floor<chrono::minutes>(chrono::system_clock::now()).time_since_epoch().count();

	for (auto it = _downloaders.cbegin() ; it != _downloaders.cend() ; ++it) {
		if ((*it)->getPollingPeriod() <= POLLING_PERIOD || now % UNPRIVILEGED_POLLING_PERIOD < POLLING_PERIOD)
			genericDownload([it](auto& client) { (*it)->downloadRealTime(client); });
	}

	for (auto it = _downloadersAPIv2.cbegin() ; it != _downloadersAPIv2.cend() ; ++it) {
		// Download only for stations without access to archive or
		// stations for which it doesn't make use fetch more frequently
		// than archives
		if ((!it->first || it->second->getPollingPeriod() <= UNPRIVILEGED_POLLING_PERIOD) &&
		    now % it->second->getPollingPeriod() < POLLING_PERIOD)
			genericDownload([it](auto& client) { (it->second)->downloadRealTime(client); });
	}
}

void WeatherlinkDownloadScheduler::downloadArchives()
{
	for (auto it = _downloaders.cbegin() ; it != _downloaders.cend() ; ++it) {
		genericDownload([it](auto& client) { (*it)->download(client); });
	}

	for (auto it = _downloadersAPIv2.cbegin() ; it != _downloadersAPIv2.cend() ; ++it) {
		if (it->first) { // only download archives from archived stations
			genericDownload([it](auto& client) { (it->second)->download(client); });
		}
	}
}

void WeatherlinkDownloadScheduler::waitUntilNextDownload()
{
	auto self(shared_from_this());
	constexpr auto realTimePollingPeriod = chrono::minutes(POLLING_PERIOD);
	auto tp = chrono::minutes(realTimePollingPeriod) -
	       (chrono::system_clock::now().time_since_epoch() % chrono::minutes(realTimePollingPeriod));
	_timer.expires_from_now(tp);
	_timer.async_wait(std::bind(&WeatherlinkDownloadScheduler::checkDeadline, self, args::_1));
}

void WeatherlinkDownloadScheduler::checkDeadline(const sys::error_code& e)
{
	/* if the timer has been cancelled, then bail out ; we probably have been
	 * asked to die */
	if (e == sys::errc::operation_canceled)
		return;

	// verify that the timeout is not spurious
	if (_timer.expires_at() <= chrono::steady_clock::now()) {
		auto now = chrono::system_clock::now();
		auto daypoint = date::floor<date::days>(now);
		auto tod = date::make_time(now - daypoint); // Yields time_of_day type

		downloadRealTime();
		if (tod.minutes().count() < POLLING_PERIOD) //will trigger once per hour
			downloadArchives();
		if (!_mustStop)
            waitUntilNextDownload();
	} else {
		/* spurious handler call, restart the timer without changing the
		 * deadline */
		auto self(shared_from_this());
		_timer.async_wait(std::bind(&WeatherlinkDownloadScheduler::checkDeadline, self, args::_1));
	}
}

void WeatherlinkDownloadScheduler::reloadStations()
{
	_downloaders.clear();
	_downloadersAPIv2.clear();

	std::vector<std::tuple<CassUuid, std::string, std::string, int>> weatherlinkStations;
	_db.getAllWeatherlinkStations(weatherlinkStations);
	for (const auto& station : weatherlinkStations) {
		add(
			std::get<0>(station), std::get<1>(station), std::get<2>(station),
			TimeOffseter::PredefinedTimezone(std::get<3>(station))
		);
	}

	std::vector<std::tuple<CassUuid, bool, std::map<int, CassUuid>, std::string>> weatherlinkAPIv2Stations;
	_db.getAllWeatherlinkAPIv2Stations(weatherlinkAPIv2Stations);

	CurlWrapper client;
	std::unordered_map<std::string, pt::ptree> stations = WeatherlinkApiv2Downloader::downloadAllStations(client, _apiId, _apiSecret);

	for (const auto& station : weatherlinkAPIv2Stations) {
		auto st = stations.find(std::get<3>(station));
		if (st == stations.end()) {
			std::cout << SD_ERR << "[Weatherlink_v2 " << std::get<0>(station ) << "] management: "
	    		<< "station is absent from the list of stations available in the API, is it unlinked?" << std::endl;
			continue;
		}
		addAPIv2(
			std::get<0>(station), std::get<1>(station),
			std::get<2>(station), std::get<3>(station),
			TimeOffseter::getTimeOffseterFor(st->second.get("time_zone", std::string{"UTC"}))
		);
	}
}

}
