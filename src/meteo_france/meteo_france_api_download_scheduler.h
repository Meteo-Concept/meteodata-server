/**
 * @file meteo_france_api_download_scheduler.h
 * @brief Definition of the MeteoFranceApiDownloadScheduler class
 * @author Laurent Georget
 * @date 2024-01-16
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

#ifndef METEOFRANCE_API_DOWNLOAD_SCHEDULER_H
#define METEOFRANCE_API_DOWNLOAD_SCHEDULER_H

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
#include <dbconnection_observations.h>

#include "async_job_publisher.h"
#include "meteo_france/meteo_france_api_downloader.h"
#include "abstract_download_scheduler.h"
#include "time_offseter.h"
#include "curl_wrapper.h"
#include "cassandra_utils.h"
#include "connector.h"

namespace meteodata
{

namespace asio = boost::asio;
namespace chrono = std::chrono;

using namespace std::placeholders;

/**
 */
class MeteoFranceApiDownloadScheduler : public AbstractDownloadScheduler
{
public:
	MeteoFranceApiDownloadScheduler(asio::io_context& ioContext,
		DbConnectionObservations& db, std::string apiKey,
		AsyncJobPublisher* jobPublisher = nullptr);
	void add(const CassUuid& station, const std::string& mfId);

private:
	const std::string _apiKey;
	AsyncJobPublisher* _jobPublisher;
	std::vector<std::shared_ptr<MeteoFranceApiDownloader>> _downloaders;
	bool _mustStop = false;

private:
	void download() override;
	void reloadStations() override;

	template<typename Downloader>
	void genericDownload(const Downloader& downloadMethod)
	{
		try {
			auto start = chrono::steady_clock::now();
			downloadMethod(_client);
			auto end = chrono::steady_clock::now();

			if (end - start < chrono::milliseconds(MeteoFranceApiDownloader::MIN_DELAY)) {
				// Wait for some time to respect the request cap
				// (50 per minute as of 2024-01-16)
				std::this_thread::sleep_for(chrono::milliseconds(MeteoFranceApiDownloader::MIN_DELAY) - (end - start));
			}
		} catch (const std::runtime_error& e) {
			std::cerr << SD_ERR << "[MeteoFrance] protocol: " << "Runtime error, impossible to download " << e.what()
					  << ", moving on..." << std::endl;
		}
	}

	// In a future version, we'll download observations on a 6-minute
	// timestep for a selection of stations

	/**
	 * The polling period that apply to all stations, in minutes
	 */
	static constexpr int UNPRIVILEGED_POLLING_PERIOD = 60;
	/**
	 * The minimal polling period, for stations authorized to get
	 * realtime data more frequently than others, in minutes
	 */
	static constexpr int POLLING_PERIOD = 6;

	/**
	 * The scheduler identifier for use in database
	 */
	static constexpr char SCHEDULER_ID[] = "meteo_france";
};

}

#endif
