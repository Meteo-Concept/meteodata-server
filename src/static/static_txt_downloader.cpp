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
#include <functional>
#include <chrono>

#include <syslog.h>
#include <unistd.h>

#include <boost/system/error_code.hpp>
#include <boost/asio/basic_waitable_timer.hpp>
#include <boost/asio/io_service.hpp>
#include <date/date.h>
#include <dbconnection_observations.h>

#include "../time_offseter.h"
#include "../curl_wrapper.h"
#include "static_txt_downloader.h"
#include "static_message.h"

// we do not expect the files to be big, so it's simpler and more
// efficient to just slurp them, which means we'd better limit the
// buffer size, for safety's sake
#define BUFFER_MAX_SIZE 4096

namespace asio = boost::asio;
namespace sys = boost::system;
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
	int pollingPeriod;
	db.getStationCoordinates(station, latitude, longitude, elevation, _stationName, pollingPeriod);

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
	_timer.expires_from_now(chrono::minutes(10 - tod.minutes().count() % 10 + 2) - chrono::seconds(tod.seconds().count()));
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
	std::cerr << "Now downloading a StatIC file for station " << _stationName << " (" << _host << ")" << std::endl;

	std::ostringstream query;
	query << (_https ? "https://" : "http://")
	      << _host
	      << _url;

	CurlWrapper client;

	CURLcode ret = client.download(query.str(), [&](const std::string& body) {
		std::istringstream responseStream{body};

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

			date::local_seconds localMidnight = date::floor<date::days>(_timeOffseter.convertToLocalTime(downloadTime));
			date::sys_seconds localMidnightInUTC = _timeOffseter.convertFromLocalTime(localMidnight);
			auto beginDay = chrono::system_clock::to_time_t(localMidnightInUTC);
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
	});

	if (ret != CURLE_OK) {
		std::string_view error = client.getLastError();
		std::ostringstream errorStream;
		errorStream << "Download failed for " << _stationName << " Bad response from " << _host << ": " << error;
		std::string errorMsg = errorStream.str();
		syslog(LOG_ERR, "%s", errorMsg.data());
	}
}

}
