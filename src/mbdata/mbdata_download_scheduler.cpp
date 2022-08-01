/**
 * @file mbdata_download_scheduler.cpp
 * @brief Implementation of the MBDataDownloadScheduler class
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
#include "mbdata_download_scheduler.h"
#include "mbdata_txt_downloader.h"
#include "../http_utils.h"

namespace asio = boost::asio;
namespace ip = boost::asio::ip;
namespace sys = boost::system;
namespace chrono = std::chrono;
namespace args = std::placeholders;

namespace meteodata
{

using namespace date;

MBDataDownloadScheduler::MBDataDownloadScheduler(asio::io_service& ioService, DbConnectionObservations& db) :
		_ioService{ioService},
		_db{db},
		_timer{ioService}
{
}

void
MBDataDownloadScheduler::add(const std::tuple<CassUuid, std::string, std::string, bool, int, std::string>& downloadDetails)
{
	_downloaders.emplace_back(std::make_shared<MBDataTxtDownloader>(_ioService, _db, downloadDetails));
}

void MBDataDownloadScheduler::start()
{
	_mustStop = false;
	reloadStations();
	waitUntilNextDownload();
}

void MBDataDownloadScheduler::stop()
{
	_mustStop = true;
	_timer.cancel();
}

void MBDataDownloadScheduler::reload()
{
	_timer.cancel();
	reloadStations();
	waitUntilNextDownload();
}

void MBDataDownloadScheduler::downloadArchives()
{
	for (const auto & _downloader : _downloaders) {
		try {
			_downloader->download(_client);
		} catch (const std::runtime_error& e) {
			std::cerr << SD_ERR << "[MBData] protocol: " << "Runtime error, impossible to download " << e.what()
					  << ", moving on..." << std::endl;
		}
	}
}

void MBDataDownloadScheduler::waitUntilNextDownload()
{
	if (_mustStop)
		return;

	auto self(shared_from_this());
	constexpr auto realTimePollingPeriod = chrono::minutes(POLLING_PERIOD);
	auto tp = chrono::minutes(realTimePollingPeriod) -
			  (chrono::system_clock::now().time_since_epoch() % chrono::minutes(realTimePollingPeriod));
	_timer.expires_from_now(tp);
	_timer.async_wait(std::bind(&MBDataDownloadScheduler::checkDeadline, self, args::_1));
}

void MBDataDownloadScheduler::checkDeadline(const sys::error_code& e)
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
		_timer.async_wait(std::bind(&MBDataDownloadScheduler::checkDeadline, self, args::_1));
	}
}

void MBDataDownloadScheduler::reloadStations()
{
	_downloaders.clear();

	std::vector<std::tuple<CassUuid, std::string, std::string, bool, int, std::string>> mbDataTxtStations;
	_db.getMBDataTxtStations(mbDataTxtStations);
	for (const auto& station : mbDataTxtStations) {
		add(station);
	}
}

}
