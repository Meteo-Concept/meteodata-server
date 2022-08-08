/**
 * @file abstract_download_scheduler.cpp
 * @brief Implementation of the AbstractDownloadScheduler class
 * @author Laurent Georget
 * @date 2022-08-05
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

#include "time_offseter.h"
#include "abstract_download_scheduler.h"
#include "http_utils.h"

namespace asio = boost::asio;
namespace ip = boost::asio::ip;
namespace sys = boost::system;
namespace chrono = std::chrono;
namespace args = std::placeholders;

namespace meteodata
{

using namespace date;

AbstractDownloadScheduler::AbstractDownloadScheduler(chrono::steady_clock::duration period, asio::io_context& ioContext, DbConnectionObservations& db) :
		Connector{ioContext, db},
		_period{period},
		_timer{ioContext}
{
	_status.shortStatus = "IDLE";
}

void AbstractDownloadScheduler::start()
{
	_mustStop = false;
	auto now = date::floor<chrono::seconds>(chrono::system_clock::now());
	_status.activeSince = now;
	_status.lastReloaded = now;
	_status.nbDownloads = 0;
	_status.shortStatus = "OK";
	reloadStations();
	waitUntilNextDownload();
}

void AbstractDownloadScheduler::stop()
{
	_mustStop = true;
	_status.shortStatus = "STOPPED";
	_timer.cancel();
}

void AbstractDownloadScheduler::reload()
{
	_timer.cancel();
	_status.lastReloaded = date::floor<chrono::seconds>(chrono::system_clock::now());
	_status.nbDownloads = 0;
	reloadStations();
	waitUntilNextDownload();
}

void AbstractDownloadScheduler::waitUntilNextDownload()
{
	if (_mustStop)
		return;

	auto self(shared_from_this());
	auto tp = _period - (chrono::system_clock::now().time_since_epoch() % _period) + _offset;
	_timer.expires_from_now(tp);
	_status.nextDownload = date::floor<chrono::seconds>(chrono::system_clock::now() + tp);
	_timer.async_wait([this, self] (const sys::error_code& e) { checkDeadline(e); });
}

void AbstractDownloadScheduler::checkDeadline(const sys::error_code& e)
{
	/* if the timer has been cancelled, then bail out ; we probably have been
	 * asked to die */
	if (e == sys::errc::operation_canceled)
		return;

	// verify that the timeout is not spurious
	if (_timer.expires_at() <= chrono::steady_clock::now()) {
		++_status.nbDownloads;
		_status.lastDownload = date::floor<chrono::seconds>(chrono::system_clock::now());
		download();
		waitUntilNextDownload();
	} else {
		/* spurious handler call, restart the timer without changing the
		 * deadline */
		auto self(shared_from_this());
		_timer.async_wait([this, self] (const sys::error_code& e) { checkDeadline(e); });
	}
}


}
