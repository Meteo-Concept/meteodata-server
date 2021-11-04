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
namespace sys = boost::system; //system() is a function, it cannot be redefined
//as a namespace
namespace chrono = std::chrono;
namespace args = std::placeholders;

namespace meteodata {

using namespace date;

const std::string WeatherlinkDownloader::REALTIME_BASE_URL = std::string{"https://"} + WeatherlinkDownloadScheduler::APIHOST;
const std::string WeatherlinkDownloader::ARCHIVE_BASE_URL = std::string{"http://"} + WeatherlinkDownloadScheduler::HOST;

WeatherlinkDownloader::WeatherlinkDownloader(const CassUuid& station, const std::string& auth,
	const std::string& apiToken, DbConnectionObservations& db,
	TimeOffseter::PredefinedTimezone tz) :
	AbstractWeatherlinkDownloader(station, db, tz),
	_authentication{auth},
	_apiToken{apiToken}
{}

void WeatherlinkDownloader::downloadRealTime(CurlWrapper& client)
{
	if (_apiToken.empty())
		return; // no token, no realtime obs

	std::cout << SD_INFO << "[Weatherlink_v1 " << _station << "] measurement: "
	    << "downloading real-time data for station " << _stationName << std::endl;

	std::cout << SD_DEBUG << "[Weatherlink_v1 " << _station << "] protocol: "
	    << "GET " << "/v1/NoaaExt.xml?"  << "user=XXXXXXXXX&pass=XXXXXXXXX&apiToken=XXXXXXXX" << " HTTP/1.1 "
	    << "Host: " << WeatherlinkDownloadScheduler::HOST << " "
	    << "Accept: application/xml ";

	std::ostringstream query;
	query << "/v1/NoaaExt.xml"
	      << "?"
	      << _authentication
	      << "&apiToken="
	      << _apiToken;
	std::string queryStr = query.str();

	client.setHeader("Accept", "application/xml");

	CURLcode ret = client.download(REALTIME_BASE_URL + queryStr, [&](const std::string& body) {
		std::istringstream responseStream(body);

		WeatherlinkApiv1RealtimeMessage obs(&_timeOffseter);
		obs.parse(responseStream);
		int dbRet = _db.insertV2DataPoint(_station, obs); // Don't bother inserting V1

		if (!dbRet) {
			std::cerr << SD_ERR << "[Weatherlink_v1 " << _station << "] measurement: "
				  << "failed to insert real-time observation"
				  << std::endl;
		}
	});

	if (ret != CURLE_OK) {
		logAndThrowCurlError(client, WeatherlinkDownloadScheduler::APIHOST);
	}
}

void WeatherlinkDownloader::download(CurlWrapper& client)
{
	std::cout << SD_INFO << "[Weatherlink_v1 " << _station << "] measurement: "
	    << " now downloading for station " << _stationName << std::endl;
	auto time = _timeOffseter.convertToLocalTime(_lastArchive);
	auto daypoint = date::floor<date::days>(time);
	auto ymd = date::year_month_day(daypoint);   // calendar date
	auto tod = date::make_time(time - daypoint); // Yields time_of_day type

	// Obtain individual components as integers
	auto y   = int(ymd.year());
	auto m   = unsigned(ymd.month());
	auto d   = unsigned(ymd.day());
	auto h   = tod.hours().count();
	auto min = tod.minutes().count();

	std::uint32_t timestamp = ((y - 2000) << 25) + (m << 21) + (d << 16) + h * 100 + min;

	std::cout << SD_DEBUG << "[Weatherlink_v2 " << _station << "] protocol: "
	    << "GET " << "/webdl.php?timestamp=" << timestamp << "&user=XXXXXXXXXX&password=XXXXXXXXX&action=data" << " HTTP/1.1 "
	    << "Host: " << WeatherlinkDownloadScheduler::HOST << " "
	    << "Accept: */* "
	    << std::endl;

	std::ostringstream query;
	query << "/webdl.php"
	      << "?"
	      << "timestamp=" << timestamp
	      << "&"
	      << _authentication
	      << "&action=data";
	std::string queryStr = query.str();

	client.setHeader("Accept", "*/*");

	CURLcode downloadRet = client.download(ARCHIVE_BASE_URL + queryStr, [&](const std::string& body) {
		if (body.size() % 52 != 0) {
			std::string errorMsg = std::string{"Incorrect response size from "} + WeatherlinkDownloadScheduler::HOST + " when downloading archives";
			std::cerr << SD_ERR << "[Weatherlink_v1 " << _station << "] protocol: "
			    << errorMsg << std::endl;
			throw std::runtime_error(errorMsg);
		}
		int pagesLeft = body.size() / 52;

		const VantagePro2ArchiveMessage::ArchiveDataPoint* dataPoint = reinterpret_cast<const VantagePro2ArchiveMessage::ArchiveDataPoint*>(body.data());
		const VantagePro2ArchiveMessage::ArchiveDataPoint* pastLastDataPoint = dataPoint + pagesLeft;

		bool ret = true;
		auto start = _lastArchive;

		for ( ; dataPoint < pastLastDataPoint && ret ; ++dataPoint) {
			VantagePro2ArchiveMessage message{*dataPoint, &_timeOffseter};

			if (message.looksValid()) {
				_lastArchive = message.getTimestamp();
				auto end = _lastArchive;
				auto day = date::floor<date::days>(start);
				auto lastDay = date::floor<date::days>(end);
				while (day <= lastDay) {
					ret = _db.deleteDataPoints(_station, day, start, end);

					if (!ret)
						std::cerr << SD_ERR << "[Weatherlink_v1 " << _station << "] management: "
							  << "couldn't delete temporary realtime observations"
							  << std::endl;
					day += date::days(1);
				}

				start = end;
				ret = _db.insertV2DataPoint(_station, message);
			} else {
				std::cerr << SD_WARNING << "[Weatherlink_v1 " << _station << "] measurement: "
					  << "record looks invalid, discarding..."
					  << std::endl;
			}
			//Otherwise, just discard
		}

		if (ret) {
			std::cout << SD_DEBUG << "[Weatherlink_v1 " << _station << "] measurement: "
				  << "archive data stored\n"
				  << std::endl;
			time_t lastArchiveDownloadTime = _lastArchive.time_since_epoch().count();
			ret = _db.updateLastArchiveDownloadTime(_station, lastArchiveDownloadTime);
			if (!ret)
				std::cerr << SD_ERR << "[Weatherlink_v1 " << _station << "] management: "
					  << "couldn't update last archive download time"
					  << std::endl;
		} else {
			std::cerr << SD_ERR << "[Weatherlink_v1 " << _station << "] measurement: "
				  << "failed to store archive! Aborting"
				  << std::endl;
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
	std::cerr << SD_ERR << "[Weatherlink_v1 " << _station << "] protocol: " <<  errorMsg << std::endl;
	throw std::runtime_error(errorMsg);
}

}
