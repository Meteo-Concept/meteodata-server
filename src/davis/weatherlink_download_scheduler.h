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
#include <boost/asio/ssl.hpp>
#include <boost/asio.hpp>
#include <boost/asio/basic_waitable_timer.hpp>
#include <cassandra.h>
#include <dbconnection_observations.h>

#include "weatherlink_downloader.h"
#include "weatherlink_apiv2_downloader.h"
#include "../time_offseter.h"

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
	void addAPIv2(const CassUuid& station, bool archived, const std::string& weatherlinkId,
		TimeOffseter::PredefinedTimezone tz);

private:
	asio::io_service& _ioService;
	DbConnectionObservations& _db;
	const std::string _apiId;
	const std::string _apiSecret;
	asio::basic_waitable_timer<chrono::steady_clock> _timer;
	std::vector<std::shared_ptr<WeatherlinkDownloader>> _downloaders;
	std::vector<std::pair<bool, std::shared_ptr<WeatherlinkApiv2Downloader>>> _downloadersAPIv2;
	asio::ssl::context _sslContext;

public:
	using DownloaderIterator =  decltype(_downloaders)::const_iterator;
	static constexpr char HOST[] = "weatherlink.com";
	static constexpr char APIHOST[] = "api.weatherlink.com";

private:
	void waitUntilNextDownload();
	void connectSocket(std::unique_ptr<ip::tcp::socket>& socket, const char host[]);
	void connectSocket(std::unique_ptr<asio::ssl::stream<ip::tcp::socket>>& socket, const char host[]);
	template<typename Socket, typename Downloader>
	void genericDownload(std::unique_ptr<Socket>& socket, const char host[], const Downloader& downloadMethod, int& retry) {
		try {
			downloadMethod(*socket);
			retry = 0;
		} catch (const sys::system_error& e) {
			retry++;
			if (e.code() == asio::error::in_progress) {
				std::cerr << "Lost connection to server while attempting to download, but some progress was made, keeping up the work." << std::endl;
				connectSocket(socket, host);
			} else if (e.code() == asio::error::eof) {
				std::cerr << "Lost connection to server while attempting to download, retrying." << std::endl;
				connectSocket(socket, host);
				// attempt twice to download and move on to the
				// next station
				if (retry >= 2) {
					std::cerr << "Tried twice already, moving on..." << std::endl;
					retry =  0;
				}
			} else {
				std::cerr << "Impossible to download, moving on..." << std::endl;
				retry =  0;
			}
		} catch (const std::runtime_error& e) {
			std::cerr << "Impossible to download, moving on..." << std::endl;
			retry =  0;
		}
	}
	void downloadArchives();
	void downloadRealTime();
	void checkDeadline(const sys::error_code& e);
	static constexpr int POLLING_PERIOD = 15; // in minutes
};

}

#endif
