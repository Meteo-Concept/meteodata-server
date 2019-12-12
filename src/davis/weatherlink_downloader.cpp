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
#include <syslog.h>
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

namespace asio = boost::asio;
namespace ip = boost::asio::ip;
namespace sys = boost::system; //system() is a function, it cannot be redefined
//as a namespace
namespace chrono = std::chrono;
namespace args = std::placeholders;

namespace meteodata {

using namespace date;

WeatherlinkDownloader::WeatherlinkDownloader(const CassUuid& station, const std::string& auth,
	const std::string& apiToken, asio::io_service& ioService, DbConnectionObservations& db,
	TimeOffseter::PredefinedTimezone tz) :
	AbstractWeatherlinkDownloader(station, ioService, db, tz),
	_authentication{auth},
	_apiToken{apiToken}
{}

void WeatherlinkDownloader::downloadRealTime(asio::ssl::stream<ip::tcp::socket>& socket)
{
	if (_apiToken.empty())
		return; // no token, no realtime obs

	std::cerr << "Downloading real-time data for station " << _stationName << std::endl;

	// Form the request. We specify the "Connection: keep-alive" header so that the
	// will be usable by other downloaders.
	boost::asio::streambuf request;
	std::ostream requestStream(&request);
	requestStream << "GET " << "/v1/NoaaExt.xml?" << _authentication << "&apiToken=" << _apiToken << " HTTP/1.0\r\n";
	std::cerr << "GET " << "/v1/NoaaExt.xml?"  << "user=XXXXXXXXX&pass=XXXXXXXXX&apiToken=XXXXXXXX" << " HTTP/1.0\r\n";
	requestStream << "Host: " << WeatherlinkDownloadScheduler::APIHOST << "\r\n";
	requestStream << "Connection: keep-alive\r\n";
	requestStream << "Accept: application/xml\r\n\r\n";

	// Send the request.
	asio::write(socket, request);

	// Read the response status line. The response streambuf will automatically
	// grow to accommodate the entire line. The growth may be limited by passing
	// a maximum size to the streambuf constructor.
	asio::streambuf response(WeatherlinkApiv1RealtimeMessage::MAXSIZE);
	asio::read_until(socket, response, "\r\n");

	// Check that response is OK.
	std::istream responseStream(&response);
	std::string httpVersion;
	responseStream >> httpVersion;
	unsigned int statusCode;
	responseStream >> statusCode;
	std::string statusMessage;
	std::getline(responseStream, statusMessage);
	std::cerr << "station " << _stationName << " Got an answer from " << WeatherlinkDownloadScheduler::APIHOST << ": " << statusMessage << std::endl;
	if (!responseStream || httpVersion.substr(0, 5) != "HTTP/")
	{
		syslog(LOG_ERR, "station %s: Bad response from %s", _stationName.c_str(), WeatherlinkDownloadScheduler::HOST);
		std::cerr << "station " << _stationName << " Bad response from " << WeatherlinkDownloadScheduler::APIHOST << std::endl;
		return;
	}
	if (statusCode != 200)
	{
		syslog(LOG_ERR, "station %s: Error code from %s: %i", _stationName.c_str(), WeatherlinkDownloadScheduler::APIHOST, statusCode);
		std::cerr << "station " << _stationName << " Error from " << WeatherlinkDownloadScheduler::HOST << ": " << statusCode << std::endl;
		return;
	}

	// Read the response headers, which are terminated by a blank line.
	asio::read_until(socket, response, "\r\n\r\n");

	// Process the response headers.
	std::string header;
	std::istringstream fromheader;
	std::string field;
	std::size_t size = 0;
	std::string connectionStatus;
	std::string type;
	while (std::getline(responseStream, header) && header != "\r") {
		fromheader.str(header);
		fromheader >> field;
		std::cerr << "Header: " << field << std::endl;
		if (compareAsciiCaseInsensitive(field, "content-length:")) {
			fromheader >> size;
			if (size == 0 || size >= WeatherlinkApiv1RealtimeMessage::MAXSIZE) {
				syslog(LOG_ERR, "station %s: Output from %s is either null or too big", _stationName.c_str(), WeatherlinkDownloadScheduler::APIHOST);
				std::cerr << "station " << _stationName << ": Output is either null or too big (" << (size / 1000.) << "ko)" << std::endl;
				return;
			}
		} else if (compareAsciiCaseInsensitive(field, "connection:")) {
			fromheader >> connectionStatus;
		} else if (compareAsciiCaseInsensitive(field, "content-type:")) {
			fromheader >> type;
			if (!compareAsciiCaseInsensitive(type, "application/xml")) { // WAT?!
				syslog(LOG_ERR, "station %s: Output from %s is not XML", _stationName.c_str(), WeatherlinkDownloadScheduler::APIHOST);
				std::cerr << "station " << _stationName << ": Output is not XML" << std::endl;
				return;
			}
		}
	}

	// Read the response body
	sys::error_code ec;
	std::cerr << "We are expecting " << size << " bytes, the buffer contains " << response.size() << " bytes." << std::endl;
	if (size == 0) {
		if (compareAsciiCaseInsensitive(connectionStatus, "close")) {
			// The server has closed the connection, read until EOF
			asio::read(socket, response, asio::transfer_all(), ec);
		} else {
			// Maybe chunk-encoded output? But we asked for HTTP/1.0 so...
			syslog(LOG_ERR, "station %s: no real-time available", _stationName.c_str());
			std::cerr << "station " << _stationName << ": no real-time available" << std::endl;
			return;
		}
	} else if (response.size() < size) {
		asio::read(socket, response, asio::transfer_at_least(size - response.size()), ec);
	}

	if (ec && ec != asio::error::eof) {
		syslog(LOG_ERR, "station %s: download error %s", _stationName.c_str(), ec.message().c_str());
		std::cerr << "station " << _stationName << ": download error " << ec.message() << std::endl;
		return;
	}
	if (response.size() < size) {
		syslog(LOG_ERR, "station %s: download error: less content read than advertised", _stationName.c_str());
		std::cerr << "station " << _stationName << ": download error: less content read than advertised" << ec.message() << std::endl;
		throw sys::system_error(asio::error::eof);
	}
	std::cerr << "Read all the content" << std::endl;
	WeatherlinkApiv1RealtimeMessage obs(&_timeOffseter);
	obs.parse(responseStream);
	int ret = _db.insertV2DataPoint(_station, obs); // Don't bother inserting V1
	if (!ret) {
		syslog(LOG_ERR, "station %s: Failed to insert real-time observation", _stationName.c_str());
		std::cerr << "station " << _stationName << ": Failed to insert real-time observation" << std::endl;
	}
}

void WeatherlinkDownloader::download(ip::tcp::socket& socket)
{
	std::cerr << "Now downloading for station " << _stationName << std::endl;
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
	std::cerr << "Timestamp: " << timestamp << std::endl;

	// Form the request
	boost::asio::streambuf request;
	std::ostream requestStream(&request);

	requestStream << "GET " << "/webdl.php?timestamp=" << timestamp << "&" << _authentication << "&action=data" << " HTTP/1.0\r\n";
	std::cerr << "GET " << "/webdl.php?timestamp=" << timestamp << "&" << _authentication << "&action=data" << " HTTP/1.0\r\n";
	requestStream << "Host: " << WeatherlinkDownloadScheduler::HOST << "\r\n";
	requestStream << "Connection: keep-alive\r\n";
	requestStream << "Accept: */*\r\n\r\n";

	// Send the request.
	asio::write(socket, request);

	// Read the response status line. The response streambuf will automatically
	// grow to accommodate the entire line. The growth may be limited by passing
	// a maximum size to the streambuf constructor.
	asio::streambuf response;
	asio::read_until(socket, response, "\r\n");

	// Check that response is OK.
	std::istream responseStream(&response);
	std::string httpVersion;
	responseStream >> httpVersion;
	unsigned int statusCode;
	responseStream >> statusCode;
	std::string statusMessage;
	std::getline(responseStream, statusMessage);
	if (!responseStream || httpVersion.substr(0, 5) != "HTTP/")
	{
		syslog(LOG_ERR, "station %s: Bad response from %s", _stationName.c_str(), WeatherlinkDownloadScheduler::HOST);
		std::cerr << "station " << _stationName << " Bad response from " << WeatherlinkDownloadScheduler::HOST << std::endl;
		return;
	}
	if (statusCode != 200)
	{
		syslog(LOG_ERR, "station %s: Error code from %s: %i", _stationName.c_str(), WeatherlinkDownloadScheduler::HOST, statusCode);
		std::cerr << "station " << _stationName << " Error from " << WeatherlinkDownloadScheduler::HOST << ": " << statusCode << std::endl;
		return;
	}

	// Read the response headers, which are terminated by a blank line.
	asio::read_until(socket, response, "\r\n\r\n");

	// Process the response headers.
	std::string header;
	std::istringstream fromheader;
	std::string field;
	int pagesLeft = -1;
	while (std::getline(responseStream, header) && header != "\r") {
		fromheader.str(header);
		fromheader >> field;
		if (compareAsciiCaseInsensitive(field, "content-length:")) {
			fromheader >> pagesLeft;
			if (pagesLeft % 52 != 0) {
				syslog(LOG_ERR, "station %s: Output from %s has an invalid size", _stationName.c_str(), WeatherlinkDownloadScheduler::HOST);
				std::cerr << "station " << _stationName << ": Output has an invalid size " << pagesLeft << std::endl;
				return;
			}
			pagesLeft = pagesLeft / 52;
		}
	}

	if (pagesLeft < 0) {
		syslog(LOG_ERR, "station %s: No Content-Length: in %s output?!", _stationName.c_str(), WeatherlinkDownloadScheduler::HOST);
		std::cerr << "station " << _stationName << ": No Content-Length: in  " << WeatherlinkDownloadScheduler::HOST << " output?!" << std::endl;
		return;
	} else if (pagesLeft == 0) {
		std::cerr << "No pages to download" << std::endl;
		return;
	} else {
		std::cerr << "We will receive " << pagesLeft << " pages" << std::endl;
	}

	std::vector<VantagePro2ArchiveMessage> pages;
	while (pagesLeft) {
		VantagePro2ArchiveMessage::ArchiveDataPoint _datapoint;
		sys::error_code error;
		if (response.size() < sizeof(_datapoint)) {
			asio::read(socket, response, asio::transfer_at_least(sizeof(_datapoint)), error);
			if (error && error != asio::error::eof) {
				syslog(LOG_ERR, "station %s: Error when validating output from %s: %s", _stationName.c_str(), WeatherlinkDownloadScheduler::HOST, error.message().c_str());
				std::cerr << "station " << _stationName << " Error when receiving from " << WeatherlinkDownloadScheduler::HOST << ": " << error.message() << std::endl;
				return;
			}
		}
		std::size_t length = asio::buffer_copy(asio::buffer(&_datapoint, sizeof(_datapoint)), response.data());
		if (length != sizeof(_datapoint)) {
			syslog(LOG_ERR, "station %s: Buffer length error: %lu", _stationName.c_str(), length);
			std::cerr << "station " << _stationName << " Buffer length error: " << length << std::endl;
			return;
		} else {
			response.consume(sizeof(_datapoint));
			std::cerr << pagesLeft << " pages left" << std::endl;
			pagesLeft--;
			pages.emplace_back(_datapoint, &_timeOffseter);
		}
	}

	bool ret = true;
	auto start = _lastArchive;
	for (auto it = pages.cbegin() ; it != pages.cend() && ret ; ++it) {
		std::cerr << "Analyzing page " << it->getTimestamp() << std::endl;
		if (it->looksValid()) {
			_lastArchive = it->getTimestamp();
			auto end = _lastArchive;
			auto day = date::floor<date::days>(start);
			auto lastDay = date::floor<date::days>(end);
			while (day <= lastDay) {
				ret = _db.deleteDataPoints(_station, day, start, end);

				if (!ret)
					syslog(LOG_ERR, "station %s: Couldn't delete temporary realtime observations", _stationName.c_str());
				day += date::days(1);
			}

			start = end;
			ret = _db.insertV2DataPoint(_station, *it);
		} else {
			std::cerr << "Record looks invalid, discarding..." << std::endl;
		}
		//Otherwise, just discard
	}

	if (ret) {
		std::cerr << "Archive data stored\n" << std::endl;
		time_t lastArchiveDownloadTime = _lastArchive.time_since_epoch().count();
		ret = _db.updateLastArchiveDownloadTime(_station, lastArchiveDownloadTime);
		if (!ret)
			syslog(LOG_ERR, "station %s: Couldn't update last archive download time", _stationName.c_str());
	} else {
		std::cerr << "Failed to store archive! Aborting" << std::endl;
		syslog(LOG_ERR, "station %s: Couldn't store archive", _stationName.c_str());
		return;
	}
}

}
