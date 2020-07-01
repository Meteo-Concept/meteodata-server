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
#include "../http_utils.h"
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

StatICTxtDownloader::StatICTxtDownloader(asio::io_service& ioService, DbConnectionObservations& db, CassUuid station, const std::string& host, const std::string& url, bool https, int timezone) :
	_ioService(ioService),
	_db(db),
	_timer(_ioService),
	_station(station),
	_host(host),
	_url(url),
	_https(https),
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
	try {
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

		asio::streambuf response{StatICMessage::MAXSIZE};
		std::istream responseStream{&response};

		if (_https) {
			sendRequestHttps(request, response, responseStream);
		} else {
			sendRequestHttp(request, response, responseStream);
		}

		StatICMessage m{responseStream, _timeOffseter};
		if (!m) {
			std::cerr << "Impossible to parse the message" << std::endl;
			syslog(LOG_ERR, "StatIC file: Cannot parse response from: %s", _host.data());
			return;
		}

		if (m.getDateTime() == _lastDownloadTime) {
			// We are still reading the last file, discard it in order
			// not to pollute the cumulative rainfall value
			std::cerr << "Previous message has the same date: " << m.getDateTime() << "!" << std::endl;
			return;
		} else {
			// The rain is given over the last hour but the file may be
			// fetched more frequently so it's necessary to compute the
			// difference with the rainfall over an hour ago
			auto downloadTime = m.getDateTime();
			auto end = chrono::system_clock::to_time_t(downloadTime);
			auto begin1h = chrono::system_clock::to_time_t(downloadTime - chrono::hours(1));
			auto beginDay = chrono::system_clock::to_time_t(date::floor<date::days>(downloadTime));
			float f1h, fDay;
			if (_db.getRainfall(_station, begin1h, end, f1h) && _db.getRainfall(_station, beginDay, end, fDay))
				m.computeRainfall(f1h, fDay);
		}

		char uuidStr[CASS_UUID_STRING_LENGTH];
		cass_uuid_string(_station, uuidStr);
		std::cerr << "UUID identified: " << uuidStr << std::endl;
		bool ret = _db.insertV2DataPoint(_station, m);
		if (ret)
			std::cerr << "Inserted into database" << std::endl;
		else
			std::cerr << "Insertion into database failed" << std::endl;
	} catch (const sys::system_error& error) {
		if (error.code() == asio::error::eof) {
			syslog(LOG_ERR, "station %s: Socket looks closed for %s: %s", _url.c_str(), _host.c_str(), error.what());
			std::cerr << "station " << _url << " Socket looks closed " << _host << ": " << error.what() << std::endl;
			return;
		} else {
			syslog(LOG_ERR, "station %s: Bad response for %s: %s", _url.c_str(), _host.c_str(), error.what());
			std::cerr << "station " << _url << " Bad response from " << _host << ": " << error.what() << std::endl;
			return;
		}

	} catch (std::runtime_error& error) {
		syslog(LOG_ERR, "station %s: Bad response for %s: %s", _url.c_str(), _host.c_str(), error.what());
		std::cerr << "station " << _url << " Bad response from " << _host << ": " << error.what() << std::endl;
		return;
	}
}

void StatICTxtDownloader::sendRequestHttps(asio::streambuf& request, asio::streambuf& response, std::istream& responseStream)
{
	std::cerr << "Now downloading a StatIC file over HTTPS " << std::endl;

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
	    throw sys::system_error(ec);
        }
	boost::asio::connect(socket.lowest_layer(), endpointIterator);

	// Verify the remote
	socket.set_verify_mode(asio::ssl::verify_peer);
	socket.set_verify_callback(asio::ssl::rfc2818_verification(_host));
	socket.handshake(decltype(socket)::client);

	// Send the request.
	asio::write(socket, request);

	// Get the response into the response stream and check the status and headers
	// this places the response stream iterator at the beginning of the body
	getReponseFromHTTP10Query(socket, response, responseStream, StatICMessage::MAXSIZE, "");
}

void StatICTxtDownloader::sendRequestHttp(asio::streambuf& request, asio::streambuf& response, std::istream& responseStream)
{
	std::cerr << "Now downloading a StatIC file over HTTP " << std::endl;

	// Get a list of endpoints corresponding to the server name.
	ip::tcp::resolver resolver(_ioService);
	ip::tcp::resolver::query query(_host, "http");
	ip::tcp::resolver::iterator endpointIterator = resolver.resolve(query);

	// Try each endpoint until we successfully establish a connection.
	ip::tcp::socket socket(_ioService);
	boost::asio::connect(socket, endpointIterator);

	// Send the request.
	asio::write(socket, request);

	// Get the response into the response stream and check the status and headers
	// this places the response stream iterator at the beginning of the body
	getReponseFromHTTP10Query(socket, response, responseStream, StatICMessage::MAXSIZE, "");
}

}
