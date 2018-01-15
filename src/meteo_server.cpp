/**
 * @file meteo_server.cpp
 * @brief Implementation of the MeteoServer class
 * @author Laurent Georget
 * @date 2016-10-05
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

#include <syslog.h>

#include <tuple>
#include <functional>

#include <boost/asio.hpp>

#include "meteo_server.h"
#include "timeoffseter.h"
#include "connector.h"
#include "weatherlinkdownloader.h"
#include "vantagepro2connector.h"

using namespace boost::asio;
using namespace boost::asio::ip;

namespace meteodata
{

MeteoServer::MeteoServer(boost::asio::io_service& ioService, const std::string& address,
		const std::string& user, const std::string& password) :
	_ioService(ioService),
	_acceptor(ioService, tcp::endpoint(tcp::v4(), 5886)),
	_db(address, user, password)
{
	syslog(LOG_NOTICE, "Meteodata has started succesfully");
}

void MeteoServer::start()
{
	std::vector<std::tuple<CassUuid, std::string, int>> weatherlinkStations;
	_db.getAllWeatherlinkStations(weatherlinkStations);
	for (const auto& station : weatherlinkStations) {
		auto wld =
			std::make_shared<WeatherlinkDownloader>(
				std::get<0>(station), std::get<1>(station),
				_ioService, _db,
				TimeOffseter::PredefinedTimezone(std::get<2>(station))
			);
		wld->start();
	}
	startAccepting();
}

void MeteoServer::startAccepting()
{
	Connector::ptr newConnector =
		Connector::create<VantagePro2Connector>(_acceptor.get_io_service(), _db);
	_acceptor.async_accept(newConnector->socket(),
		std::bind(&MeteoServer::runNewConnector, this,
			newConnector, std::placeholders::_1)
	);
}

void MeteoServer::runNewConnector(Connector::ptr c,
		const boost::system::error_code& error)
{
	if (!error) {
		startAccepting();
		c->start();
	}
}
}
