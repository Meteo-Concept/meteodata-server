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
#include <boost/asio/io_service.hpp>
#include <dbconnection_observations.h>

#include "ship_and_buoy_downloader.h"
#include "meteo_france_ship_and_buoy.h"
#include "../curl_wrapper.h"

namespace asio = boost::asio;
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
	if (e == sys::errc::operation_canceled)
		return;

	// verify that the timeout is not spurious
	if (_timer.expires_at() <= chrono::steady_clock::now()) {
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
	std::cout << SD_NOTICE << "Now downloading SHIP and BUOY data " << std::endl;
	auto ymd = date::year_month_day(date::floor<date::days>(chrono::system_clock::now() - date::days(1)));

	CurlWrapper client;

	CURLcode ret = client.download(std::string{"https://"} + HOST + date::format(URL, ymd), [&](const std::string& body) {
		std::istringstream responseStream{body};

		std::string line;
		std::getline(responseStream, line);
		std::istringstream lineIterator{line};
		std::vector<std::string> fields;
		for (std::string field ; std::getline(lineIterator, field, ';') ;)
			if (field != "")
				fields.emplace_back(std::move(field));

		while (std::getline(responseStream, line)) {
			lineIterator = std::istringstream{line};
			MeteoFranceShipAndBuoy m{lineIterator, fields};
			if (!m)
				continue;
			auto uuidIt = _icaos.find(m.getIdentifier());
			if (uuidIt != _icaos.end()) {
				char uuidStr[CASS_UUID_STRING_LENGTH];
				cass_uuid_string(uuidIt->second, uuidStr);
				std::cout << SD_DEBUG << "UUID identified: " << uuidStr << std::endl;
				bool ret = _db.insertV2DataPoint(uuidIt->second, m);
				if (ret) {
					std::cout << SD_DEBUG << "SHIP ou BUOY data inserted into database for station " << uuidStr << std::endl;
				} else {
					std::cerr << SD_ERR << "Failed to insert SHIP ou BUOY data into database for station " << uuidStr << std::endl;
				}
			}
		}
	});

	if (ret != CURLE_OK) {
		std::string_view error = client.getLastError();
		std::cerr << SD_ERR << "Failed to download SHIP and BUOY data: " << error << std::endl;
	}
}

}
