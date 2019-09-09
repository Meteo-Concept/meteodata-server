/**
 * @file shipandbuoydownloader.cpp
 * @brief Implementation of the ShipAndBuoyDownloader class
 * @author Laurent Georget
 * @date 2019-01-16
 */
/*
 * Copyright (C) 2019  JD Environnement <contact@meteo-concept.fr>
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

#include <fstream>

#include <cstring>
#include <cctype>
#include <syslog.h>
#include <unistd.h>

#include <boost/system/error_code.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/basic_waitable_timer.hpp>
#include <dbconnection_observations.h>

#include "ship_and_buoy_downloader.h"
#include "meteo_france_ship_and_buoy.h"

namespace asio = boost::asio;
namespace ip = boost::asio::ip;
namespace sys = boost::system; //system() is a function, it cannot be redefined
//as a namespace
namespace chrono = std::chrono;
namespace args = std::placeholders;

namespace meteodata {

using namespace date;

constexpr char ShipAndBuoyDownloader::HOST[];
constexpr char ShipAndBuoyDownloader::URL[];

ShipAndBuoyDownloader::ShipAndBuoyDownloader(asio::io_service& ioService, DbConnectionObservations& db) :
	_ioService(ioService),
	_db(db),
	_timer(_ioService)
{
}

void ShipAndBuoyDownloader::start()
{
	std::vector<std::tuple<CassUuid, std::string>> icaos;
	_db.getAllIcaos(icaos);
	for (auto&& icao : icaos)
		_icaos.emplace(std::get<1>(icao), std::get<0>(icao));
	download();
	waitUntilNextDownload();
}

void ShipAndBuoyDownloader::waitUntilNextDownload()
{
	auto self(shared_from_this());
	auto target = chrono::steady_clock::now();
	auto daypoint = date::floor<date::days>(target);
	auto tod = date::make_time(target - daypoint);
	_timer.expires_from_now(date::days(1) + chrono::hours(6 - tod.hours().count()) - chrono::minutes(tod.minutes().count()));
	_timer.async_wait(std::bind(&ShipAndBuoyDownloader::checkDeadline, self, args::_1));
}

void ShipAndBuoyDownloader::checkDeadline(const sys::error_code& e)
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
		_timer.async_wait(std::bind(&ShipAndBuoyDownloader::checkDeadline, self, args::_1));
	}
}

void ShipAndBuoyDownloader::download()
{
	std::cerr << "Now downloading SHIP and BUOY data " << std::endl;
	auto ymd = date::year_month_day(date::floor<date::days>(chrono::system_clock::now() - date::days(1)));

	// Make a SSL context
	asio::ssl::context ctx(asio::ssl::context::sslv23);
	ctx.set_default_verify_paths();

	// Get a list of endpoints corresponding to the server name.
	ip::tcp::resolver resolver(_ioService);
	ip::tcp::resolver::query query(HOST, "https");
	ip::tcp::resolver::iterator endpointIterator = resolver.resolve(query);

	// Try each endpoint until we successfully establish a connection.
	asio::ssl::stream<ip::tcp::socket> socket(_ioService, ctx);
	boost::asio::connect(socket.lowest_layer(), endpointIterator);

	// Verify the remote
	socket.set_verify_mode(asio::ssl::verify_peer);
	socket.set_verify_callback(asio::ssl::rfc2818_verification(HOST));
	socket.handshake(decltype(socket)::client);

	// Form the request. We specify the "Connection: close" header so that the
	// server will close the socket after transmitting the response. This will
	// allow us to treat all data up until the EOF as the content.
	boost::asio::streambuf request;
	std::ostream requestStream(&request);
	std::cerr << "GET " << date::format(URL, ymd) << " HTTP/1.1\r\n";
	requestStream << "GET " << date::format(URL, ymd) << " HTTP/1.1\r\n";
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
		std::cerr << "SHIP and BUOY: Bad response from Météo France" << std::endl;
		syslog(LOG_ERR, "SHIP and BUOY: Bad response from Météo France");
		return;
	}
	if (statusCode != 200)
	{
		std::cerr << "SHIP and BUOY: Bad response from Météo France: " << statusCode << std::endl;
		syslog(LOG_ERR, "SHIP AND BUOY: Bad response from Météo France: %i", statusCode);
		return;
	}

	// Read the response headers, which are terminated by a blank line.
	auto size = asio::read_until(socket, response, "\r\n\r\n");

	// Discard the headers
	// XXX Do we need them?
	response.consume(size);
	// Read the body
	sys::error_code ec;


	// The first line is the CSV header
	size = asio::read_until(socket, response, "\n", ec);
	std::string line;
	std::getline(responseStream, line);
	std::istringstream lineIterator{line};
	std::vector<std::string> fields;
	for (std::string field ; std::getline(lineIterator, field, ';') ;)
		if (field != "")
			fields.emplace_back(std::move(field));

	do {
		size = asio::read_until(socket, response, "\n", ec);
		std::getline(responseStream, line);

		lineIterator = std::istringstream{line};
		MeteoFranceShipAndBuoy m{lineIterator, fields};
		if (!m)
			continue;
		auto uuidIt = _icaos.find(m.getIdentifier());
		if (uuidIt != _icaos.end()) {
			char uuidStr[CASS_UUID_STRING_LENGTH];
			cass_uuid_string(uuidIt->second, uuidStr);
			std::cerr << "UUID identified: " << uuidStr << std::endl;
			bool ret = _db.insertV2DataPoint(uuidIt->second, m);
			if (ret)
				std::cerr << "Inserted into database" << std::endl;
			else
				std::cerr << "Insertion into database failed" << std::endl;
		}
	} while (!ec);
}

}
