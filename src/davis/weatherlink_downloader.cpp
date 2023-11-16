/**
 * @file weatherlinkdownloader.cpp
 * @brief Implementation of the WeatherlinkDownloader class
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

#include <iostream>
#include <memory>
#include <functional>
#include <iterator>
#include <chrono>
#include <sstream>
#include <utility>
#include <cstring>
#include <cctype>
#include <systemd/sd-daemon.h>
#include <unistd.h>

#include <boost/system/error_code.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/basic_waitable_timer.hpp>
#include <cassandra.h>
#include <dbconnection_observations.h>

#include "../http_utils.h"
#include "../time_offseter.h"
#include "weatherlink_apiv1_realtime_message.h"
#include "weatherlink_downloader.h"
#include "weatherlink_download_scheduler.h"
#include "vantagepro2_message.h"
#include "vantagepro2_archive_page.h"
#include "../curl_wrapper.h"

namespace asio = boost::asio;
namespace ip = boost::asio::ip;
namespace sys = boost::system;
namespace chrono = std::chrono;
namespace args = std::placeholders;

namespace meteodata
{

using namespace date;

const std::string WeatherlinkDownloader::REALTIME_BASE_URL =
		std::string{"https://"} + WeatherlinkDownloadScheduler::APIHOST;
const std::string WeatherlinkDownloader::ARCHIVE_BASE_URL = std::string{"http://"} + WeatherlinkDownloadScheduler::HOST;

WeatherlinkDownloader::WeatherlinkDownloader(const CassUuid& station, std::string auth,
			std::string apiToken, DbConnectionObservations& db,
			TimeOffseter::PredefinedTimezone tz, AsyncJobPublisher* jobPublisher) :
		AbstractWeatherlinkDownloader(station, db, tz),
		_authentication{std::move(auth)},
		_apiToken{std::move(apiToken)}
{}

void WeatherlinkDownloader::downloadRealTime(CurlWrapper& client)
{
	if (_apiToken.empty())
		return; // no token, no realtime obs

	std::cout << SD_INFO << "[Weatherlink_v1 " << _station << "] measurement: "
			  << "downloading real-time data for station " << _stationName << std::endl;

	std::cout << SD_DEBUG << "[Weatherlink_v1 " << _station << "] protocol: " << "GET " << "/v1/NoaaExt.xml?"
			  << "user=XXXXXXXXX&pass=XXXXXXXXX&apiToken=XXXXXXXX" << " HTTP/1.1 " << "Host: "
			  << WeatherlinkDownloadScheduler::HOST << " " << "Accept: application/xml ";

	std::ostringstream query;
	query << "/v1/NoaaExt.xml" << "?" << _authentication << "&apiToken=" << _apiToken;
	std::string queryStr = query.str();

	client.setHeader("Accept", "application/xml");

	CURLcode ret = client.download(REALTIME_BASE_URL + queryStr, [&](const std::string& body) {
		std::istringstream responseStream(body);

		WeatherlinkApiv1RealtimeMessage message(&_timeOffseter);
		message.parse(responseStream);
		int dbRet = _db.insertV2DataPoint(message.getObservation(_station));

		if (!dbRet) {
			std::cerr << SD_ERR << "[Weatherlink_v1 " << _station << "] measurement: "
					  << "failed to insert real-time observation" << std::endl;
		}
	});

	if (ret != CURLE_OK) {
		logAndThrowCurlError(client, WeatherlinkDownloadScheduler::HOST);
	}
}

void WeatherlinkDownloader::download(CurlWrapper& client)
{
	std::cout << SD_INFO << "[Weatherlink_v1 " << _station << "] measurement: " << " now downloading for station "
			  << _stationName << std::endl;
	auto time = _timeOffseter.convertToLocalTime(_lastArchive);

	// by default, download the entire datalogger archive
	std::uint32_t timestamp = 0;
	if (_lastArchive > chrono::system_clock::now() - chrono::hours{96}) {
		// if the last archive is not too old, use that one as reference
		auto daypoint = date::floor<date::days>(time);
		auto ymd = date::year_month_day(daypoint);   // calendar date
		auto tod = date::make_time(time - daypoint); // Yields time_of_day type

		// Obtain individual components as integers
		auto y = int(ymd.year());
		auto m = unsigned(ymd.month());
		auto d = unsigned(ymd.day());
		auto h = tod.hours().count();
		auto min = tod.minutes().count();

		timestamp = ((y - 2000) << 25) + (m << 21) + (d << 16) + h * 100 + min;
	}

	std::cout << SD_DEBUG << "[Weatherlink_v1 " << _station << "] protocol: " << "GET " << "/webdl.php?timestamp="
			  << timestamp << "&user=XXXXXXXXXX&password=XXXXXXXXX&action=data" << " HTTP/1.1 " << "Host: "
			  << WeatherlinkDownloadScheduler::HOST << " " << "Accept: */* " << std::endl;

	std::ostringstream query;
	query << "/webdl.php" << "?" << "timestamp=" << timestamp << "&" << _authentication << "&action=data";
	std::string queryStr = query.str();

	client.setHeader("Accept", "*/*");

	CURLcode downloadRet = client.download(ARCHIVE_BASE_URL + queryStr, [&](const std::string& body) {
		if (body.size() % 52 != 0) {
			std::string errorMsg = std::string{"Incorrect response size from "} + WeatherlinkDownloadScheduler::HOST +
								   " when downloading archives";
			std::cerr << SD_ERR << "[Weatherlink_v1 " << _station << "] protocol: " << errorMsg << std::endl;
			throw std::runtime_error(errorMsg);
		}
		int pagesLeft = body.size() / 52;

		const auto* dataPoint = reinterpret_cast<const VantagePro2ArchiveMessage::ArchiveDataPoint*>(body.data());
		const VantagePro2ArchiveMessage::ArchiveDataPoint* pastLastDataPoint = dataPoint + pagesLeft;

		bool ret = true;
		auto start = date::floor<chrono::seconds>(chrono::system_clock::now());
		auto end = _lastArchive;

		std::vector<VantagePro2ArchiveMessage> messages;

		// Find the timestamp of the last valid data point (and constructs the messages while we are at it)
		for (; dataPoint < pastLastDataPoint ; ++dataPoint) {
			VantagePro2ArchiveMessage message{*dataPoint, &_timeOffseter};

			if (message.looksValid()) {
				auto time = message.getTimestamp();
				if (time < start)
					start = time;
				if (time > end)
					end = time;
				messages.emplace_back(message);
			} else {
				std::cerr << SD_WARNING << "[Weatherlink_v1 " << _station << "] measurement: "
						  << "record looks invalid, discarding..." << std::endl;
			}
		}

		auto day = date::floor<date::days>(start);
		auto lastDay = date::floor<date::days>(end);
		int i = 0;
		int LOG_FLOODING_LIMIT = 100;
		while (day <= lastDay) {
			ret = _db.deleteDataPoints(_station, day, start, end);

			if (!ret)
				std::cerr << SD_ERR << "[Weatherlink_v1 " << _station << "] management: "
					  << "couldn't delete temporary realtime observations "
					  << "between " << date::format("%Y-%m-%dT%H:%M", start)
					  << " and " << date::format("%Y-%m-%dT%H:%M", end)
					  << std::endl;
			day += date::days(1);

			// avoid flooding the log too much
			if (i % LOG_FLOODING_LIMIT == 0) {
				std::cerr << SD_DEBUG << "[Weatherlink_v1 " << _station << "] measurement: "
					  << "Data deleted until "
					  << date::format("%Y-%m-%d", day)
					  << std::endl;
			}
			i++;
		}
		std::cerr << SD_INFO << "[Weatherlink_v1 " << _station << "] management: "
			  << "Deleted temporary data "
			  << "between " << date::format("%Y-%m-%dT%H:%M", start)
			  << " and " << date::format("%Y-%m-%dT%H:%M", end)
			  << std::endl;

		i = 0;
		for (auto&& message : messages) {
			auto lastArchive = message.getTimestamp();
			if (lastArchive < _oldestArchive)
				_oldestArchive = lastArchive;
			if (lastArchive > _newestArchive)
				_newestArchive = lastArchive;
			ret = _db.insertV2DataPoint(message.getObservation(_station));
			// avoid flooding the log too much
			if (i % LOG_FLOODING_LIMIT == 0) {
				std::cerr << SD_DEBUG << "[Weatherlink_v1 " << _station << "] measurement: "
					  << "Data inserted until "
					  << date::format("%Y-%m-%dT%H:%M", lastArchive)
					  << std::endl;
			}
			i++;
		}

		if (!messages.empty() && ret) {
			std::cout << SD_DEBUG << "[Weatherlink_v1 " << _station << "] measurement: " << "archive data stored\n"
					  << std::endl;
			time_t lastArchiveDownloadTime = _newestArchive.time_since_epoch().count();
			ret = _db.updateLastArchiveDownloadTime(_station, lastArchiveDownloadTime);
			if (!ret)
				std::cerr << SD_ERR << "[Weatherlink_v1 " << _station << "] management: "
						  << "couldn't update last archive download time" << std::endl;

			if (_jobPublisher) {
				_jobPublisher->publishJobsForPastDataInsertion(_station, _oldestArchive, _newestArchive);
			}
		} else {
			std::cerr << SD_ERR << "[Weatherlink_v1 " << _station << "] measurement: "
					  << "failed to store archive! Aborting" << std::endl;
			return;
		}
	});

	if (downloadRet != CURLE_OK)
		logAndThrowCurlError(client, WeatherlinkDownloadScheduler::HOST);
}

void WeatherlinkDownloader::logAndThrowCurlError(CurlWrapper& client, const std::string& host)
{
	std::string_view error = client.getLastError();
	std::ostringstream errorStream;
	errorStream << "station " << _stationName << " Bad response from " << host << ": " << error;
	std::string errorMsg = errorStream.str();
	std::cerr << SD_ERR << "[Weatherlink_v1 " << _station << "] protocol: " << errorMsg << std::endl;
	throw std::runtime_error(errorMsg);
}

}
