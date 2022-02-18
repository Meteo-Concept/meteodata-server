/**
 * @file deferredsynopdownloader.h
 * @brief Definition of the DeferredSynopDownloader class
 * @author Laurent Georget
 * @date 2019-02-20
 */
/*
 * Copyright (C) 2019  SAS JD Environnement <contact@meteo-concept.fr>
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

#ifndef DEFERREDSYNOPDOWNLOADER_H
#define DEFERREDSYNOPDOWNLOADER_H

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

namespace meteodata
{

namespace ip = boost::asio::ip;
namespace asio = boost::asio;
namespace sys = boost::system;
namespace chrono = std::chrono;

using namespace std::placeholders;
using namespace meteodata;

/**
 */
class DeferredSynopDownloader : public AbstractSynopDownloader
{
public:
	DeferredSynopDownloader(asio::io_service& ioService, DbConnectionObservations& db, const std::string& icao,
							const CassUuid& uuid);
	void start() override;

private:
	std::string _icao;
	chrono::minutes computeWaitDuration() override;
	void buildDownloadRequest(std::ostream& out) override;
};

}

#endif
