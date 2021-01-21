/**
 * @file pessl_apiv2_downloader.cpp
 * @brief Implementation of the FieldClimateApiDownloader class
 * @author Laurent Georget
 * @date 2020-09-01
 */
/*
 * Copyright (C) 2020  SAS JD Environnement <contact@meteo-concept.fr>
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

#include <string>
#include <tuple>
#include <map>

#include <boost/system/error_code.hpp>
#include <boost/asio.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <dbconnection_observations.h>
#include <date.h>
#include <syslog.h>
#include <cassandra.h>

#include "fieldclimate_api_download_scheduler.h"
#include "fieldclimate_api_downloader.h"
#include "fieldclimate_archive_message_collection.h"
#include "fieldclimate_archive_message.h"
#include "../time_offseter.h"
#include "../http_utils.h"
#include "../curl_wrapper.h"

namespace meteodata {

constexpr char FieldClimateApiDownloader::APIHOST[];
const std::string FieldClimateApiDownloader::BASE_URL = std::string{"https://"} + FieldClimateApiDownloader::APIHOST + "/v2";

namespace asio = boost::asio;
namespace chrono = std::chrono;

using namespace meteodata;

FieldClimateApiDownloader::FieldClimateApiDownloader(const CassUuid& station,
		const std::string& fieldclimateId,
		const std::map<std::string, std::string>& sensors,
		DbConnectionObservations& db,
		TimeOffseter::PredefinedTimezone tz,
		const std::string& apiKey, const std::string& apiSecret) :
	_station(station),
	_fieldclimateId(fieldclimateId),
	_sensors(sensors),
	_db(db),
	_apiKey(apiKey),
	_apiSecret(apiSecret)
{
	time_t lastArchiveDownloadTime;
	db.getStationDetails(station, _stationName, _pollingPeriod, lastArchiveDownloadTime);
	float latitude, longitude;
	int elevation;
	db.getStationLocation(station, latitude, longitude, elevation);
	_lastArchive = date::sys_seconds(chrono::seconds(lastArchiveDownloadTime));
	_timeOffseter = TimeOffseter::getTimeOffseterFor(tz);
	_timeOffseter.setLatitude(latitude);
	_timeOffseter.setLongitude(longitude);
	_timeOffseter.setElevation(elevation);
	_timeOffseter.setMeasureStep(_pollingPeriod);
	std::cerr << "Discovered Pessl station " << _stationName << std::endl;
}

date::sys_seconds FieldClimateApiDownloader::getLastDatetimeAvailable(CurlWrapper& client)
{
	std::cerr << "Checking if new data is available for station " << _stationName << std::endl;

	using namespace date;

	std::string route = "/data/" + _fieldclimateId;
	std::string authorization;
	std::string headerDate;
	std::tie(authorization, headerDate) = computeAuthorizationAndDateFields("GET", route);

	std::cerr << "GET " << "/v2" << route << " HTTP/1.1\r\n"
		<< "Date: " << headerDate << "\r\n"
		<< "Authorization: " << authorization << "\r\n"
		<< "Accept: application/json\r\n\r\n";
	client.setHeader("Authorization", authorization);
	client.setHeader("Date", headerDate);
	client.setHeader("Accept", "application/json");

	date::sys_seconds dateInUTC;

	auto ret = client.download(BASE_URL + route, [&](const std::string& body) {
		std::istringstream bodyStream{body};
		pt::ptree jsonTree;
		pt::read_json(bodyStream, jsonTree);
		const std::string& maxDate = jsonTree.get<std::string>("max_date");
		std::istringstream dateStream{maxDate};
		date::local_seconds date;
		dateStream >> date::parse("%Y-%m-%d %H:%M:%S", date);
		dateInUTC = _timeOffseter.convertFromLocalTime(date);
		std::cerr << "Last date: " << dateInUTC << std::endl;
	});

	if (ret != CURLE_OK) {
		logAndThrowCurlError(client);
	}

	return dateInUTC;
}

void FieldClimateApiDownloader::download(CurlWrapper& client)
{
	std::cerr << "Downloading historical data for station " << _stationName << std::endl;

	// Form the request. We specify the "Connection: keep-alive" header so that the
	// will be usable by other downloaders.
	boost::asio::streambuf request;
	std::ostream requestStream(&request);

	auto date = _lastArchive;

	// may throw
	date::sys_seconds lastAvailable = getLastDatetimeAvailable(client);
	if (lastAvailable <= _lastArchive) {
		std::cerr << "No new data available for station " << _stationName << ", bailing off" << std::endl;
		return;
	}

	using namespace date;
	std::cerr << "Last archive dates back from " << _lastArchive << "; last available is " << lastAvailable << std::endl;
	std::cerr << "(approximately " << date::floor<date::days>(lastAvailable - date) << " days)" << std::endl;
	while (date < lastAvailable) {
		auto datePlus24Hours = date + chrono::hours{24};
		std::ostringstream routeBuilder;
		routeBuilder << "/data/"
			<< _fieldclimateId
			<< "/raw/from/"
			<< chrono::system_clock::to_time_t(date)
			<< "/to/"
			<< chrono::system_clock::to_time_t(datePlus24Hours);
		std::string route = routeBuilder.str();
		std::string authorization;
		std::string headerDate;
		std::tie(authorization, headerDate) = computeAuthorizationAndDateFields("GET", route);

		client.setHeader("Authorization", authorization);
		client.setHeader("Date", headerDate);
		client.setHeader("Accept", "application/json");

		std::cerr << "GET " << "/v2" << route << " HTTP/1.1\r\n"
			<< "Host: " << APIHOST << "\r\n"
			<< "Date: " << headerDate << "\r\n"
			<< "Authorization: " << authorization << "\r\n"
			<< "Accept: application/json\r\n\r\n";


		CURLcode ret = client.download(BASE_URL + route, [&](const std::string& body) {
			std::cerr << "Read all the content" << std::endl;
			std::istringstream responseStream(body);

			bool insertionOk = true;

			FieldClimateApiArchiveMessageCollection collection{&_timeOffseter, &_sensors};
			collection.parse(responseStream);

			if (collection.begin() != collection.end()) {
				// Not having data can happen if the station
				// malfunctioned
				auto newestTimestamp = collection.getNewestMessageTime();
				auto archiveDay = date::floor<date::days>(_lastArchive);
				auto lastDay = date::floor<date::days>(newestTimestamp);
				while (archiveDay <= lastDay) {
					int ret = _db.deleteDataPoints(_station, archiveDay, _lastArchive, newestTimestamp);

					if (!ret)
						syslog(LOG_ERR, "station %s: Couldn't delete replaced observations", _stationName.c_str());
					archiveDay += date::days(1);
				}
				for (const FieldClimateApiArchiveMessage& m : collection) {
					int ret = _db.insertV2DataPoint(_station, m); // Cannot insert V1
					if (!ret) {
						syslog(LOG_ERR, "station %s: Failed to insert archive observation for station", _stationName.c_str());
						std::cerr << "station " << _stationName << ": Failed to insert archive observation for station " << _stationName << std::endl;
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
						_lastArchive = newestTimestamp;
					}
				}
			}
		});

		if (ret != CURLE_OK) {
			logAndThrowCurlError(client);
		}

		date += chrono::hours{24};
	}
}

void FieldClimateApiDownloader::downloadRealTime(CurlWrapper& client)
{
	std::cerr << "Downloading real-time data for station " << _stationName << std::endl;

	std::ostringstream routeBuilder;
	routeBuilder << "/data/"
		<< _fieldclimateId
		<< "/raw/last/1";
	std::string route = routeBuilder.str();
	std::string authorization;
	std::string headerDate;
	std::tie(authorization, headerDate) = computeAuthorizationAndDateFields("GET", route);

	client.setHeader("Authorization", authorization);
	client.setHeader("Date", headerDate);
	client.setHeader("Accept", "application/json");

	std::cerr << "GET " << "/v2" << route << " HTTP/1.1\r\n"
		<< "Host: " << APIHOST << "\r\n"
		<< "Date: " << headerDate << "\r\n"
		<< "Authorization: " << authorization << "\r\n"
		<< "Accept: application/json\r\n\r\n";


	CURLcode ret = client.download(BASE_URL + route, [&,this](const std::string& body) {
		std::istringstream responseStream{body};

		std::cerr << "Read all the content" << std::endl;

		FieldClimateApiArchiveMessageCollection collection{&_timeOffseter, &_sensors};
		collection.parse(responseStream);

		if (collection.begin() == collection.end()) {
			// No data
			return;
		}

		bool insertionOk = true;

		// we expect exactly one message in the collection
		const FieldClimateApiArchiveMessage& m = *(collection.begin());
		int ret = _db.insertV2DataPoint(_station, m); // Cannot insert V1
		if (!ret) {
			syslog(LOG_ERR, "station %s: Failed to insert realtime observation for station", _stationName.c_str());
			std::cerr << "station " << _stationName << ": Failed to insert realtime observation for station " << _stationName << std::endl;
			insertionOk = false;
		}

		if (insertionOk) {
			std::cerr << "Realtime data stored\n" << std::endl;
		}
	});

	if (ret != CURLE_OK) {
		logAndThrowCurlError(client);
	}
}

std::tuple<std::string, std::string> FieldClimateApiDownloader::computeAuthorizationAndDateFields(const std::string& method, const std::string& route)
{
	std::string date = date::format("%a, %d %b %Y %T GMT", chrono::system_clock::now());
	std::string sig = computeHMACWithSHA256(method + route + date + _apiKey, _apiSecret);
	return { "hmac " + _apiKey + ":" + std::move(sig), std::move(date) };
}

void FieldClimateApiDownloader::logAndThrowCurlError(CurlWrapper& client)
{
		std::string_view error = client.getLastError();
		std::ostringstream errorStream;
		errorStream << "station " << _stationName << " Bad response from " << APIHOST << ": " << error;
		std::string errorMsg = errorStream.str();
		syslog(LOG_ERR, "%s", errorMsg.data());
		std::cerr << errorMsg << std::endl;
		throw std::runtime_error(errorMsg);
}

}
