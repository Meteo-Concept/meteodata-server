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
#include <exception>

#include <cstring>
#include <cctype>
#include <unistd.h>

#include <boost/system/error_code.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/basic_waitable_timer.hpp>
#include <dbconnection_observations.h>

#include "synop_downloader.h"
#include "abstract_synop_downloader.h"
#include "ogimet_synop.h"
#include "synop_decoder/parser.h"

namespace asio = boost::asio;
namespace ip = boost::asio::ip;
namespace sys = boost::system; //system() is a function, it cannot be redefined
//as a namespace
namespace chrono = std::chrono;
namespace args = std::placeholders;

namespace meteodata {

using namespace date;

constexpr char SynopDownloader::GROUP_FR[];

SynopDownloader::SynopDownloader(asio::io_service& ioService, DbConnectionObservations& db, const std::string& group) :
	AbstractSynopDownloader(ioService, db),
	_group(group)
{
}

void SynopDownloader::start()
{
	std::vector<std::tuple<CassUuid, std::string>> icaos;
	_db.getAllIcaos(icaos);
	for (auto&& icao : icaos)
		_icaos.emplace(std::get<1>(icao), std::get<0>(icao));
	waitUntilNextDownload();
}

chrono::minutes SynopDownloader::computeWaitDuration()
{
	auto time = chrono::system_clock::now();
	auto daypoint = date::floor<date::days>(time);
	auto tod = date::make_time(time - daypoint);
	return chrono::minutes(20 - tod.minutes().count() % 20);
}

void SynopDownloader::buildDownloadRequest(std::ostream& out)
{
	auto time = chrono::system_clock::now() - chrono::hours(3);
	auto daypoint = date::floor<date::days>(time);
	auto ymd = date::year_month_day(daypoint);   // calendar date
	auto tod = date::make_time(time - daypoint); // Yields time_of_day type

	// Obtain individual components as integers
	auto y   = int(ymd.year());
	auto m   = unsigned(ymd.month());
	auto d   = unsigned(ymd.day());
	auto h   = tod.hours().count();
	auto min = 30;

	out << "/cgi-bin/getsynop?begin=" << std::setfill('0')
		<< std::setw(4) << y
		<< std::setw(2) << m
		<< std::setw(2) << d
		<< std::setw(2) << h
		<< std::setw(2) << min
		<< "&block=" << _group;
}

}
