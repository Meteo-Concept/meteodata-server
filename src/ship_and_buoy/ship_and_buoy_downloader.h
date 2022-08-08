/**
 * @file shipandbuoydownloader.h
 * @brief Definition of the ShipAndBuoyDownloader class
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

#ifndef SHIP_AND_BUOY_DOWNLOADER_H
#define SHIP_AND_BUOY_DOWNLOADER_H

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
#include <date.h>
#include <tz.h>
#include <dbconnection_observations.h>

#include "../abstract_download_scheduler.h"

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
class ShipAndBuoyDownloader : public AbstractDownloadScheduler
{
public:
	ShipAndBuoyDownloader(asio::io_context& ioContext, DbConnectionObservations& db);

private:
	std::map<std::string, CassUuid> _icaos;

	static constexpr char HOST[] = "donneespubliques.meteofrance.fr";
	static constexpr char URL[] = "/donnees_libres/Txt/Marine/marine.%Y%m%d.csv";
	static constexpr int POLLING_PERIOD_HOURS = 6;

	void download() override;
	void reloadStations() override;
};

}

#endif /* SHIP_AND_BUOY_DOWNLOADER_H */
