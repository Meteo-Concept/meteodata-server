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

#ifndef SHIPANDBUOYDOWNLOADER_H
#define SHIPANDBUOYDOWNLOADER_H

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

#include "../connector.h"

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
class ShipAndBuoyDownloader : public Connector
{
public:
	ShipAndBuoyDownloader(asio::io_context& ioContext, DbConnectionObservations& db);
	void start() override;
	void stop() override;
	void reload() override;

private:
	asio::basic_waitable_timer<chrono::steady_clock> _timer;
	std::map<std::string, CassUuid> _icaos;
	bool _mustStop = false;

	static constexpr char HOST[] = "donneespubliques.meteofrance.fr";
	static constexpr char URL[] = "/donnees_libres/Txt/Marine/marine.%Y%m%d.csv";

	void checkDeadline(const sys::error_code& e);
	void waitUntilNextDownload();
	void download();
	void reloadStations();
};

}

#endif
