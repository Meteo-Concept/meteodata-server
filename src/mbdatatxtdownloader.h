/**
 * @file mbdatatxtdownloader.h
 * @brief Definition of the MBDataTxtDownloader class
 * @author Laurent Georget
 * @date 2019-02-21
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

#ifndef MBDATATXTDOWNLOADER_H
#define MBDATATXTDOWNLOADER_H

#include <iostream>
#include <memory>
#include <array>
#include <vector>
#include <functional>
#include <experimental/optional>
#include <syslog.h>
#include <unistd.h>

#include <boost/system/error_code.hpp>
#include <boost/asio.hpp>
#include <boost/asio/basic_waitable_timer.hpp>
#include <cassandra.h>
#include <date/date.h>
#include <date/tz.h>
#include <dbconnection_observations.h>

#include "timeoffseter.h"

namespace meteodata {

namespace ip = boost::asio::ip;
namespace asio = boost::asio;
namespace sys = boost::system;
namespace chrono = std::chrono;

using namespace std::placeholders;
using namespace meteodata;

/**
 */
class MBDataTxtDownloader : public std::enable_shared_from_this<MBDataTxtDownloader>
{
public:
	MBDataTxtDownloader(asio::io_service& ioService, DbConnectionObservations& db, const std::tuple<CassUuid, std::string, std::string, bool, int, std::string>& downloadDetails);
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
	std::string _type;
	date::sys_seconds _lastDownloadTime;
	TimeOffseter _timeOffseter;

	void checkDeadline(const sys::error_code& e);
	void waitUntilNextDownload();
	bool checkStatusLine(boost::asio::streambuf& response);
	bool handleHttpHeaders(boost::asio::streambuf& response, size_t size);
	bool downloadHttps(boost::asio::streambuf& request, boost::asio::streambuf& response, sys::error_code& ec);
	bool downloadHttp(boost::asio::streambuf& request, boost::asio::streambuf& response, sys::error_code& ec);
	void download();
};

}

#endif
