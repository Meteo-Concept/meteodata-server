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
#include <systemd/sd-daemon.h>
#include <unistd.h>

#include <boost/system/error_code.hpp>
#include <boost/asio/basic_waitable_timer.hpp>
#include <boost/asio/io_context.hpp>
#include <dbconnection_observations.h>
#include <date.h>

#include "ship_and_buoy_downloader.h"
#include "meteo_france_ship_and_buoy.h"
#include "../curl_wrapper.h"
#include "../cassandra_utils.h"
#include "../connector.h"

namespace asio = boost::asio;
namespace sys = boost::system;
namespace chrono = std::chrono;
namespace args = std::placeholders;

namespace meteodata
{
using namespace date;

ShipAndBuoyDownloader::ShipAndBuoyDownloader(asio::io_context& ioContext, DbConnectionObservations& db) :
		Connector{ioContext, db},
		_timer{ioContext}
{
	_status.shortStatus = "IDLE";
}

void ShipAndBuoyDownloader::start()
{
	_status.shortStatus = "OK";
	_status.activeSince = date::floor<chrono::seconds>(chrono::system_clock::now());
	_status.lastReloaded = date::floor<chrono::seconds>(chrono::system_clock::now());

	_mustStop = false;
	reloadStations();
	waitUntilNextDownload();
}

void ShipAndBuoyDownloader::stop()
{
	_status.shortStatus = "STOPPED";
	_mustStop = true;
	_timer.cancel();
}

void ShipAndBuoyDownloader::reload()
{
	_status.lastReloaded = date::floor<chrono::seconds>(chrono::system_clock::now());
	_status.nbDownloads = 0;

	_timer.cancel();
	reloadStations();
	waitUntilNextDownload();
}

void ShipAndBuoyDownloader::waitUntilNextDownload()
{
	auto self(shared_from_this());
	auto target = chrono::steady_clock::now();
	auto daypoint = date::floor<date::days>(target);
	auto tod = date::make_time(target - daypoint);

	_status.timeToNextDownload = chrono::hours(6 - (tod.hours().count() % 6)) - chrono::minutes(tod.minutes().count());
	_timer.expires_from_now(_status.timeToNextDownload);
	_timer.async_wait([this, self] (const sys::error_code& e) { return checkDeadline(e); });
}

void ShipAndBuoyDownloader::checkDeadline(const sys::error_code& e)
{
	/* if the timer has been cancelled, then bail out ; we probably have been
	 * asked to die */
	if (e == sys::errc::operation_canceled)
		return;

	// verify that the timeout is not spurious
	if (_timer.expires_at() <= chrono::steady_clock::now()) {
		download();
		// Going back to sleep unless we shouldn't
		if (!_mustStop)
			waitUntilNextDownload();
	} else {
		/* spurious handler call, restart the timer without changing the
		 * deadline */
		auto self(shared_from_this());
		_timer.async_wait([this, self] (const sys::error_code& e) { return checkDeadline(e); });
	}
}

void ShipAndBuoyDownloader::download()
{
	std::cout << SD_NOTICE << "[SHIP] measurement: " << "Now downloading SHIP and BUOY data " << std::endl;
	auto ymd = date::year_month_day(date::floor<date::days>(chrono::system_clock::now() - date::days(1)));

	CurlWrapper client;

	CURLcode ret = client.download(std::string{"https://"} + HOST + date::format(URL, ymd),
								   [&](const std::string& body) {
		std::istringstream responseStream{body};

		std::string line;
		std::getline(responseStream, line);
		std::istringstream lineIterator{line};
		std::vector<std::string> fields;
		for (std::string field ; std::getline(lineIterator, field, ';') ;)
			   if (!field.empty())
				   fields.emplace_back(std::move(field));

		while (std::getline(responseStream, line)) {
			   lineIterator = std::istringstream{line};
			   MeteoFranceShipAndBuoy m{lineIterator, fields};
			   if (!m)
				   continue;
			   auto uuidIt = _icaos.find(m.getIdentifier());
			   if (uuidIt != _icaos.end()) {
				   std::cout << SD_DEBUG << "[SHIP " << uuidIt->second << "] protocol: "
							 << "UUID identified: " << uuidIt->second << std::endl;
				   bool ret = _db.insertV2DataPoint(m.getObservation(uuidIt->second));
				   if (ret) {
					   std::cout << SD_DEBUG << "[SHIP " << uuidIt->second
								 << "] measurement: "
								 << "SHIP ou BUOY data inserted into database for station "
								 << uuidIt->second << std::endl;
				   } else {
					   std::cerr << SD_ERR << "[SHIP " << uuidIt->second
								 << "] measurement: "
								 << "Failed to insert SHIP ou BUOY data into database for station "
								 << uuidIt->second << std::endl;
				   }
			   }
		}
	});

	if (ret != CURLE_OK) {
		std::string_view error = client.getLastError();
		std::cerr << SD_ERR << "[SHIP] protocol: " << "Failed to download SHIP and BUOY data: " << error << std::endl;
	}
}

void ShipAndBuoyDownloader::reloadStations()
{
	_icaos.clear();
	std::vector<std::tuple<CassUuid, std::string>> icaos;
	_db.getAllIcaos(icaos);
	for (auto&& icao : icaos)
		_icaos.emplace(std::get<1>(icao), std::get<0>(icao));
}

}
