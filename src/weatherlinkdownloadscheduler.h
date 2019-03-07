/**
 * @file weatherlinkdownloadscheduler.h
 * @brief Definition of the WeatherlinkDownloadScheduler class
 * @author Laurent Georget
 * @date 2019-03-07
 */
/*
 * Copyright (C) 2016  SAS Météo Concept <contact@meteo-concept.fr>
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

#ifndef WEATHERLINKDOWNLOADSCHEDULER_H
#define WEATHERLINKDOWNLOADSCHEDULER_H

#include <memory>
#include <vector>
#include <string>
#include <chrono>

#include <boost/system/error_code.hpp>
#include <boost/asio.hpp>
#include <boost/asio/basic_waitable_timer.hpp>
#include <cassandra.h>
#include <dbconnection_observations.h>

#include "weatherlinkdownloader.h"
#include "timeoffseter.h"

namespace meteodata {

namespace asio = boost::asio;
namespace sys = boost::system;
namespace chrono = std::chrono;

using namespace std::placeholders;

/**
 */
class WeatherlinkDownloadScheduler : public std::enable_shared_from_this<WeatherlinkDownloadScheduler>
{
public:
	WeatherlinkDownloadScheduler(asio::io_service& ioService, DbConnectionObservations& db);
	void start();
	void add(const CassUuid& station, const std::string& auth,
		const std::string& apiToken, TimeOffseter::PredefinedTimezone tz);

private:
	asio::io_service& _ioService;
	DbConnectionObservations& _db;
	asio::basic_waitable_timer<chrono::steady_clock> _timer;
	std::vector<std::shared_ptr<WeatherlinkDownloader>> _downloaders;

public:
	using DownloaderIterator =  decltype(_downloaders)::const_iterator;
	static constexpr char HOST[] = "weatherlink.com";
	static constexpr char APIHOST[] = "api.weatherlink.com";

private:
	void waitUntilNextDownload();
	void connectSocket(ip::tcp::socket& socket, const char host[]);
	void downloadArchives();
	void downloadRealTime();
	void checkDeadline(const sys::error_code& e);
	static constexpr int POLLING_PERIOD = 10; // in minutes
};

}

#endif
