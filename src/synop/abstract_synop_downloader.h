/**
 * @file abstractsynopdownloader.h
 * @brief Definition of the AbstractSynopDownloader class
 * @author Laurent Georget
 * @date 2019-02-20
 */
/*
 * Copyright (C) 2019  SAS Météo Concept <contact@meteo-concept.fr>
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

#ifndef ABSTRACTSYNOPDOWNLOADER_H
#define ABSTRACTSYNOPDOWNLOADER_H

#include <iostream>
#include <memory>
#include <array>
#include <vector>
#include <functional>
#include <map>
#include <syslog.h>
#include <unistd.h>

#include <boost/system/error_code.hpp>
#include <boost/asio.hpp>
#include <boost/asio/basic_waitable_timer.hpp>
#include <cassandra.h>
#include <date/date.h>
#include <date/tz.h>
#include <dbconnection_observations.h>


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
class AbstractSynopDownloader : public std::enable_shared_from_this<AbstractSynopDownloader>
{
public:
	AbstractSynopDownloader(asio::io_service& ioService, DbConnectionObservations& db);
	virtual ~AbstractSynopDownloader() = default;
	virtual void start() = 0;

protected:
	asio::io_service& _ioService;
	DbConnectionObservations& _db;
	asio::basic_waitable_timer<chrono::steady_clock> _timer;
	std::map<std::string, CassUuid> _icaos;

	static constexpr char HOST[] = "www.ogimet.com";

	void checkDeadline(const sys::error_code& e);
	virtual chrono::minutes computeWaitDuration() = 0;
	void waitUntilNextDownload();
	void download();
	virtual void buildDownloadRequest(std::ostream& out) = 0;
};

}

#endif
