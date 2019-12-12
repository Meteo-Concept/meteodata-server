/**
 * @file statictxtdownloader.cpp
 * @brief Implementation of the StatICTxtDownloader class
 * @author Laurent Georget
 * @date 2019-02-06
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
#include <date/date.h>
#include <dbconnection_observations.h>

#include "../time_offseter.h"
#include "static_txt_downloader.h"
#include "static_message.h"

// we do not expect the files to be big, so it's simpler and more
// efficient to just slurp them, which means we'd better limit the
// buffer size, for safety's sake
#define BUFFER_MAX_SIZE 4096

namespace asio = boost::asio;
namespace ip = boost::asio::ip;
namespace sys = boost::system; //system() is a function, it cannot be redefined
//as a namespace
namespace chrono = std::chrono;
namespace args = std::placeholders;

namespace meteodata {

using namespace date;

StatICTxtDownloader::StatICTxtDownloader(asio::io_service& ioService, DbConnectionObservations& db, CassUuid station, const std::string& host, const std::string& url, int timezone) :
	_ioService(ioService),
	_db(db),
	_timer(_ioService),
	_station(station),
	_host(host),
	_url(url),
	_lastDownloadTime(chrono::seconds(0)) // any impossible date will do before the first download, if it's old enough, it cannot correspond to any date sent by the station
{
	float latitude;
	float longitude;
	int elevation;
	std::string stationName;
	int pollingPeriod;
	db.getStationCoordinates(station, latitude, longitude, elevation, stationName, pollingPeriod);

	// Timezone is supposed to always be UTC for StatIC files, but it's better not to rely
	// on station owners to never misconfigure their station
	_timeOffseter = TimeOffseter::getTimeOffseterFor(TimeOffseter::PredefinedTimezone(timezone));
	_timeOffseter.setLatitude(latitude);
	_timeOffseter.setLongitude(longitude);
	_timeOffseter.setElevation(elevation);
	_timeOffseter.setMeasureStep(pollingPeriod);
}

void StatICTxtDownloader::start()
{
	waitUntilNextDownload();
}

void StatICTxtDownloader::waitUntilNextDownload()
{
	auto self(shared_from_this());
	auto target = chrono::steady_clock::now();
	auto daypoint = date::floor<date::days>(target);
	auto tod = date::make_time(target - daypoint);
	_timer.expires_from_now(chrono::minutes(10 - tod.minutes().count() % 10 + 2)- chrono::seconds(tod.seconds().count()));
	_timer.async_wait(std::bind(&StatICTxtDownloader::checkDeadline, self, args::_1));
}

void StatICTxtDownloader::checkDeadline(const sys::error_code& e)
{
	/* if the timer has been cancelled, then bail out ; we probably have been
	 * asked to die */
	std::cerr << "Deadline handler hit: " << e.value() << ": " << e.message() << std::endl;
	if (e == sys::errc::operation_canceled)
		return;

	// verify that the timeout is not spurious
	if (_timer.expires_at() <= chrono::steady_clock::now()) {
		std::cerr << "Timed out!" << std::endl;
		try {
			download();
		} catch (std::exception& e) {
			std::cerr << "StatIC file: Couldn't download from "  << _host << ": " << e.what() << std::endl;
			syslog(LOG_ERR, "StatIC file: Couldn't download from %s: %s", _host.data(), e.what());
		}
		// Going back to sleep
		waitUntilNextDownload();
	} else {
		/* spurious handler call, restart the timer without changing the
		 * deadline */
		auto self(shared_from_this());
		_timer.async_wait(std::bind(&StatICTxtDownloader::checkDeadline, self, args::_1));
	}
}

void StatICTxtDownloader::download()
{
	std::cerr << "Now downloading a StatIC file " << std::endl;

	// Make a SSL context
	asio::ssl::context ctx(asio::ssl::context::sslv23);
	ctx.set_default_verify_paths();

	// Get a list of endpoints corresponding to the server name.
	ip::tcp::resolver resolver(_ioService);
	ip::tcp::resolver::query query(_host, "https");
	ip::tcp::resolver::iterator endpointIterator = resolver.resolve(query);

	// Try each endpoint until we successfully establish a connection.
	asio::ssl::stream<ip::tcp::socket> socket(_ioService, ctx);
        // Set SNI Hostname (many hosts need this to handshake successfully)
        if(!SSL_set_tlsext_host_name(socket.native_handle(), _host.c_str()))
        {
            sys::error_code ec{static_cast<int>(::ERR_get_error()), boost::asio::error::get_ssl_category()};
            throw boost::system::system_error{ec};
        }
	boost::asio::connect(socket.lowest_layer(), endpointIterator);

	// Verify the remote
	socket.set_verify_mode(asio::ssl::verify_peer);
	socket.set_verify_callback(asio::ssl::rfc2818_verification(_host));
	socket.handshake(decltype(socket)::client);

	// Form the request. We specify the "Connection: close" header so that the
	// server will close the socket after transmitting the response. This will
	// allow us to treat all data up until the EOF as the content.
	boost::asio::streambuf request;
	std::ostream requestStream(&request);
	std::cerr << "GET " << _url << " HTTP/1.0\r\n";
	requestStream << "GET " << _url << " HTTP/1.0\r\n";
	requestStream << "Host: " << _host << "\r\n";
	requestStream << "Accept: */*\r\n";
	requestStream << "Connection: close\r\n\r\n";

	// Send the request.
	asio::write(socket, request);

	// Read the response status line. The response streambuf will automatically
	// grow to accommodate the entire line. The growth may be limited by passing
	// a maximum size to the streambuf constructor.
	asio::streambuf response(BUFFER_MAX_SIZE);
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
		std::cerr << "StatIC file: Bad response from "  << _host << std::endl;
		syslog(LOG_ERR, "StatIC file: Bad response from %s", _host.data());
		return;
	}
	if (statusCode != 200)
	{
		std::cerr << "StatIC file: Bad response from: " << statusCode << std::endl;
		syslog(LOG_ERR, "StatIC file: Bad response from: %i", statusCode);
		return;
	}

	// Read the response headers, which are terminated by a blank line.
	auto size = asio::read_until(socket, response, "\r\n\r\n");

	// Discard the headers
	// XXX Do we need them?
	response.consume(size);
	// Read the body
	sys::error_code ec;


	// slurp the file (in the answer body)
	size = asio::read(socket, response, ec);
	std::istream fileStream(&response);
	if (ec == asio::error::eof) {
		StatICMessage m{fileStream, _timeOffseter};
		if (!m)
			return;

		if (m.getDateTime() == _lastDownloadTime) {
			// We are still reading the last file, discard it in order
			// not to pollute the cumulative rainfall value
			return;
		} else {
			// The rain is given over the last hour but the file may be
			// fetched more frequently so it's necessary to compute the
			// difference with the rainfall over an hour ago
			auto downloadTime = m.getDateTime();
			auto end = chrono::system_clock::to_time_t(downloadTime);
			auto begin = chrono::system_clock::to_time_t(downloadTime - chrono::hours(1));
			float f;
			if (_db.getRainfall(_station, begin, end, f))
				m.computeRainfall(f);
		}

		char uuidStr[CASS_UUID_STRING_LENGTH];
		cass_uuid_string(_station, uuidStr);
		std::cerr << "UUID identified: " << uuidStr << std::endl;
		bool ret = _db.insertV2DataPoint(_station, m);
		if (ret)
			std::cerr << "Inserted into database" << std::endl;
		else
			std::cerr << "Insertion into database failed" << std::endl;
	}
}

}
