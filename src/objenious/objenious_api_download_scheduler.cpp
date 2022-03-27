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
#include "objenious_api_download_scheduler.h"
#include "objenious_api_downloader.h"
#include "../http_utils.h"

namespace asio = boost::asio;
namespace ip = boost::asio::ip;
namespace sys = boost::system;
namespace chrono = std::chrono;
namespace args = std::placeholders;

namespace meteodata
{

constexpr int ObjeniousApiDownloadScheduler::POLLING_PERIOD;


using namespace date;

ObjeniousApiDownloadScheduler::ObjeniousApiDownloadScheduler(asio::io_service& ioService, DbConnectionObservations& db,
															 const std::string& apiKey) :
		_ioService{ioService},
		_db{db},
		_apiKey{apiKey},
		_timer{ioService}
{
}

void ObjeniousApiDownloadScheduler::add(const CassUuid& station, const std::string& fieldClimateId,
										const std::map<std::string, std::string> variables)
{
	_downloaders.emplace_back(
			std::make_shared<ObjeniousApiDownloader>(station, fieldClimateId, variables, _db, _apiKey));
}

void ObjeniousApiDownloadScheduler::start()
{
	_mustStop = false;
	reloadStations();
	waitUntilNextDownload();
}

void ObjeniousApiDownloadScheduler::stop()
{
	_mustStop = true;
	_timer.cancel();
}

void ObjeniousApiDownloadScheduler::downloadArchives()
{
	for (auto it = _downloaders.cbegin() ; it != _downloaders.cend() ; ++it) {
		try {
			(*it)->download(_client);
			// Wait for 100ms to limit the number of requests
			// (10 per second looks fine)
			std::this_thread::sleep_for(chrono::milliseconds(100));
		} catch (const std::runtime_error& e) {
			std::cerr << SD_ERR << "[Objenious] protocol: " << "Runtime error, impossible to download " << e.what()
					  << ", moving on..." << std::endl;
		}
	}
}

void ObjeniousApiDownloadScheduler::waitUntilNextDownload()
{
	auto self(shared_from_this());
	constexpr auto pollingPeriod = chrono::minutes(POLLING_PERIOD);
	auto tp = chrono::minutes(pollingPeriod) -
			  (chrono::system_clock::now().time_since_epoch() % chrono::minutes(pollingPeriod));
	_timer.expires_from_now(tp);
	_timer.async_wait(std::bind(&ObjeniousApiDownloadScheduler::checkDeadline, self, args::_1));
}

void ObjeniousApiDownloadScheduler::checkDeadline(const sys::error_code& e)
{
	/* if the timer has been cancelled, then bail out ; we probably have been
	 * asked to die */
	if (e == sys::errc::operation_canceled)
		return;

	// verify that the timeout is not spurious
	if (_timer.expires_at() <= chrono::steady_clock::now()) {
		downloadArchives();
		if (!_mustStop)
			waitUntilNextDownload();
	} else {
		/* spurious handler call, restart the timer without changing the
		 * deadline */
		auto self(shared_from_this());
		_timer.async_wait(std::bind(&ObjeniousApiDownloadScheduler::checkDeadline, self, args::_1));
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
