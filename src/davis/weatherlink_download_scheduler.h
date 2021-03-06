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
#include <map>
#include <thread>

#include <boost/system/error_code.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio.hpp>
#include <boost/asio/basic_waitable_timer.hpp>
#include <cassandra.h>
#include <dbconnection_observations.h>

#include "weatherlink_downloader.h"
#include "weatherlink_apiv2_downloader.h"
#include "../time_offseter.h"
#include "../curl_wrapper.h"

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
	WeatherlinkDownloadScheduler(asio::io_service& ioService, DbConnectionObservations& db,
		const std::string& apiId, const std::string& apiSecret);
	void start();
	void add(const CassUuid& station, const std::string& auth,
		const std::string& apiToken, TimeOffseter::PredefinedTimezone tz);
	void addAPIv2(const CassUuid& station, bool archived,
		const std::map<int, CassUuid>& substations,
		const std::string& weatherlinkId,
		TimeOffseter&& to);

private:
	asio::io_service& _ioService;
	DbConnectionObservations& _db;
	const std::string _apiId;
	const std::string _apiSecret;
	asio::basic_waitable_timer<chrono::steady_clock> _timer;
	std::vector<std::shared_ptr<WeatherlinkDownloader>> _downloaders;
	std::vector<std::pair<bool, std::shared_ptr<WeatherlinkApiv2Downloader>>> _downloadersAPIv2;
	CurlWrapper _client;

public:
	using DownloaderIterator =  decltype(_downloaders)::const_iterator;
	static constexpr char HOST[] = "weatherlink.com";
	static constexpr char APIHOST[] = "api.weatherlink.com";

private:
	void reloadStations();
	void waitUntilNextDownload();
	template<typename Downloader>
	void genericDownload(const Downloader& downloadMethod) {
		try {
			downloadMethod(_client);

			// Wait for 100ms because the number of requests is
			// capped at 10 per second
			std::this_thread::sleep_for(chrono::milliseconds(100));

		} catch (const std::runtime_error& e) {
			std::cerr << "Runtime error, impossible to download " << e.what() << ", moving on..." << std::endl;
		}
	}
	void downloadArchives();
	void downloadRealTime();
	void checkDeadline(const sys::error_code& e);

	/**
	 * The polling period that apply to all stations, in minutes
	 */
	static constexpr int UNPRIVILEGED_POLLING_PERIOD = 15;
	/**
	 * The minimal polling period, for stations authorized to get
	 * realtime data more frequently than others, in minutes
	 */
	static constexpr int POLLING_PERIOD = 5;
};

}

#endif
