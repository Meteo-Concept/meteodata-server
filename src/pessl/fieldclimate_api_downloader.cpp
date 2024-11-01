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

#include <boost/asio.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <dbconnection_observations.h>
#include <date.h>
#include <systemd/sd-daemon.h>
#include <cassandra.h>

#include "http_utils.h"
#include "cassandra_utils.h"
#include "async_job_publisher.h"
#include "pessl/fieldclimate_api_downloader.h"
#include "pessl/fieldclimate_archive_message_collection.h"
#include "pessl/fieldclimate_archive_message.h"

namespace meteodata
{

namespace asio = boost::asio;
namespace chrono = std::chrono;

using namespace meteodata;

FieldClimateApiDownloader::FieldClimateApiDownloader(const CassUuid& station, std::string fieldclimateId,
	std::map<std::string, std::string> sensors, DbConnectionObservations& db, TimeOffseter::PredefinedTimezone tz,
	const std::string& apiKey, const std::string& apiSecret, AsyncJobPublisher* jobPublisher) :
		_station{station},
		_fieldclimateId{std::move(fieldclimateId)},
		_sensors{std::move(sensors)},
		_db{db},
		_jobPublisher{jobPublisher},
		_apiKey{apiKey},
		_apiSecret{apiSecret},
		_pollingPeriod{0}
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
	std::cout << SD_DEBUG << "[Pessl " << _station << "] connection: " << "Discovered Pessl station " << _stationName
			  << std::endl;
}

date::sys_seconds FieldClimateApiDownloader::getLastDatetimeAvailable(CurlWrapper& client)
{
	std::cout << SD_INFO << "[Pessl " << _station << "] management: "
			  << "Checking if new data is available for Pessl station " << _stationName << std::endl;

	using namespace date;

	std::string route = "/data/" + _fieldclimateId;
	std::string authorization;
	std::string headerDate;
	std::tie(authorization, headerDate) = computeAuthorizationAndDateFields("GET", route);

	std::cout << SD_DEBUG << "[Pessl " << _station << "] protocol: " << "GET " << "/v2" << route << " HTTP/1.1 "
			  << "Date: " << headerDate << " " << "Authorization: " << authorization << " "
			  << "Accept: application/json ";
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
	});

	if (ret != CURLE_OK) {
		logAndThrowCurlError(client);
	}

	return dateInUTC;
}

void FieldClimateApiDownloader::download(CurlWrapper& client)
{
	std::cout << SD_INFO << "[Pessl " << _station << "] measurement: "
			  << "Downloading historical data for Pessl station " << _stationName << std::endl;

	auto date = _lastArchive;

	// may throw
	date::sys_seconds lastAvailable = getLastDatetimeAvailable(client);
	if (lastAvailable <= _lastArchive) {
		std::cout << SD_DEBUG << "[Pessl " << _station << "] measurement: "
				  << "No new data available for Pessl station " << _stationName << ", bailing off" << std::endl;
		return;
	}

	using namespace date;
	std::cout << SD_DEBUG << "[Pessl " << _station << "] measurement: " << "Last archive dates back from "
			  << _lastArchive << "; last available is " << lastAvailable << "\n" << "(approximately "
			  << date::floor<date::days>(lastAvailable - date) << " days)" << std::endl;
	while (date < lastAvailable) {
		auto datePlus24Hours = date + chrono::hours{24};
		std::ostringstream routeBuilder;
		routeBuilder << "/data/" << _fieldclimateId << "/raw/from/" << chrono::system_clock::to_time_t(date) << "/to/"
					 << chrono::system_clock::to_time_t(datePlus24Hours);
		std::string route = routeBuilder.str();
		std::string authorization;
		std::string headerDate;
		std::tie(authorization, headerDate) = computeAuthorizationAndDateFields("GET", route);

		client.setHeader("Authorization", authorization);
		client.setHeader("Date", headerDate);
		client.setHeader("Accept", "application/json");

		std::cout << SD_DEBUG << "[Pessl " << _station << "] protocol: " << "GET " << "/v2" << route << " HTTP/1.1 "
				  << "Host: " << APIHOST << " " << "Date: " << headerDate << " " << "Authorization: " << authorization
				  << " " << "Accept: application/json ";


		CURLcode ret = client.download(BASE_URL + route, [&](const std::string& body) {
			std::istringstream responseStream(body);

			bool insertionOk = true;

			FieldClimateApiArchiveMessageCollection collection{&_timeOffseter, &_sensors};
			collection.parse(responseStream);

			if (collection.begin() != collection.end()) {
				// Not having data can happen if the station malfunctioned
				auto newestTimestamp = collection.getNewestMessageTime();
				auto oldestTimestamp = collection.getOldestMessageTime();
				auto archiveDay = date::floor<date::days>(_lastArchive);
				auto lastDay = date::floor<date::days>(newestTimestamp);
				while (archiveDay <= lastDay) {
					int ret = _db.deleteDataPoints(_station, archiveDay, _lastArchive, newestTimestamp);

					if (!ret)
						std::cerr << SD_ERR << "[Pessl " << _station << "] management: "
								  << "couldn't delete replaced observations" << std::endl;
					archiveDay += date::days(1);
				}
				std::vector<Observation> allObs;
				for (const FieldClimateApiArchiveMessage& m : collection) {
					auto o = m.getObservation(_station);
					allObs.push_back(o);
					int ret = _db.insertV2DataPoint(o); // Cannot insert V1
					if (!ret) {
						std::cerr << SD_ERR << "[Pessl " << _station << "] measurement: "
								  << "Failed to insert archive observation for station " << _stationName << std::endl;
						insertionOk = false;
					}
				}
				if (insertionOk) {
					std::cout << SD_DEBUG << "[Pessl " << _station << "] measurement: "
							  << "Archive data stored for Pessl station" << _stationName << std::endl;
					time_t lastArchiveDownloadTime = chrono::system_clock::to_time_t(newestTimestamp);
					insertionOk = _db.updateLastArchiveDownloadTime(_station, lastArchiveDownloadTime);
					if (!insertionOk) {
						std::cerr << SD_ERR << "[Pessl " << _station << "] management: "
							  << "couldn't update last archive download time for station " << _stationName
							  << std::endl;
					} else {
						_lastArchive = newestTimestamp;
					}

					if (_jobPublisher)
						_jobPublisher->publishJobsForPastDataInsertion(_station, oldestTimestamp, newestTimestamp);
				}
				bool ret = _db.insertV2DataPointsInTimescaleDB(allObs.begin(), allObs.end());
				if (!ret) {
					std::cerr << SD_ERR << "[Pessl " << _station << "] measurement: "
						  << "Failed to insert data in TimescaleDB for station " << _stationName
						  << std::endl;
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
	std::cout << SD_INFO << "[Pessl " << _station << "] measurement: "
			  << "Downloading real-time data for Pessl station " << _stationName << std::endl;

	std::ostringstream routeBuilder;
	routeBuilder << "/data/" << _fieldclimateId << "/raw/last/1";
	std::string route = routeBuilder.str();
	std::string authorization;
	std::string headerDate;
	std::tie(authorization, headerDate) = computeAuthorizationAndDateFields("GET", route);

	client.setHeader("Authorization", authorization);
	client.setHeader("Date", headerDate);
	client.setHeader("Accept", "application/json");

	std::cout << SD_DEBUG << "[Pessl " << _station << "] protocol: " << "GET " << "/v2" << route << " HTTP/1.1 "
			  << "Host: " << APIHOST << " " << "Date: " << headerDate << " " << "Authorization: " << authorization
			  << " " << "Accept: application/json ";


	CURLcode ret = client.download(BASE_URL + route, [&, this](const std::string& body) {
		std::istringstream responseStream{body};

		FieldClimateApiArchiveMessageCollection collection{&_timeOffseter, &_sensors};
		collection.parse(responseStream);

		if (collection.begin() == collection.end()) {
			// No data
			return;
		}

		bool insertionOk = true;

		// we expect exactly one message in the collection
		const FieldClimateApiArchiveMessage& m = *(collection.begin());
		auto o = m.getObservation(_station);
		int ret = _db.insertV2DataPoint(o) && _db.insertV2DataPointInTimescaleDB(o);
		if (!ret) {
			std::cerr << SD_ERR << "[Pessl " << _station << "] measurement: "
				  << "failed to insert realtime observation for station " << _stationName << std::endl;
			insertionOk = false;
		}

		if (insertionOk) {
			std::cout << SD_DEBUG << "[Pessl " << _station << "] measurement: " << "realtime data stored for station "
					  << _stationName << std::endl;
		}
	});

	if (ret != CURLE_OK) {
		logAndThrowCurlError(client);
	}
}

std::tuple<std::string, std::string>
FieldClimateApiDownloader::computeAuthorizationAndDateFields(const std::string& method, const std::string& route)
{
	std::string date = date::format("%a, %d %b %Y %T GMT", chrono::system_clock::now());
	std::string sig = computeHMACWithSHA256(method + route + date + _apiKey, _apiSecret);
	return {"hmac " + _apiKey + ":" + std::move(sig), std::move(date)};
}

void FieldClimateApiDownloader::logAndThrowCurlError(CurlWrapper& client)
{
	std::string_view error = client.getLastError();
	std::ostringstream errorStream;
	errorStream << "station " << _stationName << " Bad response from " << APIHOST << ": " << error;
	std::string errorMsg = errorStream.str();
	std::cerr << SD_ERR << "[Pessl " << _station << "] protocol: " << errorMsg << std::endl;
	throw std::runtime_error(errorMsg);
}

}
