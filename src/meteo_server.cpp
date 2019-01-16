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

#include <memory>
#include <tuple>
#include <functional>

#include <boost/asio.hpp>

#include "meteo_server.h"
#include "timeoffseter.h"
#include "connector.h"
#include "weatherlinkdownloader.h"
#include "vantagepro2connector.h"
#include "synopdownloader.h"
#include "mqttsubscriber.h"
#include "shipandbuoydownloader.h"

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
	// Start the Weatherlink downloaders workers (one per Weatherlink station)
/*	std::vector<std::tuple<CassUuid, std::string, std::string, int>> weatherlinkStations;
	_db.getAllWeatherlinkStations(weatherlinkStations);
	for (const auto& station : weatherlinkStations) {
		auto wld =
			std::make_shared<WeatherlinkDownloader>(
				std::get<0>(station), std::get<1>(station), std::get<2>(station),
				_ioService, _db,
				TimeOffseter::PredefinedTimezone(std::get<3>(station))
			);
		wld->start();
	}
*/
	// Start the MQTT subscribers (one per station)
/*	std::vector<std::tuple<CassUuid, std::string, int, std::string, std::unique_ptr<char[]>, size_t, std::string, int>> mqttStations;
	_db.getMqttStations(mqttStations);
	for (auto&& station : mqttStations) {
		auto subscriber =
			std::make_shared<MqttSubscriber>(
				std::get<0>(station),
				MqttSubscriber::MqttSubscriptionDetails{
					std::get<1>(station),
					std::get<2>(station),
					std::get<3>(station),
					std::move(std::get<4>(station)),
					std::get<5>(station),
					std::get<6>(station)
				},
				_ioService, _db,
				TimeOffseter::PredefinedTimezone(std::get<7>(station))
			);
		subscriber->start();
	}
*/
	// Start the Synop downloader worker (one for all the SYNOP stations)
/*	auto synopDownloader = std::make_shared<SynopDownloader>(_ioService, _db);
	synopDownloader->start();
*/

	auto meteofranceDownloader = std::make_shared<ShipAndBuoyDownloader>(_ioService, _db);
	meteofranceDownloader->start();
	// Listen on the Meteodata port for incoming stations (one connector per direct-connect station)
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
