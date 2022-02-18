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
#include <systemd/sd-daemon.h>
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
#include "../curl_wrapper.h"
#include "weatherlink_apiv2_realtime_message.h"
#include "weatherlink_apiv2_archive_page.h"
#include "weatherlink_apiv2_archive_message.h"
#include "weatherlink_apiv2_downloader.h"
#include "weatherlink_download_scheduler.h"

namespace asio = boost::asio;
namespace ip = boost::asio::ip;
namespace chrono = std::chrono;
namespace sys = boost::system;

namespace meteodata
{

using namespace date;

const std::string WeatherlinkApiv2Downloader::BASE_URL =
		std::string{"https://"} + WeatherlinkDownloadScheduler::APIHOST;

WeatherlinkApiv2Downloader::WeatherlinkApiv2Downloader(const CassUuid& station, const std::string& weatherlinkId,
													   const std::map<int, CassUuid>& mapping,
													   const std::string& apiKey, const std::string& apiSecret,
													   DbConnectionObservations& db, TimeOffseter&& to) :
		AbstractWeatherlinkDownloader(station, db, std::forward<TimeOffseter&&>(to)),
		_apiKey(apiKey),
		_apiSecret(apiSecret),
		_weatherlinkId(weatherlinkId),
		_substations(mapping)
{
	for (const auto& s : _substations)
		_uuids.insert(s.second);
	_uuids.insert(station);
}

WeatherlinkApiv2Downloader::WeatherlinkApiv2Downloader(const CassUuid& station, const std::string& weatherlinkId,
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

std::string WeatherlinkApiv2Downloader::computeApiSignature(const Params& params, const std::string& apiSecret)
{
	std::string allParamsButApiSignature;
	for (const auto& param : params)
		allParamsButApiSignature += param.first + param.second;
	return computeHMACWithSHA256(allParamsButApiSignature, apiSecret);
}

std::unordered_map<std::string, pt::ptree>
WeatherlinkApiv2Downloader::downloadAllStations(CurlWrapper& client, const std::string& apiId,
												const std::string& apiSecret)
{
	WeatherlinkApiv2Downloader::Params params = {{"t",       std::to_string(
			chrono::system_clock::to_time_t(chrono::system_clock::now()))},
												 {"api-key", apiId}};
	params.emplace("api-signature", computeApiSignature(params, apiSecret));
	std::ostringstream query;
	query << "/v2/stations" << "?" << "api-key=" << params["api-key"] << "&" << "api-signature="
		  << params["api-signature"] << "&" << "t=" << params["t"];
	std::string queryStr = query.str();
	std::cout << SD_DEBUG << "[Weatherlink_v2] protocol: " << "GET " << queryStr << " HTTP/1.1 " << "Host: "
			  << WeatherlinkDownloadScheduler::APIHOST << " " << "Accept: application/json ";

	client.setHeader("Accept", "application/json");

	std::unordered_map<std::string, pt::ptree> stations;
	client.download(BASE_URL + queryStr, [&](const std::string& content) {
		std::istringstream contentStream(content);
		pt::ptree jsonTree;
		pt::read_json(contentStream, jsonTree);

		for (std::pair<const std::string, pt::ptree>& st : jsonTree.get_child("stations")) {
			auto id = st.second.get<std::string>("station_id");
			stations.insert_or_assign(id, st.second);
		}
	});

	return stations;
}

void WeatherlinkApiv2Downloader::downloadRealTime(CurlWrapper& client)
{
	std::cout << SD_INFO << "[Weatherlink_v2 " << _station << "] measurement: "
			  << "downloading real-time data for station " << _stationName << std::endl;

	WeatherlinkApiv2Downloader::Params params = {{"t",          std::to_string(
			chrono::system_clock::to_time_t(chrono::system_clock::now()))},
												 {"station-id", _weatherlinkId},
												 {"api-key",    _apiKey}};
	params.emplace("api-signature", computeApiSignature(params, _apiSecret));
	std::ostringstream query;
	query << "/v2/current/" << _weatherlinkId << "?" << "api-key=" << params["api-key"] << "&" << "api-signature="
		  << params["api-signature"] << "&" << "t=" << params["t"];
	std::string queryStr = query.str();
	std::cout << SD_DEBUG << "[Weatherlink_v2 " << _station << "] protocol: " << "GET " << queryStr << " HTTP/1.1 "
			  << "Host: " << WeatherlinkDownloadScheduler::APIHOST << " " << "Accept: application/json ";

	client.setHeader("Accept", "application/json");

	CURLcode ret = client.download(BASE_URL + queryStr, [&](const std::string& content) {
		for (const auto& u : _uuids) {
			std::istringstream contentStream(content); // rewind

			// If there are no substations, there's a unique UUID equal to _station in the set
			float rainfall;
			std::optional<float> maybeRainfall{
					0.f}; // force rainfall to 0 to store all the rain for the day at the moment of the observation in the worst case
			auto now = chrono::system_clock::now();
			date::local_seconds localMidnight = date::floor<date::days>(_timeOffseter.convertToLocalTime(now));
			date::sys_seconds localMidnightInUTC = _timeOffseter.convertFromLocalTime(localMidnight);
			std::time_t beginDay = chrono::system_clock::to_time_t(localMidnightInUTC);
			if (_db.getRainfall(u, beginDay, chrono::system_clock::to_time_t(now), rainfall))
				maybeRainfall = rainfall;

			WeatherlinkApiv2RealtimeMessage obs(&_timeOffseter, maybeRainfall);
			if (_substations.empty())
				obs.parse(contentStream);
			else
				obs.parse(contentStream, _substations, u);
			int ret = _db.insertV2DataPoint(obs.getObservation(u));
			if (!ret) {
				std::cerr << SD_ERR << "[Weatherlink_v2 " << _station << "] measurement: "
						  << "Failed to insert real-time observation for substation " << u << std::endl;
			}
		}
	});

	if (ret != CURLE_OK) {
		logAndThrowCurlError(client);
	}
}

void WeatherlinkApiv2Downloader::download(CurlWrapper& client, bool force)
{
	std::cout << SD_INFO << "[Weatherlink_v2 " << _station << "] measurement: "
			  << "Weatherlink APIv2: downloading historical data for station " << _stationName << std::endl;

	auto end = chrono::system_clock::now();
	auto date = _lastArchive;

	using namespace date;
	auto days = date::floor<date::days>(end - date);
	std::cout << SD_DEBUG << "[Weatherlink_v2 " << _station << "] measurement: " << "Last archive dates back from "
			  << _lastArchive << "; now is " << end << "\n" << "(approximately " << days << " days)" << std::endl;

	if (days.count() > MAX_DISCONNECTION_DAYS && !force) {
		std::cout << SD_ERR << "[Weatherlink_v2 " << _station << "] connection: " << "Station " << _stationName
				  << " has been disconnected for more than " << MAX_DISCONNECTION_DAYS
				  << " days, not downloading without --force, " << "please reset the station manually" << std::endl;
		return;
	}

	bool stationIsDisconnected = false;
	if (days.count() > 1) {
		WeatherlinkApiv2Downloader::Params params = {{"t",          std::to_string(
				chrono::system_clock::to_time_t(chrono::system_clock::now()))},
													 {"station-id", _weatherlinkId},
													 {"api-key",    _apiKey}};
		params.emplace("api-signature", computeApiSignature(params, _apiSecret));
		std::ostringstream query;
		query << "/v2/current/" << _weatherlinkId << "?" << "api-key=" << params["api-key"] << "&" << "api-signature="
			  << params["api-signature"] << "&" << "t=" << params["t"];
		std::string queryStr = query.str();
		client.setHeader("Accept", "application/json");

		CURLcode ret = client.download(BASE_URL + queryStr, [&](const std::string& content) {
			std::vector<date::sys_seconds> lastUpdates;
			for (const auto& u : _uuids) {
				std::istringstream contentStream(content); // rewind
				WeatherlinkApiv2RealtimeMessage obs(&_timeOffseter, std::nullopt);
				lastUpdates.push_back(obs.getLastUpdateTimestamp(contentStream, _substations, u));
			}
			stationIsDisconnected = std::all_of(lastUpdates.begin(), lastUpdates.end(),
												[this](auto&& ts) { return ts <= _lastArchive; });
			if (!lastUpdates.empty()) {
				end = *std::max_element(lastUpdates.begin(), lastUpdates.end());
				std::cout << SD_DEBUG << "[Weatherlink_v2 " << _station << "] management: "
						  << "most recent update on Weatherlink: " << end << std::endl;
			}
		});

		if (ret != CURLE_OK) {
			if (client.getLastRequestCode() == 403) {
				std::cout << SD_ERR << "[Weatherlink_v2 " << _station << "] connect: "
						  << "Impossible to get archive for station " << _stationName << ", "
						  << "please check that it's still got a PRO subscription" << std::endl;
			}
			logAndThrowCurlError(client);
		}
	}

	if (stationIsDisconnected) {
		std::cerr << SD_ERR << "[Weatherlink_v2 " << _station << "] connection: " << "station " << _station
				  << " looks disconnected from Weatherlink" << std::endl;
		// early return, there's no point in trying to download data
		return;
	}

	while (date < end) {
		auto datePlus24Hours = date + chrono::hours{24};
		WeatherlinkApiv2Downloader::Params params = {{"t",               std::to_string(
				chrono::system_clock::to_time_t(chrono::system_clock::now()))},
													 {"station-id",      _weatherlinkId},
													 {"api-key",         _apiKey},
													 {"start-timestamp", std::to_string(
															 chrono::system_clock::to_time_t(date))},
													 {"end-timestamp",   std::to_string(
															 chrono::system_clock::to_time_t(datePlus24Hours))}};
		params.emplace("api-signature", computeApiSignature(params, _apiSecret));
		std::ostringstream query;
		query << "/v2/historic/" << _weatherlinkId << "?" << "api-key=" << params["api-key"] << "&" << "api-signature="
			  << params["api-signature"] << "&" << "t=" << params["t"] << "&" << "start-timestamp="
			  << params["start-timestamp"] << "&" << "end-timestamp=" << params["end-timestamp"];
		std::string queryStr = query.str();
		std::cout << SD_DEBUG << "[Weatherlink_v2 " << _station << "] protocol: " << "GET " << queryStr << " HTTP/1.1 "
				  << "Host: " << WeatherlinkDownloadScheduler::APIHOST << " " << "Accept: application/json ";

		client.setHeader("Accept", "application/json");

		CURLcode ret = client.download(BASE_URL + queryStr, [&](const std::string& content) {
			bool insertionOk = true;
			auto archiveDay = date::floor<date::days>(_lastArchive);
			auto referenceTimestamp = _lastArchive;

			for (const auto& u : _uuids) {
				std::istringstream contentStream(content); // rewind

				std::cout << SD_DEBUG << "[Weatherlink_v2 " << _station << "] measurement: "
						  << " parsing output for substation " << u << std::endl;
				WeatherlinkApiv2ArchivePage page(_lastArchive, &_timeOffseter);
				if (_substations.empty())
					page.parse(contentStream);
				else
					page.parse(contentStream, _substations, u);

				auto newestTimestamp = page.getNewestMessageTime();
				// find the oldest of all the newest records
				// of each substation
				if (newestTimestamp < referenceTimestamp || referenceTimestamp == _lastArchive)
					referenceTimestamp = newestTimestamp;

				auto lastDay = date::floor<date::days>(end);
				if (newestTimestamp <= _lastArchive) {
					std::cerr << SD_WARNING << "[Weatherlink_v2 " << _station << "] measurement: "
							  << "no new archive observation for substation " << u << std::endl;
					continue;
				}

				while (archiveDay <= lastDay) {
					int ret = _db.deleteDataPoints(u, archiveDay, _lastArchive, newestTimestamp);

					if (!ret)
						std::cerr << SD_ERR << "[Weatherlink_v2 " << _station << "] management: "
								  << "couldn't delete temporary realtime observations" << std::endl;
					archiveDay += date::days(1);
				}
				for (const WeatherlinkApiv2ArchiveMessage& m : page) {
					int ret = _db.insertV2DataPoint(m.getObservation(u));
					if (!ret) {
						std::cerr << SD_ERR << "[Weatherlink_v2 " << _station << "] measurement: "
								  << "failed to insert archive observation for substation " << u << std::endl;
						insertionOk = false;
					}
				}
			}

			if (insertionOk) {
				std::cout << SD_INFO << "[Weatherlink_v2 " << _station << "] measurement: " << "archive data stored"
						  << std::endl;
				time_t lastArchiveDownloadTime = chrono::system_clock::to_time_t(referenceTimestamp);
				insertionOk = _db.updateLastArchiveDownloadTime(_station, lastArchiveDownloadTime);
				if (!insertionOk) {
					std::cerr << SD_ERR << "[Weatherlink_v2 " << _station << "] management: "
							  << "couldn't update last archive download time" << std::endl;
				} else {
					_lastArchive = referenceTimestamp;
				}
			}
		});

		if (ret != CURLE_OK)
			logAndThrowCurlError(client);

		date += chrono::hours{24};
	}
}

void WeatherlinkApiv2Downloader::logAndThrowCurlError(CurlWrapper& client)
{
	std::string_view error = client.getLastError();
	std::ostringstream errorStream;
	errorStream << "station " << _stationName << " Bad response from " << WeatherlinkDownloadScheduler::APIHOST << ": "
				<< error;
	std::string errorMsg = errorStream.str();
	std::cout << SD_DEBUG << "[Weatherlink_v2 " << _station << "] protocol: " << errorMsg << std::endl;
	throw std::runtime_error(errorMsg);
}

}
