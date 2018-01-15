/**
 * @file weatherlinkdownloader.h
 * @brief Definition of the WeatherlinkDownloader class
 * @author Laurent Georget
 * @date 2018-01-10
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

#ifndef WEATHERLINKDOWNLOADER_H
#define WEATHERLINKDOWNLOADER_H

#include <iostream>
#include <memory>
#include <array>
#include <vector>
#include <functional>

#include <boost/system/error_code.hpp>
#include <boost/asio.hpp>
#include <boost/asio/basic_waitable_timer.hpp>

#include <syslog.h>
#include <unistd.h>

#include <cassandra.h>
#include <date/date.h>
#include <date/tz.h>

#include "vantagepro2archivepage.h"
#include "timeoffseter.h"
#include "dbconnection.h"


namespace meteodata {

namespace ip = boost::asio::ip;
namespace asio = boost::asio;
namespace sys = boost::system; //system() is a function, it cannot be redefined
			       //as a namespace
namespace chrono = std::chrono;

using namespace std::placeholders;
using namespace meteodata;

/**
 */
class WeatherlinkDownloader : public std::enable_shared_from_this<WeatherlinkDownloader>
{
public:
	WeatherlinkDownloader(const CassUuid& _station, const std::string& auth,
		asio::io_service& ioService, DbConnection& db,
		TimeOffseter::PredefinedTimezone tz);
	void start();

private:
	DbConnection& _db;
	asio::io_service& _ioService;
	std::string _authentication;
	asio::basic_waitable_timer<chrono::steady_clock> _timer;
	/**
	 * @brief The connected station's identifier in the database
	 */
	CassUuid _station;
	std::string _stationName;
	/**
	 * @brief The amount of time between two queries for data to the stations
	 */
	int _pollingPeriod;

	/**
	 * @brief The timestamp (in POSIX time) of the last archive entry
	 * retrieved from the station
	 */
	date::sys_seconds _lastArchive;

	/**
	 * @brief The \a TimeOffseter to use to convert timestamps between the
	 * station's time and POSIX time
	 */
	TimeOffseter _timeOffseter;

	static constexpr char HOST[] = "weatherlink.com";

	void checkDeadline(const sys::error_code& e);
	void waitUntilNextDownload();
	static bool compareAsciiCaseInsensitive(const std::string& s1, const std::string& s2);
	void download();
};

}

#endif
