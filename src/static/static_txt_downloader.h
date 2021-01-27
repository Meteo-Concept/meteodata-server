/**
 * @file statictxtdownloader.h
 * @brief Definition of the StatICTxtDownloader class
 * @author Laurent Georget
 * @date 2019-02-06
 */
/*
 * Copyright (C) 2019  JD Environnement <contact@meteo-concept.fr>
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

#ifndef STATICTXTDOWNLOADER_H
#define STATICTXTDOWNLOADER_H

#include <iostream>
#include <experimental/optional>
#include <string>

#include <unistd.h>

#include <boost/asio/io_service.hpp>
#include <boost/asio/basic_waitable_timer.hpp>
#include <cassandra.h>
#include <date/date.h>
#include <dbconnection_observations.h>

#include "../time_offseter.h"

namespace meteodata {

namespace asio = boost::asio;
namespace sys = boost::system;

using namespace meteodata;

/**
 */
class StatICTxtDownloader : public std::enable_shared_from_this<StatICTxtDownloader>
{
public:
	StatICTxtDownloader(asio::io_service& ioService, DbConnectionObservations& db, CassUuid station, const std::string& host, const std::string& url, bool _https, int timezone);
	void start();

private:
	asio::io_service& _ioService;
	DbConnectionObservations& _db;
	asio::basic_waitable_timer<chrono::steady_clock> _timer;
	CassUuid _station;
	std::string _stationName;
	std::string _host;
	std::string _url;
	bool _https;
	std::experimental::optional<float> _previousRainfall;
	date::sys_seconds _lastDownloadTime;
	TimeOffseter _timeOffseter;

	void checkDeadline(const sys::error_code& e);
	void waitUntilNextDownload();
	void download();
};

}

#endif
