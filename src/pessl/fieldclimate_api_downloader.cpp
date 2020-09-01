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

#include <boost/system/error_code.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <dbconnection_observations.h>
#include <syslog.h>

#include "fieldclimate_api_download_scheduler.h"
#include "fieldclimate_api_downloader.h"
#include "fieldclimate_archive_message_collection.h"
#include "fieldclimate_archive_message.h"
#include "../time_offseter.h"
#include "../blocking_tcp_client.h"
#include "../http_utils.h"

namespace meteodata {

namespace ip = boost::asio::ip;
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

void FieldClimateApiDownloader::download(BlockingTcpClient<asio::ssl::stream<ip::tcp::socket>>& client)
{
	std::cerr << "Downloading historical data for station " << _stationName << std::endl;

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

		requestStream << "GET " << "/v2" << route << " HTTP/1.0\r\n";
		std::cerr << "GET " << "/v2" << route << " HTTP/1.0\r\n";
		requestStream << "Host: " << FieldClimateApiDownloadScheduler::APIHOST << "\r\n"
			<< "Connection: keep-alive\r\n"
			<< "Date: " << headerDate << "\r\n"
			<< "Authorization: " << authorization << "\r\n"
			<< "Accept: application/json\r\n\r\n";
		std::cerr << "Host: " << FieldClimateApiDownloadScheduler::APIHOST << "\r\n"
			<< "Connection: keep-alive\r\n"
			<< "Date: " << headerDate << "\r\n"
			<< "Authorization: " << authorization << "\r\n"
			<< "Accept: application/json\r\n\r\n";


		// Send the request.
		std::size_t bytesWritten;
		client.write(request, bytesWritten);

		// Prepare the buffer and the stream on the buffer to receive the response.
		asio::streambuf response{FieldClimateApiDownloader::MAXSIZE};
		std::istream responseStream{&response};

		bool stillOpen = true;
		try {
			stillOpen = getReponseFromHTTP10QueryFromClient(client, response, responseStream, FieldClimateApiDownloader::MAXSIZE, "application/json");
			std::cerr << "Read all the content" << std::endl;

			bool insertionOk = true;

			FieldClimateApiArchiveMessageCollection collection{&_timeOffseter, &_sensors};
			collection.parse(responseStream);

			if (collection.begin() == collection.end()) {
				// No data that day, can happen if the station
				// malfunctioned
				continue;
			}

			auto newestTimestamp = collection.getNewestMessageTime();
			auto archiveDay = date::floor<date::days>(_lastArchive);
			auto lastDay = date::floor<date::days>(end);
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

		} catch (const sys::system_error& error) {
			if (error.code() == asio::error::eof) {
				syslog(LOG_ERR, "station %s: Socket looks closed for %s: %s", _stationName.c_str(), FieldClimateApiDownloadScheduler::APIHOST, error.what());
				std::cerr << "station " << _stationName << " Socket looks closed " << FieldClimateApiDownloadScheduler::APIHOST << ": " << error.what() << std::endl;
				throw sys::system_error{asio::error::in_progress}; // The socket is closed but we made some progress so we use a different code to indicate that we're not stuck in an endless failure cycle
			} else {
				syslog(LOG_ERR, "station %s: Bad response for %s: %s", _stationName.c_str(), FieldClimateApiDownloadScheduler::APIHOST, error.what());
				std::cerr << "station " << _stationName << " Bad response from " << FieldClimateApiDownloadScheduler::APIHOST << ": " << error.what() << std::endl;
				throw error;
			}

		} catch (std::runtime_error& error) {
			syslog(LOG_ERR, "station %s: Bad response for %s: %s", _stationName.c_str(), FieldClimateApiDownloadScheduler::APIHOST, error.what());
			std::cerr << "station " << _stationName << " Bad response from " << FieldClimateApiDownloadScheduler::APIHOST << ": " << error.what() << std::endl;
			throw error;
		}

		date += chrono::hours{24};
		if (date < end && !stillOpen)
			throw sys::system_error{asio::error::in_progress};
	}
}

void FieldClimateApiDownloader::downloadRealTime(BlockingTcpClient<asio::ssl::stream<ip::tcp::socket>>& client)
{
	std::cerr << "Downloading real-time data for station " << _stationName << std::endl;

	// Form the request. We specify the "Connection: keep-alive" header so that the
	// will be usable by other downloaders.
	boost::asio::streambuf request;
	std::ostream requestStream(&request);

	std::ostringstream routeBuilder;
	routeBuilder << "/data/"
		<< _fieldclimateId
		<< "/raw/last/1";
	std::string route = routeBuilder.str();
	std::string authorization;
	std::string headerDate;
	std::tie(authorization, headerDate) = computeAuthorizationAndDateFields("GET", route);

	requestStream << "GET " << "/v2" << route << " HTTP/1.0\r\n";
	std::cerr << "GET " << "/v2" << route << " HTTP/1.0\r\n";
	requestStream << "Host: " << FieldClimateApiDownloadScheduler::APIHOST << "\r\n"
		<< "Connection: keep-alive\r\n"
		<< "Date: " << headerDate << "\r\n"
		<< "Authorization: " << authorization << "\r\n"
		<< "Accept: application/json\r\n\r\n";

	// Send the request.
	std::size_t bytesWritten;
	client.write(request, bytesWritten);

	// Prepare the buffer and the stream on the buffer to receive the response.
	asio::streambuf response{FieldClimateApiDownloader::MAXSIZE};
	std::istream responseStream{&response};

	try {
		getReponseFromHTTP10QueryFromClient(client, response, responseStream, FieldClimateApiDownloader::MAXSIZE, "application/json");
		std::cerr << "Read all the content" << std::endl;

		// Store the content because if we must read it several times
		std::string content;
		std::copy(std::istream_iterator<char>(responseStream), std::istream_iterator<char>(), std::back_inserter(content));

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
	} catch (std::runtime_error& error) {
		syslog(LOG_ERR, "station %s: Bad response for %s: %s", _stationName.c_str(), FieldClimateApiDownloadScheduler::APIHOST, error.what());
		std::cerr << "station " << _stationName << " Bad response from " << FieldClimateApiDownloadScheduler::APIHOST << ": " << error.what() << std::endl;
		throw error;
	}
}

std::tuple<std::string, std::string> FieldClimateApiDownloader::computeAuthorizationAndDateFields(const std::string& method, const std::string& route)
{
	std::string date = date::format("%a, %d %b %Y %H:%M:%S GMT", chrono::system_clock::now());
	std::string sig = computeHMACWithSHA256(method + route + date + _apiKey, _apiSecret);
	return { "hmac " + _apiKey + ":" + std::move(sig), std::move(date) };
}

}
