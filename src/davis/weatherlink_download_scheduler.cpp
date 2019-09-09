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
#include <syslog.h>
#include <unistd.h>

#include <boost/system/error_code.hpp>
#include <boost/asio/basic_waitable_timer.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <date/date.h>
#include <cassandra.h>
#include <dbconnection_observations.h>

#include "../time_offseter.h"
#include "weatherlink_download_scheduler.h"
#include "weatherlink_downloader.h"

namespace asio = boost::asio;
namespace ip = boost::asio::ip;
namespace sys = boost::system;
namespace chrono = std::chrono;
namespace args = std::placeholders;

namespace meteodata {

constexpr char WeatherlinkDownloadScheduler::HOST[];
constexpr char WeatherlinkDownloadScheduler::APIHOST[];
constexpr int WeatherlinkDownloadScheduler::POLLING_PERIOD;


using namespace date;

WeatherlinkDownloadScheduler::WeatherlinkDownloadScheduler(asio::io_service& ioService, DbConnectionObservations& db) :
	_ioService(ioService),
	_db(db),
	_timer(ioService)
{
}

void WeatherlinkDownloadScheduler::add(const CassUuid& station, const std::string& auth,
	const std::string& apiToken, TimeOffseter::PredefinedTimezone tz)
{
	_downloaders.emplace_back(std::make_shared<WeatherlinkDownloader>(station, auth, apiToken, _ioService, _db, tz));
}

void WeatherlinkDownloadScheduler::start()
{
	waitUntilNextDownload();
}

void WeatherlinkDownloadScheduler::connectSocket(ip::tcp::socket& socket, const char host[])
{
	// Get a list of endpoints corresponding to the server name.
	ip::tcp::resolver resolver(_ioService);
	ip::tcp::resolver::query query(host, "http");
	ip::tcp::resolver::iterator endpointIterator = resolver.resolve(query);

	// Try each endpoint until we successfully establish a connection.
	asio::connect(socket, endpointIterator);
}

void WeatherlinkDownloadScheduler::genericDownload(const char host[], std::function<void(decltype(_downloaders)::const_reference, ip::tcp::socket&)> downloadMethod)
{
	ip::tcp::socket socket(_ioService);
	connectSocket(socket, host);
	int retry = 0;

	for (auto it = _downloaders.cbegin() ; it != _downloaders.cend() ; ) {
		try {
			downloadMethod(*it, socket);
			retry = 0;
			++it;
		} catch (const sys::system_error& e) {
			retry++;
			if (e.code() == asio::error::eof) {
				std::cerr << "Lost connection to server while attempting to download, retrying." << std::endl;
				connectSocket(socket, host);
				// attempt twice to download and move on to the
				// next station
				if (retry >= 2) {
					std::cerr << "Tried twice already, moving on..." << std::endl;
					retry =  0;
					++it;
				}
			} else {
				throw e;
			}
		}
	}
}

void WeatherlinkDownloadScheduler::downloadRealTime()
{
	genericDownload(APIHOST, &WeatherlinkDownloader::downloadRealTime);
}

void WeatherlinkDownloadScheduler::downloadArchives()
{
	genericDownload(HOST, &WeatherlinkDownloader::download);
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
	std::cerr << "Deadline handler hit: " << e.value() << ": " << e.message() << std::endl;
	if (e == sys::errc::operation_canceled)
		return;

	// verify that the timeout is not spurious
	if (_timer.expires_at() <= chrono::steady_clock::now()) {
		std::cerr << "Timed out!" << std::endl;
		auto now = chrono::system_clock::now();
		auto daypoint = date::floor<date::days>(now);
		auto tod = date::make_time(now - daypoint); // Yields time_of_day type

		downloadRealTime();
		if (tod.minutes().count() < POLLING_PERIOD) //will trigger once per hour
			downloadArchives();
		waitUntilNextDownload();
	} else {
		/* spurious handler call, restart the timer without changing the
		 * deadline */
		auto self(shared_from_this());
		_timer.async_wait(std::bind(&WeatherlinkDownloadScheduler::checkDeadline, self, args::_1));
	}
}

}
