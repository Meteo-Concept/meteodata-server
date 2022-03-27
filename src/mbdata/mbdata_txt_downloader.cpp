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
#include <chrono>
#include <tuple>
#include <string>
#include <functional>

#include <systemd/sd-daemon.h>
#include <unistd.h>

#include <boost/asio/basic_waitable_timer.hpp>
#include <boost/asio/io_service.hpp>
#include <date.h>
#include <dbconnection_observations.h>

#include "../cassandra_utils.h"
#include "mbdata_txt_downloader.h"
#include "mbdata_messages/mbdata_message_factory.h"
#include "../time_offseter.h"
#include "../curl_wrapper.h"

// we do not expect the files to be big, so it's simpler and more
// efficient to just slurp them, which means we'd better limit the
// buffer size, for safety's sake
#define BUFFER_MAX_SIZE 4096

namespace asio = boost::asio;
namespace args = std::placeholders;

namespace meteodata
{

using namespace date;

MBDataTxtDownloader::MBDataTxtDownloader(asio::io_service& ioService, DbConnectionObservations& db,
										 const std::tuple<CassUuid, std::string, std::string, bool, int, std::string>& downloadDetails)
		:
		_ioService(ioService),
		_db(db),
		_timer(_ioService),
		_station(std::get<0>(downloadDetails)),
		_host(std::get<1>(downloadDetails)),
		_url(std::get<2>(downloadDetails)),
		_https(std::get<3>(downloadDetails)),
		_type(std::get<5>(downloadDetails)),
		_lastDownloadTime(chrono::seconds(
				0)) // any impossible date will do before the first download, if it's old enough, it cannot correspond to any date sent by the station
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
	_mustStop = false;
	waitUntilNextDownload();
}

void MBDataTxtDownloader::stop()
{
	_mustStop = true;
	_timer.cancel();
}

void MBDataTxtDownloader::waitUntilNextDownload()
{
	auto self(shared_from_this());
	auto target = chrono::steady_clock::now();
	auto daypoint = date::floor<date::days>(target);
	auto tod = date::make_time(target - daypoint);
	_timer.expires_from_now(
			chrono::minutes(10 - tod.minutes().count() % 10 + 2) - chrono::seconds(tod.seconds().count()));
	_timer.async_wait(std::bind(&MBDataTxtDownloader::checkDeadline, self, args::_1));
}

void MBDataTxtDownloader::checkDeadline(const sys::error_code& e)
{
	/* if the timer has been cancelled, then bail out ; we probably have been
	 * asked to die */
	if (e == sys::errc::operation_canceled)
		return;

	// verify that the timeout is not spurious
	if (_timer.expires_at() <= chrono::steady_clock::now()) {
		try {
			download();
		} catch (std::exception& e) {
			std::cerr << SD_ERR << "MBData file: Couldn't download from " << _host << ": " << e.what() << std::endl;
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

void MBDataTxtDownloader::download()
{
	std::cout << SD_INFO << "[MBData " << _station << "] measurement: " << "Downloading a MBData file for station "
			  << _stationName << " (" << _host << ")" << std::endl;

	std::ostringstream query;
	query << (_https ? "https://" : "http://") << _host << _url;

	CurlWrapper client;

	CURLcode ret = client.download(query.str(), [&](const std::string& body) {
		std::istringstream fileStream{body};

		auto m = MBDataMessageFactory::chose(_db, _station, _type, fileStream, _timeOffseter);
		if (!m || !(*m)) {
			std::cerr << SD_ERR << "[MBData " << _station << "] protocol: " << "Download failed for station "
					  << _stationName << std::endl;
			return;
		}

		// We are still reading the last file, discard it
		if (m->getDateTime() <= _lastDownloadTime) {
			std::cerr << SD_NOTICE << "[MBData " << _station << "] measurement: " << "File for station " << _stationName
					  << " has not been updated" << std::endl;
			return;
		}
		if (m->getDateTime() > chrono::system_clock::now() + chrono::minutes(1)) { // Allow for some clock deviation
			std::cerr << SD_ERR << "[MBData " << _station << "] management: " << "Station " << _stationName
					  << " has data in the future" << std::endl;
			return;
		}

		char uuidStr[CASS_UUID_STRING_LENGTH];
		cass_uuid_string(_station, uuidStr);
		bool ret = _db.insertV2DataPoint(m->getObservation(_station));
		if (ret) {
			std::cout << SD_DEBUG << "[MBData " << _station << "] measurement: " << "Data from station " << _stationName
					  << " inserted into database" << std::endl;
		} else {
			std::cerr << SD_ERR << "[MBData " << _station << "] measurement: "
					  << "Insertion into database failed for station " << _stationName << std::endl;
			return;
		}
		_lastDownloadTime = m->getDateTime();
		ret = _db.updateLastArchiveDownloadTime(_station, chrono::system_clock::to_time_t(_lastDownloadTime));
		if (!ret) {
			std::cerr << SD_ERR << "[MBData " << _station << "] management: "
					  << "Failed to update the last insertion time of station " << _stationName << std::endl;
			return;
		}
	});

	if (ret != CURLE_OK) {
		std::string_view error = client.getLastError();
		std::cerr << SD_ERR << "[MBData " << _station << "] protocol: " << "Download failed for " << _stationName
				  << ", bad response from " << _host << ": " << error;
	}
}

}
