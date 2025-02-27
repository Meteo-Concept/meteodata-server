/**
 * @file weatherlink_apiv2_download_scheduler.h
 * @brief Definition of the WeatherlinkApiv2DownloadScheduler class
 * @author Laurent Georget
 * @date 2023-11-16
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

#ifndef WEATHERLINKAPIV2DOWNLOADSCHEDULER_H
#define WEATHERLINKAPIV2DOWNLOADSCHEDULER_H

#include <memory>
#include <vector>
#include <string>
#include <chrono>
#include <map>
#include <thread>

#include <systemd/sd-daemon.h>
#include <boost/system/error_code.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio.hpp>
#include <boost/asio/basic_waitable_timer.hpp>
#include <cassandra.h>
#include <cassobs/dbconnection_observations.h>

#include "async_job_publisher.h"
#include "davis/weatherlink_apiv2_downloader.h"
#include "abstract_download_scheduler.h"
#include "time_offseter.h"
#include "curl_wrapper.h"
#include "cassandra_utils.h"
#include "connector.h"

namespace meteodata
{

namespace asio = boost::asio;
namespace sys = boost::system;
namespace chrono = std::chrono;

using namespace std::placeholders;

/**
 */
class WeatherlinkApiv2DownloadScheduler : public AbstractDownloadScheduler
{
public:
	WeatherlinkApiv2DownloadScheduler(asio::io_context& ioContext,
		DbConnectionObservations& db, std::string apiId, std::string apiSecret,
		AsyncJobPublisher* jobPublisher = nullptr);
	void add(const CassUuid& station, bool archived, const std::map<int, CassUuid>& substations,
		const std::map<int, std::map<std::string, std::string>>& parsers,
		const std::string& weatherlinkId, TimeOffseter&& to);

private:
	const std::string _apiId;
	const std::string _apiSecret;
	AsyncJobPublisher* _jobPublisher;
	std::vector<std::pair<bool, std::shared_ptr<WeatherlinkApiv2Downloader>>> _downloadersAPIv2;
	bool _mustStop = false;

public:
	static constexpr char APIHOST[] = "api.weatherlink.com";

	/**
	 * The polling period that apply to all stations, in minutes
	 */
	static constexpr int UNPRIVILEGED_POLLING_PERIOD = 15;
	/**
	 * The minimal polling period, for stations authorized to get
	 * realtime data more frequently than others, in minutes
	 */
	static constexpr int POLLING_PERIOD = 5;

private:
	void download() override;
	void reloadStations() override;

	template<typename Downloader>
	void genericDownload(const Downloader& downloadMethod)
	{
		try {
			auto start = std::chrono::steady_clock::now();
			downloadMethod(_client);
			auto end = std::chrono::steady_clock::now();

			if (end - start < std::chrono::milliseconds(100)) {
				// Wait for 100ms because the number of requests is
				// capped at 10 per second
				std::this_thread::sleep_for(chrono::milliseconds(100) - (end - start));
			}
		} catch (const std::runtime_error& e) {
			std::cerr << SD_ERR << "[Weatherlink v2] protocol: " << "Runtime error, impossible to download " << e.what()
				  << ", moving on..." << std::endl;
		}
	}

	void downloadArchives(int minutes);
	void downloadRealTime(int minutes);
};

}

#endif
