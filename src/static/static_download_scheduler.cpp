/**
 * @file static_download_scheduler.cpp
 * @brief Implementation of the StatICDownloadScheduler class
 * @author Laurent Georget
 * @date 2022-08-01
 */
/*
 * Copyright (C) 2022  SAS JD Environnement <contact@meteo-concept.fr>
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
#include <thread>
#include <unistd.h>

#include <systemd/sd-daemon.h>
#include <boost/system/error_code.hpp>
#include <boost/asio/basic_waitable_timer.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <date.h>
#include <cassandra.h>
#include <dbconnection_observations.h>

#include "../time_offseter.h"
#include "static_download_scheduler.h"
#include "static_txt_downloader.h"
#include "../http_utils.h"

namespace asio = boost::asio;
namespace ip = boost::asio::ip;
namespace sys = boost::system;
namespace chrono = std::chrono;
namespace args = std::placeholders;

namespace meteodata
{

using namespace date;

StatICDownloadScheduler::StatICDownloadScheduler(asio::io_context& ioContext, DbConnectionObservations& db) :
		Connector{ioContext, db},
		_timer{ioContext}
{
}

void
StatICDownloadScheduler::add(const CassUuid& station, const std::string& host, const std::string& url,
							 bool https, int timezone,
							 const std::map<std::string, std::string>& sensors)
{
	_downloaders.emplace_back(
			std::make_shared<StatICTxtDownloader>(_ioContext, _db, station, host, url, https, timezone, sensors)
	);
}

void StatICDownloadScheduler::start()
{
	_mustStop = false;
	reloadStations();
	waitUntilNextDownload();
}

void StatICDownloadScheduler::stop()
{
	_mustStop = true;
	_timer.cancel();
}

void StatICDownloadScheduler::reload()
{
	_timer.cancel();
	reloadStations();
	waitUntilNextDownload();
}

void StatICDownloadScheduler::downloadArchives()
{
	for (const auto & _downloader : _downloaders) {
		try {
			_downloader->download(_client);
		} catch (const std::runtime_error& e) {
			std::cerr << SD_ERR << "[StatIC] protocol: " << "Runtime error, impossible to download " << e.what()
					  << ", moving on..." << std::endl;
		}
	}
}

void StatICDownloadScheduler::waitUntilNextDownload()
{
	if (_mustStop)
		return;

	auto self(shared_from_this());
	constexpr auto realTimePollingPeriod = chrono::minutes(POLLING_PERIOD);
	auto tp = chrono::minutes(realTimePollingPeriod) -
			  (chrono::system_clock::now().time_since_epoch() % chrono::minutes(realTimePollingPeriod));
	_timer.expires_from_now(tp);
	_timer.async_wait([this, self] (const sys::error_code& e) { checkDeadline(e); });
}

void StatICDownloadScheduler::checkDeadline(const sys::error_code& e)
{
	/* if the timer has been cancelled, then bail out ; we probably have been
	 * asked to die */
	if (e == sys::errc::operation_canceled)
		return;

	// verify that the timeout is not spurious
	if (_timer.expires_at() <= chrono::steady_clock::now()) {
		downloadArchives();
		waitUntilNextDownload();
	} else {
		/* spurious handler call, restart the timer without changing the
		 * deadline */
		auto self(shared_from_this());
		_timer.async_wait([this, self] (const sys::error_code& e) { checkDeadline(e); });
	}
}

void StatICDownloadScheduler::reloadStations()
{
	_downloaders.clear();

	std::vector<std::tuple<CassUuid, std::string, std::string, bool, int, std::map<std::string, std::string>>> statICTxtStations;
	_db.getStatICTxtStations(statICTxtStations);
	for (const auto& station : statICTxtStations) {
		add(std::get<0>(station), std::get<1>(station), std::get<2>(station),
		    std::get<3>(station), std::get<4>(station), std::get<5>(station));
	}
}

}
