/**
 * @file synopdownloader.h
 * @brief Definition of the SynopDownloader class
 * @author Laurent Georget
 * @date 2018-08-20
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

#ifndef SYNOPDOWNLOADER_H
#define SYNOPDOWNLOADER_H

#include <iostream>
#include <memory>
#include <array>
#include <vector>
#include <functional>
#include <map>

#include <boost/system/error_code.hpp>
#include <boost/asio.hpp>
#include <boost/asio/basic_waitable_timer.hpp>

#include <syslog.h>
#include <unistd.h>

#include <cassandra.h>
#include <date/date.h>
#include <date/tz.h>

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
class SynopDownloader : public std::enable_shared_from_this<SynopDownloader>
{
public:
	SynopDownloader(asio::io_service& ioService, DbConnection& db);
	void start();

private:
	DbConnection& _db;
	asio::io_service& _ioService;
	asio::basic_waitable_timer<chrono::steady_clock> _timer;
	std::map<std::string, CassUuid> _icaos;

	static constexpr char HOST[] = "www.ogimet.com";
	static constexpr char GROUP_FR[] = "07";

	void checkDeadline(const sys::error_code& e);
	void waitUntilNextDownload();
	void download();
};

}

#endif
