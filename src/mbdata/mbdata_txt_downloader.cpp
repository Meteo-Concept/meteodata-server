/**
 * @file mbdatatxtdownloader.cpp
 * @brief Implementation of the MBDataTxtDownloader class
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
#include <algorithm>

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

#include "mbdata_txt_downloader.h"
#include "mbdata_messages/mbdata_message_factory.h"
#include "../time_offseter.h"
#include "../http_utils.h"
#include "../blocking_tcp_client.h"

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

MBDataTxtDownloader::MBDataTxtDownloader(asio::io_service& ioService, DbConnectionObservations& db, const std::tuple<CassUuid, std::string, std::string, bool, int, std::string>& downloadDetails) :
	_ioService(ioService),
	_db(db),
	_timer(_ioService),
	_station(std::get<0>(downloadDetails)),
	_host(std::get<1>(downloadDetails)),
	_url(std::get<2>(downloadDetails)),
	_https(std::get<3>(downloadDetails)),
	_type(std::get<5>(downloadDetails)),
	_lastDownloadTime(chrono::seconds(0)) // any impossible date will do before the first download, if it's old enough, it cannot correspond to any date sent by the station
{
	float latitude;
	float longitude;
	int elevation;
	int pollingPeriod;
	db.getStationCoordinates(_station, latitude, longitude, elevation, _stationName, pollingPeriod);

	_timeOffseter = TimeOffseter::getTimeOffseterFor(TimeOffseter::PredefinedTimezone(std::get<4>(downloadDetails)));
	_timeOffseter.setLatitude(latitude);
	_timeOffseter.setLongitude(longitude);
	_timeOffseter.setElevation(elevation);
	_timeOffseter.setMeasureStep(pollingPeriod);
}

void MBDataTxtDownloader::start()
{
	waitUntilNextDownload();
}

void MBDataTxtDownloader::waitUntilNextDownload()
{
	auto self(shared_from_this());
	auto target = chrono::steady_clock::now();
	auto daypoint = date::floor<date::days>(target);
	auto tod = date::make_time(target - daypoint);
	_timer.expires_from_now(chrono::minutes(10 - tod.minutes().count() % 10 + 2)- chrono::seconds(tod.seconds().count()));
	_timer.async_wait(std::bind(&MBDataTxtDownloader::checkDeadline, self, args::_1));
}

void MBDataTxtDownloader::checkDeadline(const sys::error_code& e)
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
			std::cerr << "MBData file: Couldn't download from "  << _host << ": " << e.what() << std::endl;
			syslog(LOG_ERR, "MBData file: Couldn't download from %s: %s", _host.data(), e.what());
		}
		// Going back to sleep
		waitUntilNextDownload();
	} else {
		/* spurious handler call, restart the timer without changing the
		 * deadline */
		auto self(shared_from_this());
		_timer.async_wait(std::bind(&MBDataTxtDownloader::checkDeadline, self, args::_1));
	}
}

void MBDataTxtDownloader::downloadHttps(boost::asio::streambuf& request, boost::asio::streambuf& response, std::istream& responseStream)
{
	// Make a SSL context
	asio::ssl::context ctx(asio::ssl::context::sslv23);
	ctx.set_default_verify_paths();

	// Create a blocking TCP client to handle the download
	// Set a high enough timeout because servers can be a bit
	// unresponsive sometimes.
	BlockingTcpClient<asio::ssl::stream<ip::tcp::socket>> client(chrono::seconds(5), std::move(ctx));

	// We need the socket itself to do the TLS handshake successfully
	auto& socket = client.socket();
	if(!SSL_set_tlsext_host_name(socket.native_handle(), _host.c_str()))
	{
		sys::error_code ec{static_cast<int>(::ERR_get_error()), asio::error::get_ssl_category()};
		throw sys::system_error{ec};
	}

	// Connect the client
	client.connect(_host, "https");

	// Verify the remote
	socket.set_verify_mode(asio::ssl::verify_peer);
	socket.set_verify_callback(asio::ssl::rfc2818_verification(_host));
	socket.handshake(asio::ssl::stream<ip::tcp::socket>::client);

	// Send the request.
	std::size_t bytesWritten;
	client.write(request, bytesWritten);

	// Read the response and its headers
	getReponseFromHTTP10QueryFromClient(client, response, responseStream, BUFFER_MAX_SIZE, "");
}

void MBDataTxtDownloader::downloadHttp(boost::asio::streambuf& request, boost::asio::streambuf& response, std::istream& responseStream)
{
	// Create a blocking TCP client and connect it
	BlockingTcpClient<ip::tcp::socket> client(chrono::seconds(5));
	client.connect(_host, "http");

	// Send the request.
	std::size_t bytesWritten;
	client.write(request, bytesWritten);

	// Read the response and its headers
	getReponseFromHTTP10QueryFromClient(client, response, responseStream, BUFFER_MAX_SIZE, "");
}

void MBDataTxtDownloader::download()
{
	std::cerr << "Now downloading a MBData file for station " << _stationName << " (" << _host << ")" << std::endl;

	// Form the request. We specify the "Connection: close" header so that the
	// server will close the socket after transmitting the response. This will
	// allow us to treat all data up until the EOF as the content.
	boost::asio::streambuf request;
	std::ostream requestStream(&request);
	std::cerr << "GET " << "(" << _host << ")" << _url << " HTTP/1.0\r\n";
	requestStream << "GET " << _url << " HTTP/1.0\r\n";
	requestStream << "Host: " << _host << "\r\n";
	requestStream << "Accept: */*\r\n";
	requestStream << "Connection: close\r\n\r\n";

	// The response streambuf will automatically grow to accommodate the
	// entire line. The growth may be limited by passing a maximum size to
	// the streambuf constructor.
	asio::streambuf response(BUFFER_MAX_SIZE);
	std::istream fileStream(&response);

	sys::error_code ec;
	try {
		if (_https) {
			downloadHttps(request, response, fileStream);
		} else  {
			downloadHttp(request, response, fileStream);
		}

		auto m = MBDataMessageFactory::chose(_db, _station, _type, fileStream, _timeOffseter);
		if (!m || !(*m)) {
			std::cerr << "Download failed" << std::endl;
			syslog(LOG_ERR, "%s: Download failed", _stationName.c_str());
			return;
		}

		// We are still reading the last file, discard it
		if (m->getDateTime() <= _lastDownloadTime) {
			std::cerr << "File has not been updated" << std::endl;
			return;
		}
		if (m->getDateTime() > chrono::system_clock::now() + chrono::minutes(1)) { // Allow for some clock deviation
			std::cerr << "Station " << _stationName << " has data in the future" << std::endl;
			syslog(LOG_ERR, "%s: Data from the future detected", _stationName.c_str());
			return;
		}

		char uuidStr[CASS_UUID_STRING_LENGTH];
		cass_uuid_string(_station, uuidStr);
		std::cerr << "UUID identified: " << uuidStr << std::endl;
		bool ret = _db.insertV2DataPoint(_station, *m);
		if (ret) {
			std::cerr << "Inserted into database" << std::endl;
		} else {
			std::cerr << "Insertion into database failed" << std::endl;
			syslog(LOG_ERR, "%s: Insertion into database failed", _stationName.c_str());
			return;
		}
		_lastDownloadTime = m->getDateTime();
		ret = _db.updateLastArchiveDownloadTime(_station, chrono::system_clock::to_time_t(_lastDownloadTime));
		if (!ret) {
			std::cerr << "Failed to update the last insertion time" << std::endl;
			syslog(LOG_ERR, "%s: Failed to update the last insertion time", _stationName.c_str());
			return;
		}
	} catch (std::runtime_error& error) {
		std::cerr << "Download failed: " << error.what() << std::endl;
		syslog(LOG_ERR, "%s: Download failed: %s", _stationName.c_str(), error.what());
	}
}

}
