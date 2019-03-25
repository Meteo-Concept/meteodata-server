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
#include <vector>

#include <boost/asio.hpp>

#include "meteo_server.h"
#include "timeoffseter.h"
#include "connector.h"
#include "weatherlinkdownloadscheduler.h"
#include "weatherlinkdownloader.h"
#include "vantagepro2connector.h"
#include "synopdownloader.h"
#include "deferredsynopdownloader.h"
#include "mqttsubscriber.h"
#include "statictxtdownloader.h"
#include "shipandbuoydownloader.h"
#include "mbdatatxtdownloader.h"

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
	// Start the MQTT subscribers (one per station)
	std::vector<std::tuple<CassUuid, std::string, int, std::string, std::unique_ptr<char[]>, size_t, std::string, int>> mqttStations;
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

	// Start the Synop downloader worker (one for all the SYNOP stations)
	auto synopDownloader = std::make_shared<SynopDownloader>(_ioService, _db);
	synopDownloader->start();

	// Start the deferred SYNOP downloader worker (one for each deferred SYNOP)
	std::vector<std::tuple<CassUuid, std::string>> deferredSynops;
	_db.getDeferredSynops(deferredSynops);
	for (auto&& synop : deferredSynops) {
		auto deferredSynopDownloader =
			std::make_shared<DeferredSynopDownloader>(
					_ioService, _db,
					std::get<1>(synop),
					std::get<0>(synop)
				);
		deferredSynopDownloader->start();
	}

	// Start the Meteo France SHIP and BUOY downloader (one for all SHIP and BUOY messages)
	auto meteofranceDownloader = std::make_shared<ShipAndBuoyDownloader>(_ioService, _db);
	meteofranceDownloader->start();

	// Start the StatIC stations downloaders (one per station)
	std::vector<std::tuple<CassUuid, std::string, std::string>> statICTxtStations;
	_db.getStatICTxtStations(statICTxtStations);
	for (auto&& station : statICTxtStations) {
		auto subscriber =
			std::make_shared<StatICTxtDownloader>(
				_ioService, _db,
				std::get<0>(station),
				std::get<1>(station),
				std::get<2>(station)
			);
		subscriber->start();
	}

	// Start the Weatherlink downloaders workers (one per Weatherlink station)
	auto weatherlinkScheduler = std::make_shared<WeatherlinkDownloadScheduler>(_ioService, _db);
	std::vector<std::tuple<CassUuid, std::string, std::string, int>> weatherlinkStations;
	_db.getAllWeatherlinkStations(weatherlinkStations);
	for (const auto& station : weatherlinkStations) {
		weatherlinkScheduler->add(
			std::get<0>(station), std::get<1>(station), std::get<2>(station),
			TimeOffseter::PredefinedTimezone(std::get<3>(station))
		);
	}
	weatherlinkScheduler->start();

	// Start the MBData txt downloaders workers (one per station)
	std::vector<std::tuple<CassUuid, std::string, std::string, bool, int, std::string>> mbDataTxtStations;
	_db.getMBDataTxtStations(mbDataTxtStations);
	for (auto&& station : mbDataTxtStations) {
		auto subscriber =
			std::make_shared<MBDataTxtDownloader>(
				_ioService, _db,
				station
			);
		subscriber->start();
	}

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
