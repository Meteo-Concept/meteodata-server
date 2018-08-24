/**
 * @file synopdownloader.cpp
 * @brief Implementation of the SynopDownloader class
 * @author Laurent Georget
 * @date 2018-08-20
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
#include <map>
#include <sstream>
#include <iomanip>

#include <cstring>
#include <cctype>

#include <boost/system/error_code.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/basic_waitable_timer.hpp>

#include <syslog.h>
#include <unistd.h>

#include "synopdownloader.h"
#include "dbconnection.h"
#include "ogimetsynop.h"
#include "synopdecoder/parser.h"

namespace asio = boost::asio;
namespace ip = boost::asio::ip;
namespace sys = boost::system; //system() is a function, it cannot be redefined
//as a namespace
namespace chrono = std::chrono;
namespace args = std::placeholders;

namespace meteodata {

using namespace date;

constexpr char SynopDownloader::HOST[];
constexpr char SynopDownloader::GROUP_FR[];

SynopDownloader::SynopDownloader(asio::io_service& ioService, DbConnection& db) :
	_db(db),
	_ioService(ioService),
	_timer(_ioService)
{
}

void SynopDownloader::start()
{
	std::vector<std::tuple<CassUuid, std::string>> icaos;
	_db.getAllIcaos(icaos);
	for (auto&& icao : icaos)
		_icaos.emplace(std::get<1>(icao), std::get<0>(icao));
	download();
	waitUntilNextDownload();
}

void SynopDownloader::waitUntilNextDownload()
{
	auto self(shared_from_this());
	_timer.expires_from_now(chrono::hours(1));
	_timer.async_wait(std::bind(&SynopDownloader::checkDeadline, self, args::_1));
}

void SynopDownloader::checkDeadline(const sys::error_code& e)
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
		_timer.async_wait(std::bind(&SynopDownloader::checkDeadline, self, args::_1));
	}
}

void SynopDownloader::download()
{
	std::cerr << "Now downloading SYNOP messages " << std::endl;
	auto time = chrono::system_clock::now() - chrono::hours(1);
	auto daypoint = date::floor<date::days>(time);
	auto ymd = date::year_month_day(daypoint);   // calendar date
	auto tod = date::make_time(time - daypoint); // Yields time_of_day type

	// Obtain individual components as integers
	auto y   = int(ymd.year());
	auto m   = unsigned(ymd.month());
	auto d   = unsigned(ymd.day());
	auto h   = tod.hours().count();
	auto min = 30;


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
	std::cerr << "GET " << "/cgi-bin/getsynop?begin=" << std::setfill('0')
		<< std::setw(4) << y
		<< std::setw(2) << m
		<< std::setw(2) << d
		<< std::setw(2) << h
		<< std::setw(2) << min
		<< "&block=" << GROUP_FR << " HTTP/1.0\r\n";
	requestStream << "GET " << "/cgi-bin/getsynop?begin=" << std::setfill('0')
		<< std::setw(4) << y
		<< std::setw(2) << m
		<< std::setw(2) << d
		<< std::setw(2) << h
		<< std::setw(2) << min
		<< "&block=" << GROUP_FR << " HTTP/1.0\r\n";
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
		syslog(LOG_ERR, "SYNOP: Bad response from ogimet");
		return;
	}
	if (statusCode != 200)
	{
		syslog(LOG_ERR, "SYNOP: Error code from ogimet: %i", statusCode);
		return;
	}

	// Read the response headers, which are terminated by a blank line.
	auto size = asio::read_until(socket, response, "\r\n\r\n");

	// Discard the headers
	// XXX Should we do something about them?
	response.consume(size);

	// Read the body
	sys::error_code ec;
	do {
		size = asio::read_until(socket, response, "\n", ec);
		std::string line;
		std::getline(responseStream, line);

		// Deal with the annoying case as early as possible
		if (line.find("NIL") != std::string::npos)
			continue;

		std::istringstream lineIterator{line};

		Parser parser;
		if (parser.parse(lineIterator)) {
			const SynopMessage& m = parser.getDecodedMessage();
			auto uuidIt = _icaos.find(m._stationIcao);
			if (uuidIt != _icaos.end()) {
				char uuidStr[CASS_UUID_STRING_LENGTH];
				cass_uuid_string(uuidIt->second, uuidStr);
				std::cerr << "UUID identified: " << uuidStr << std::endl;
				OgimetSynop synop{m};
				_db.insertV2DataPoint(uuidIt->second, synop);
				std::cerr << "Inserted into database" << std::endl;
			}
		} else {
			std::cerr << "Record looks invalid, discarding..." << std::endl;
		}
	} while (!ec);
}

}
