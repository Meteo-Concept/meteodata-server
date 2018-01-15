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

#include <boost/system/error_code.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/basic_waitable_timer.hpp>

#include <syslog.h>
#include <unistd.h>

#include <cassandra.h>

#include "weatherlinkdownloader.h"
#include "vantagepro2message.h"
#include "vantagepro2archivepage.h"
#include "dbconnection.h"
#include "timeoffseter.h"

namespace asio = boost::asio;
namespace ip = boost::asio::ip;
namespace sys = boost::system; //system() is a function, it cannot be redefined
//as a namespace
namespace chrono = std::chrono;

namespace meteodata {

using namespace std::placeholders;
using namespace date;

constexpr char WeatherlinkDownloader::HOST[];

WeatherlinkDownloader::WeatherlinkDownloader(const CassUuid& station, const std::string& auth,
	asio::io_service& ioService, DbConnection& db, TimeOffseter::PredefinedTimezone tz) :
	_db(db),
	_ioService(ioService),
	_authentication(auth),
	_timer(_ioService),
	_station(station)
{
	time_t lastArchiveDownloadTime;
	db.getStationDetails(station, _stationName, _pollingPeriod, lastArchiveDownloadTime);
	_lastArchive = date::sys_seconds(chrono::seconds(lastArchiveDownloadTime));
	_timeOffseter = TimeOffseter::getTimeOffseterFor(tz);
	std::cerr << "Discovered Weatherlink station " << _stationName << std::endl;
}

void WeatherlinkDownloader::start()
{
	download();
	waitUntilNextDownload();
}

void WeatherlinkDownloader::waitUntilNextDownload()
{
	auto self(shared_from_this());
	_timer.expires_from_now(chrono::minutes(_pollingPeriod));
	_timer.async_wait(std::bind(&WeatherlinkDownloader::checkDeadline, self, _1));
}

void WeatherlinkDownloader::checkDeadline(const sys::error_code& e)
{
	/* if the timer has been cancelled, then bail out ; we probably have been
	 * asked to die */
	std::cerr << "Deadline handler hit: " << e.value() << ": " << e.message() << std::endl;
	if (e == sys::errc::operation_canceled)
		return;

	// verify that the timeout is not spurious
	if (_timer.expires_at() <= chrono::steady_clock::now()) {
		std::cerr << "Timed out!" << std::endl;
		download();
		// Going back to sleep
		waitUntilNextDownload();
	} else {
		/* spurious handler call, restart the timer without changing the
		 * deadline */
		auto self(shared_from_this());
		_timer.async_wait(std::bind(&WeatherlinkDownloader::checkDeadline, self, _1));
	}
}

bool WeatherlinkDownloader::compareAsciiCaseInsensitive(const std::string& str1, const std::string& str2)
{
	if (str1.size() != str2.size()) {
		return false;
	}
	for (std::string::const_iterator c1 = str1.begin(), c2 = str2.begin(); c1 != str1.end(); ++c1, ++c2) {
		if (::tolower(*c1) != ::tolower(*c2)) {
			return false;
		}
	}
	return true;
}

void WeatherlinkDownloader::download()
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

	// Get a list of endpoints corresponding to the server name.
	ip::tcp::resolver resolver(_ioService);
	ip::tcp::resolver::query query(HOST, "http");
	ip::tcp::resolver::iterator endpointIterator = resolver.resolve(query);

	// Try each endpoint until we successfully establish a connection.
	ip::tcp::socket socket(_ioService);
	boost::asio::connect(socket, endpointIterator);

	// Form the request. We specify the "Connection: close" header so that the
	// server will close the socket after transmitting the response. This will
	// allow us to treat all data up until the EOF as the content.
	boost::asio::streambuf request;
	std::ostream requestStream(&request);
	requestStream << "GET " << "/webdl.php?timestamp=" << timestamp << "&" << _authentication << "&action=data" << " HTTP/1.0\r\n";
	std::cerr << "GET " << "/webdl.php?timestamp=" << timestamp << "&" << _authentication << "&action=data" << " HTTP/1.0\r\n";
	requestStream << "Host: " << HOST << "\r\n";
	requestStream << "Accept: */*\r\n";
	requestStream << "Connection: close\r\n\r\n";

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
		syslog(LOG_ERR, "station %s: Bad response from %s", _stationName.c_str(), HOST);
		std::cerr << "station " << _stationName << " Bad response from " << HOST << std::endl;
		return;
	}
	if (statusCode != 200)
	{
		syslog(LOG_ERR, "station %s: Error code from %s: %i", _stationName.c_str(), HOST, statusCode);
		std::cerr << "station " << _stationName << " Error from " << HOST << ": " << statusCode << std::endl;
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
				syslog(LOG_ERR, "station %s: Output from %s has an invalid size", _stationName.c_str(), HOST);
				std::cerr << "station " << _stationName << ": Output has an invalid size " << pagesLeft << std::endl;
				return;
			}
			pagesLeft = pagesLeft / 52;
		}
	}

	if (pagesLeft < 0) {
		syslog(LOG_ERR, "station %s: No Content-Length: in %s output?!", _stationName.c_str(), HOST);
		std::cerr << "station " << _stationName << ": No Content-Length: in  " << HOST << " output?!" << std::endl;
		return;
	} else if (pagesLeft == 0) {
		std::cerr << "No pages to download, sleeping" << std::endl;
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
				syslog(LOG_ERR, "station %s: Error when validating output from %s: %s", _stationName.c_str(), HOST, error.message().c_str());
				std::cerr << "station " << _stationName << " Error when receiving from " << HOST << ": " << error.message() << std::endl;
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
	for (auto it = pages.cbegin() ; it != pages.cend() && ret ; ++it) {
		std::cerr << "Analyzing page " << it->getTimestamp() << std::endl;
		if (it->looksValid()) {
			ret = _db.insertDataPoint(_station, *it);
			_lastArchive = it->getTimestamp();
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
