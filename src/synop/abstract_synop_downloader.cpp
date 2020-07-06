/**
 * @file abstractsynopdownloader.cpp
 * @brief Implementation of the AbstractSynopDownloader class
 * @author Laurent Georget
 * @date 2019-02-20
 */
/*
 * Copyright (C) 2019  SAS Météo Concept <contact@meteo-concept.fr>
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
#include <exception>

#include <cstring>
#include <cctype>
#include <syslog.h>
#include <unistd.h>

#include <boost/system/error_code.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/basic_waitable_timer.hpp>
#include <dbconnection_observations.h>

#include "synop_downloader.h"
#include "ogimet_synop.h"
#include "synop_decoder/parser.h"
#include "../http_utils.h"
#include "../blocking_tcp_client.h"

namespace asio = boost::asio;
namespace ip = boost::asio::ip;
namespace sys = boost::system; //system() is a function, it cannot be redefined
//as a namespace
namespace chrono = std::chrono;
namespace args = std::placeholders;

namespace meteodata {

using namespace date;

constexpr char AbstractSynopDownloader::HOST[];

AbstractSynopDownloader::AbstractSynopDownloader(asio::io_service& ioService, DbConnectionObservations& db) :
	_ioService(ioService),
	_db(db),
	_timer(_ioService)
{
}

void AbstractSynopDownloader::waitUntilNextDownload()
{
	auto self(shared_from_this());
	_timer.expires_from_now(computeWaitDuration());
	_timer.async_wait(std::bind(&AbstractSynopDownloader::checkDeadline, self, args::_1));
}

void AbstractSynopDownloader::checkDeadline(const sys::error_code& e)
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
			syslog(LOG_ERR, "SYNOP: Getting the SYNOP messages failed (%s), will retry", e.what());
			// nothing more, just go back to sleep and retry next time
		}
		// Going back to sleep
		waitUntilNextDownload();
	} else {
		/* spurious handler call, restart the timer without changing the
		 * deadline */
		auto self(shared_from_this());
		_timer.async_wait(std::bind(&AbstractSynopDownloader::checkDeadline, self, args::_1));
	}
}

void AbstractSynopDownloader::download()
{
	std::cerr << "Now downloading SYNOP messages " << std::endl;

	// Try each endpoint until we successfully establish a connection.
	BlockingTcpClient<ip::tcp::socket> client(chrono::seconds(5));
	client.connect(HOST, "http");

	// Form the request. We specify the "Connection: close" header so that the
	// server will close the socket after transmitting the response. This will
	// allow us to treat all data up until the EOF as the content.
	boost::asio::streambuf request;
	std::ostream requestStream(&request);
	buildDownloadRequest(requestStream);

	// Send the request.
	std::size_t bytesWritten;
	client.write(request, bytesWritten);

	// Read the response status line. The response streambuf will automatically
	// grow to accommodate the entire line. The growth may be limited by passing
	// a maximum size to the streambuf constructor.
	asio::streambuf response;
	std::size_t bytesReadInFirstLine;
	client.read_until(response, "\r\n", bytesReadInFirstLine, true);

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
	std::size_t size;
	client.read_until(response, "\r\n\r\n", size, true);

	// Discard the headers
	// XXX Should we do something about them?
	response.consume(size);

	// Read the body
	sys::error_code ec;
	do {
		ec = client.read_until(response, "\n", size, false);
		std::string line;
		std::getline(responseStream, line);

		// Deal with the annoying case as early as possible
		if (line.empty() || line.find("NIL") != std::string::npos)
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

				std::pair<bool, float> rainfall24 = std::make_pair(false, 0.f);
				std::pair<bool, int> insolationTime24 = std::make_pair(false, 0);
				auto it = std::find_if(m._precipitation.begin(), m._precipitation.end(),
						[](const auto& p) { return p._duration == 24; });
				if (it != m._precipitation.end())
					rainfall24 = std::make_pair(true, it->_amount);
				if (m._minutesOfSunshineLastDay)
					insolationTime24 = std::make_pair(true, *m._minutesOfSunshineLastDay);
				auto day = date::floor<date::days>(m._observationTime) - date::days(1);
				_db.insertV2EntireDayValues(uuidIt->second, date::sys_seconds(day).time_since_epoch().count(), rainfall24, insolationTime24);
				if (m._minTemperature)
					_db.insertV2Tn(uuidIt->second, chrono::system_clock::to_time_t(m._observationTime), *m._minTemperature / 10.f);
				if (m._maxTemperature)
					_db.insertV2Tx(uuidIt->second, chrono::system_clock::to_time_t(m._observationTime), *m._maxTemperature / 10.f);
			}
		} else {
			std::cerr << "Record looks invalid, discarding..." << std::endl;
		}
	} while (!ec);
}

}
