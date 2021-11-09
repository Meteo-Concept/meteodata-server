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
#include <date/date.h>
#include <cassandra.h>
#include <dbconnection_observations.h>

#include "../time_offseter.h"
#include "fieldclimate_api_download_scheduler.h"
#include "fieldclimate_api_downloader.h"
#include "../http_utils.h"

namespace asio = boost::asio;
namespace ip = boost::asio::ip;
namespace sys = boost::system;
namespace chrono = std::chrono;
namespace args = std::placeholders;

namespace meteodata {

constexpr int FieldClimateApiDownloadScheduler::POLLING_PERIOD;


using namespace date;

FieldClimateApiDownloadScheduler::FieldClimateApiDownloadScheduler(
	asio::io_service& ioService, DbConnectionObservations& db,
	const std::string& apiId, const std::string& apiSecret
	) :
	_ioService{ioService},
	_db{db},
	_apiId{apiId},
	_apiSecret{apiSecret},
	_timer{ioService}
{
}

void FieldClimateApiDownloadScheduler::add(
	const CassUuid& station, const std::string& fieldClimateId,
	TimeOffseter::PredefinedTimezone tz,
	const std::map<std::string, std::string> sensors
) {
	_downloaders.emplace_back(std::make_shared<FieldClimateApiDownloader>(station, fieldClimateId, sensors, _db, tz, _apiId, _apiSecret));
}

void FieldClimateApiDownloadScheduler::start()
{
    _mustStop = false;
	reloadStations();
	waitUntilNextDownload();
}

void FieldClimateApiDownloadScheduler::stop()
{
    _mustStop = true;
    _timer.cancel();
}

void FieldClimateApiDownloadScheduler::downloadArchives()
{
	for (auto it = _downloaders.cbegin() ; it != _downloaders.cend() ; ++it) {
		try {
			(*it)->download(_client);
			// Wait for 100ms to limit the number of requests
			// (capped at 10 per second)
			std::this_thread::sleep_for(chrono::milliseconds(100));
		} catch (const std::runtime_error& e) {
			std::cerr << SD_ERR << "[Pessl] protocol: "
			    << "Runtime error, impossible to download " << e.what() << ", moving on..." << std::endl;
		}
	}
}

void FieldClimateApiDownloadScheduler::waitUntilNextDownload()
{
	auto self(shared_from_this());
	constexpr auto realTimePollingPeriod = chrono::minutes(POLLING_PERIOD);
	auto tp = chrono::minutes(realTimePollingPeriod) -
	       (chrono::system_clock::now().time_since_epoch() % chrono::minutes(realTimePollingPeriod));
	_timer.expires_from_now(tp);
	_timer.async_wait(std::bind(&FieldClimateApiDownloadScheduler::checkDeadline, self, args::_1));
}

void FieldClimateApiDownloadScheduler::checkDeadline(const sys::error_code& e)
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
		_timer.async_wait(std::bind(&FieldClimateApiDownloadScheduler::checkDeadline, self, args::_1));
	}
}

void FieldClimateApiDownloadScheduler::reloadStations()
{
	_downloaders.clear();

	std::vector<std::tuple<CassUuid, std::string, int, std::map<std::string, std::string>>> fieldClimateStations;
	_db.getAllFieldClimateApiStations(fieldClimateStations);
	for (const auto& station : fieldClimateStations) {
		add(
			std::get<0>(station), std::get<1>(station),
			TimeOffseter::PredefinedTimezone(std::get<2>(station)),
			std::get<3>(station)
		);
	}
}

}
