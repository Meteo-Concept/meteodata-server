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
#include <unistd.h>

#include <boost/system/error_code.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <cassandra.h>
#include <date.h>
#include <tz.h>
#include <cassobs/dbconnection_observations.h>
#include "async_job_publisher.h"

#include "abstract_weatherlink_downloader.h"
#include "vantagepro2_archive_page.h"
#include "../time_offseter.h"
#include "../curl_wrapper.h"

namespace meteodata
{

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
	WeatherlinkDownloader(const CassUuid& station, std::string auth, std::string apiToken,
		DbConnectionObservations& db, TimeOffseter::PredefinedTimezone tz, AsyncJobPublisher* jobPublisher = nullptr);
	void download(CurlWrapper& client);
	void downloadRealTime(CurlWrapper& client);

private:
	std::string _authentication;
	std::string _apiToken;
	static const std::string REALTIME_BASE_URL;
	static const std::string ARCHIVE_BASE_URL;

	void logAndThrowCurlError(CurlWrapper& client, const std::string& host);
};

}

#endif
