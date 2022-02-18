/**
 * @file synopdownloader.h
 * @brief Definition of the SynopDownloader class
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

#ifndef SYNOPDOWNLOADER_H
#define SYNOPDOWNLOADER_H

#include <iostream>
#include <memory>
#include <array>
#include <vector>
#include <functional>
#include <map>
#include <unistd.h>

#include <boost/system/error_code.hpp>
#include <boost/asio.hpp>
#include <boost/asio/basic_waitable_timer.hpp>
#include <cassandra.h>
#include <date/date.h>
#include <date/tz.h>
#include <dbconnection_observations.h>

#include "abstract_synop_downloader.h"

namespace meteodata {

namespace ip = boost::asio::ip;
namespace asio = boost::asio;
namespace sys = boost::system;
namespace chrono = std::chrono;

using namespace std::placeholders;
using namespace meteodata;

/**
 * A downloader for recent (last three or so hours) SYNOP data from Ogimet
 */
class SynopDownloader : public AbstractSynopDownloader
{
public:
	/**
	 * Construct the downloader
	 *
	 * @param ioService The Boost service to handle asynchronous events and
	 * callbacks
	 * @param db The connection to the observations database
	 * @param group The prefix of the SYNOP stations to be downloaded (can
	 * be an entire SYNOP to download just one station, or a country prefix
	 * like 07 for France)
	 */
	SynopDownloader(asio::io_service& ioService, DbConnectionObservations& db, const std::string& group);
	void start() override;

	/**
	 * The SYNOP country prefix for France
	 */
	static constexpr char GROUP_FR[] = "07";
	/**
	 * The SYNOP country prefix for Luxemburg
	 */
	static constexpr char GROUP_LU[] = "06";

private:
	chrono::minutes computeWaitDuration() override;
	void buildDownloadRequest(std::ostream& out) override;

	/**
	 * The prefix of the SYNOP stations to download
	 *
	 * It can be one of the static prefixes in this class like GROUP_FR, or
	 * a complete SYNOP identifier, or anything that is a valid prefix for
	 * SYNOP stations.
	 */
	const std::string _group;
};

}

#endif
