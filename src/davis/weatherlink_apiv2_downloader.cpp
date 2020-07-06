/**
 * @file weatherlink_apiv2_downloader.cpp
 * @brief Implementation of the WeatherlinkApiv2Downloader class
 * @author Laurent Georget
 * @date 2019-09-19
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

#include <iostream>
#include <memory>
#include <functional>
#include <iterator>
#include <chrono>
#include <sstream>
#include <utility>
#include <cstring>
#include <cctype>
#include <syslog.h>
#include <unistd.h>

#include <boost/system/error_code.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/ssl.hpp>
#include <cassandra.h>
#include <dbconnection_observations.h>
#include <date.h>

#include "../time_offseter.h"
#include "../http_utils.h"
#include "../cassandra_utils.h"
#include "weatherlink_apiv2_realtime_message.h"
#include "weatherlink_apiv2_archive_page.h"
#include "weatherlink_apiv2_archive_message.h"
#include "weatherlink_apiv2_downloader.h"
#include "weatherlink_download_scheduler.h"

namespace asio = boost::asio;
namespace ip = boost::asio::ip;
namespace chrono = std::chrono;
namespace sys = boost::system;

namespace meteodata {

using namespace date;

WeatherlinkApiv2Downloader::WeatherlinkApiv2Downloader(
	const CassUuid& station, const std::string& weatherlinkId,
	const std::map<int, CassUuid>& mapping,
	const std::string& apiKey, const std::string& apiSecret,
	DbConnectionObservations& db,
	TimeOffseter::PredefinedTimezone tz) :
	AbstractWeatherlinkDownloader(station, db, tz),
	_apiKey(apiKey),
	_apiSecret(apiSecret),
	_weatherlinkId(weatherlinkId),
	_substations(mapping)
{
	for (const auto& s : _substations)
		_uuids.insert(s.second);
	_uuids.insert(station);
}

std::string WeatherlinkApiv2Downloader::computeApiSignature(const Params& params)
{
	std::string allParamsButApiSignature;
	for (const auto& param : params)
		allParamsButApiSignature += param.first + param.second;

	std::cerr << "Params for computing API signature: " << allParamsButApiSignature << std::endl;
	return computeHMACWithSHA256(allParamsButApiSignature, _apiSecret);
}

void WeatherlinkApiv2Downloader::downloadRealTime(BlockingTcpClient<asio::ssl::stream<ip::tcp::socket>>& client)
{
	std::cerr << "Downloading real-time data for station " << _stationName << std::endl;
	//
	// Form the request. We specify the "Connection: keep-alive" header so that the
	// will be usable by other downloaders.
	boost::asio::streambuf request;
	std::ostream requestStream(&request);

	WeatherlinkApiv2Downloader::Params params = {
		{"t", std::to_string(chrono::system_clock::to_time_t(chrono::system_clock::now()))},
		{"station-id", _weatherlinkId},
		{"api-key", _apiKey}
	};
	params.emplace("api-signature", computeApiSignature(params));
	std::ostringstream query;
	query << "/v2/current/" << _weatherlinkId
	      << "?"
	      << "api-key=" << params["api-key"] << "&"
	      << "api-signature=" << params["api-signature"] << "&"
	      << "t=" << params["t"];
	requestStream << "GET " << query.str() << " HTTP/1.0\r\n";
	std::cerr << "GET " << query.str() << " HTTP/1.0\r\n";
	requestStream << "Host: " << WeatherlinkDownloadScheduler::APIHOST << "\r\n";
	requestStream << "Connection: keep-alive\r\n";
	requestStream << "Accept: application/json\r\n\r\n";

	// Send the request.
	std::size_t bytesWrote;
	client.write(request, bytesWrote);
	std::cerr << "Query sent" << std::endl;

	// Read the response status line. The response streambuf will automatically
	// grow to accommodate the entire line. The growth may be limited by passing
	// a maximum size to the streambuf constructor.
	asio::streambuf response(WeatherlinkApiv2RealtimeMessage::MAXSIZE);
	std::istream responseStream{&response};

	try {
		getReponseFromHTTP10QueryFromClient(client, response, responseStream, WeatherlinkApiv2RealtimeMessage::MAXSIZE, "application/json");
		std::cerr << "Read all the content" << std::endl;

		// Store the content because if we must read it several times
		std::string content;
		std::copy(std::istream_iterator<char>(responseStream), std::istream_iterator<char>(), std::back_inserter(content));

		for (const auto& u : _uuids) {
			std::istringstream contentStream(content); // rewind

			// If there are no substations, there's a unique UUID equal to _station in the set
			WeatherlinkApiv2RealtimeMessage obs(&_timeOffseter);
			if (_substations.empty())
				obs.parse(contentStream);
			else
				obs.parse(contentStream, _substations, u);
			int ret = _db.insertV2DataPoint(u, obs); // Don't bother inserting V1
			if (!ret) {
				syslog(LOG_ERR, "station %s: Failed to insert real-time observation for one of the substations", _stationName.c_str());
				std::cerr << "station " << _stationName << ": Failed to insert real-time observation for substation " << u << std::endl;
			}
		}
	} catch (std::runtime_error& error) {
		syslog(LOG_ERR, "station %s: Bad response for %s", _stationName.c_str(), WeatherlinkDownloadScheduler::HOST);
		std::cerr << "station " << _stationName << " Bad response from " << WeatherlinkDownloadScheduler::APIHOST << std::endl;
		throw error;
	}
}

void WeatherlinkApiv2Downloader::download(BlockingTcpClient<asio::ssl::stream<ip::tcp::socket>>& client)
{
	std::cerr << "Downloading historical data for station " << _stationName << std::endl;
	//
	// Form the request. We specify the "Connection: keep-alive" header so that the
	// will be usable by other downloaders.
	boost::asio::streambuf request;
	std::ostream requestStream(&request);

	auto end = chrono::system_clock::now();
	auto date = _lastArchive;

	using namespace date;
	std::cerr << "Last archive dates back from " << _lastArchive << "; now is " << end << std::endl;
	std::cerr << "(approximately " << date::floor<date::days>(end - date) << " days)" << std::endl;
	while (date < end) {
		auto datePlus24Hours = date + chrono::hours{24};
		WeatherlinkApiv2Downloader::Params params = {
			{"t", std::to_string(chrono::system_clock::to_time_t(chrono::system_clock::now()))},
			{"station-id", _weatherlinkId},
			{"api-key", _apiKey},
			{"start-timestamp", std::to_string(chrono::system_clock::to_time_t(date))},
			{"end-timestamp", std::to_string(chrono::system_clock::to_time_t(datePlus24Hours))}
		};
		params.emplace("api-signature", computeApiSignature(params));
		std::ostringstream query;
		query << "/v2/historic/" << _weatherlinkId
		      << "?"
		      << "api-key=" << params["api-key"] << "&"
		      << "api-signature=" << params["api-signature"] << "&"
		      << "t=" << params["t"] << "&"
		      << "start-timestamp=" << params["start-timestamp"] << "&"
		      << "end-timestamp=" << params["end-timestamp"];
		requestStream << "GET " << query.str() << " HTTP/1.0\r\n";
		std::cerr << "GET " << query.str() << " HTTP/1.0\r\n";
		requestStream << "Host: " << WeatherlinkDownloadScheduler::APIHOST << "\r\n";
		requestStream << "Connection: keep-alive\r\n";
		requestStream << "Accept: application/json\r\n\r\n";

		// Send the request.
		std::size_t bytesWritten;
		client.write(request, bytesWritten);

		// Read the response status line. The response streambuf will automatically
		// grow to accommodate the entire line. The growth may be limited by passing
		// a maximum size to the streambuf constructor.
		asio::streambuf response(WeatherlinkApiv2RealtimeMessage::MAXSIZE);
		std::istream responseStream{&response};

		bool stillOpen = true;
		try {
			stillOpen = getReponseFromHTTP10QueryFromClient(client, response, responseStream, WeatherlinkApiv2RealtimeMessage::MAXSIZE, "application/json");
			std::cerr << "Read all the content" << std::endl;

			bool insertionOk = true;
			auto updatedTimestamp = _lastArchive;

			// Store the content because if we must read it several times
			std::string content;
			std::copy(std::istream_iterator<char>(responseStream), std::istream_iterator<char>(), std::back_inserter(content));

			for (const auto& u : _uuids) {
				std::istringstream contentStream(content); // rewind

				std::cerr << "Parsing output for substation " << u << std::endl;
				WeatherlinkApiv2ArchivePage page(_lastArchive, &_timeOffseter);
				if (_substations.empty())
					page.parse(contentStream);
				else
					page.parse(contentStream, _substations, u);

				std::cerr << "\tParsed output for substation " << u << std::endl;

				auto newestTimestamp = page.getNewestMessageTime();
				auto archiveDay = date::floor<date::days>(_lastArchive);
				auto lastDay = date::floor<date::days>(end);
				while (archiveDay <= lastDay) {
					int ret = _db.deleteDataPoints(u, archiveDay, _lastArchive, newestTimestamp);

					if (!ret)
						syslog(LOG_ERR, "station %s: Couldn't delete temporary realtime observations", _stationName.c_str());
					archiveDay += date::days(1);
				}
				for (const WeatherlinkApiv2ArchiveMessage& m : page) {
					int ret = _db.insertV2DataPoint(u, m); // Don't bother inserting V1
					if (!ret) {
						syslog(LOG_ERR, "station %s: Failed to insert archive observation for one of the substations", _stationName.c_str());
						std::cerr << "station " << _stationName << ": Failed to insert archive observation for (sub)station " << u << std::endl;
						insertionOk = false;
					}
				}
				if (insertionOk) {
					std::cerr << "Archive data stored\n" << std::endl;
					time_t lastArchiveDownloadTime = chrono::system_clock::to_time_t(newestTimestamp);
						std::cerr << "station " << _stationName << ": Newest timestamp " << lastArchiveDownloadTime << std::endl;
					insertionOk = _db.updateLastArchiveDownloadTime(_station, lastArchiveDownloadTime);
					if (!insertionOk) {
						syslog(LOG_ERR, "station %s: Couldn't update last archive download time", _stationName.c_str());
					} else {
						updatedTimestamp = newestTimestamp;
					}
				}
			}
			if (insertionOk)
				_lastArchive = updatedTimestamp;
		} catch (const sys::system_error& error) {
			if (error.code() == asio::error::eof) {
				syslog(LOG_ERR, "station %s: Socket looks closed for %s: %s", _stationName.c_str(), WeatherlinkDownloadScheduler::HOST, error.what());
				std::cerr << "station " << _stationName << " Socket looks closed " << WeatherlinkDownloadScheduler::APIHOST << ": " << error.what() << std::endl;
				throw sys::system_error{asio::error::in_progress}; // The socket is closed but we made some progress so we use a different code to indicate that we're not stuck in an endless failure cycle
			} else {
				syslog(LOG_ERR, "station %s: Bad response for %s: %s", _stationName.c_str(), WeatherlinkDownloadScheduler::HOST, error.what());
				std::cerr << "station " << _stationName << " Bad response from " << WeatherlinkDownloadScheduler::APIHOST << ": " << error.what() << std::endl;
				throw error;
			}

		} catch (std::runtime_error& error) {
			syslog(LOG_ERR, "station %s: Bad response for %s: %s", _stationName.c_str(), WeatherlinkDownloadScheduler::HOST, error.what());
			std::cerr << "station " << _stationName << " Bad response from " << WeatherlinkDownloadScheduler::APIHOST << ": " << error.what() << std::endl;
			throw error;
		}

		date += chrono::hours{24};
		if (date < end && !stillOpen)
			throw sys::system_error{asio::error::in_progress};
	}
}

}
