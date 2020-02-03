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
#include <syslog.h>
#include <unistd.h>

#include <boost/system/error_code.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <cassandra.h>
#include <date/date.h>
#include <date/tz.h>
#include <dbconnection_observations.h>

#include "abstract_weatherlink_downloader.h"
#include "vantagepro2_archive_page.h"
#include "../time_offseter.h"


namespace meteodata {

namespace ip = boost::asio::ip;
namespace asio = boost::asio;
namespace sys = boost::system;

using namespace std::placeholders;
using namespace meteodata;

/**
 */
class WeatherlinkDownloader : public AbstractWeatherlinkDownloader
{
public:
	WeatherlinkDownloader(const CassUuid& station, const std::string& auth,
		const std::string& apiToken, asio::io_service& ioService, DbConnectionObservations& db,
		TimeOffseter::PredefinedTimezone tz);
	void download(ip::tcp::socket& socket);
	void downloadRealTime(asio::ssl::stream<ip::tcp::socket>& socket);

private:
	std::string _authentication;
	std::string _apiToken;
};

}

#endif