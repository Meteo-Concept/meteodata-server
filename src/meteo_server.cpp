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

#include <systemd/sd-daemon.h>

#include <memory>
#include <tuple>
#include <functional>
#include <vector>

#include <boost/asio.hpp>

#include "connector.h"
#include "meteo_server.h"
#include "time_offseter.h"
#include "davis/vantagepro2_connector.h"
#include "davis/weatherlink_download_scheduler.h"
#include "davis/weatherlink_downloader.h"
#include "mbdata/mbdata_txt_downloader.h"
#include "mqtt/mqtt_subscriber.h"
#include "mqtt/vp2_mqtt_subscriber.h"
#include "mqtt/objenious_mqtt_subscriber.h"
#include "ship_and_buoy/ship_and_buoy_downloader.h"
#include "static/static_txt_downloader.h"
#include "synop/deferred_synop_downloader.h"
#include "synop/synop_downloader.h"
#include "pessl/fieldclimate_api_download_scheduler.h"
#include "objenious/objenious_api_download_scheduler.h"

using namespace boost::asio;
using namespace boost::asio::ip;

namespace meteodata
{

MeteoServer::MeteoServer(boost::asio::io_service& ioService, const std::string& address,
		const std::string& user, const std::string& password,
		const std::string& weatherlinkAPIv2Key, const std::string& weatherlinkAPIv2Secret,
		const std::string& fieldClimateApiKey, const std::string& fieldClimateApiSecret,
		const std::string& objeniousApiKey) :
	_ioService(ioService),
	_acceptor(ioService, tcp::endpoint(tcp::v4(), 5886)),
	_db(address, user, password),
	_weatherlinkAPIv2Key(weatherlinkAPIv2Key),
	_weatherlinkAPIv2Secret(weatherlinkAPIv2Secret),
	_fieldClimateApiKey(fieldClimateApiKey),
	_fieldClimateApiSecret(fieldClimateApiSecret),
	_objeniousApiKey(objeniousApiKey)
{
	std::cerr << SD_INFO << "Meteodata has started succesfully";
}

void MeteoServer::start()
{
#if 0
	// Start the MQTT subscribers (one per station)
	std::vector<std::tuple<CassUuid, std::string, int, std::string, std::unique_ptr<char[]>, size_t, std::string, int>> mqttStations;
	std::vector<std::tuple<CassUuid, std::string, std::map<std::string, std::string>>> objeniousStations;
	_db.getAllObjeniousApiStations(objeniousStations);
	_db.getMqttStations(mqttStations);
	for (auto&& station : mqttStations) {
		MqttSubscriber::MqttSubscriptionDetails details{
			std::get<1>(station),
			std::get<2>(station),
			std::get<3>(station),
			std::move(std::get<4>(station)),
			std::get<5>(station),
			std::get<6>(station)
		};

		std::shared_ptr<MqttSubscriber> subscriber;
		if (details.topic.substr(0, 4) == "vp2/") {
			subscriber.reset(new VP2MqttSubscriber(
						std::get<0>(station),
						std::move(details),
						_ioService, _db,
						TimeOffseter::PredefinedTimezone(std::get<7>(station))
					));
		} else if (details.topic.substr(0, 11) == "objenious/") {
			const CassUuid& mqttSt = std::get<0>(station);
			auto it = std::find_if(objeniousStations.begin(), objeniousStations.end(),
					[&mqttSt](auto&& objSt){ return mqttSt == std::get<0>(objSt); });
			if (it != objeniousStations.end()) {
				subscriber.reset(new ObjeniousMqttSubscriber(
							std::get<0>(station),
							std::move(details),
							std::get<1>(*it), std::get<2>(*it),
							_ioService, _db,
							TimeOffseter::PredefinedTimezone(std::get<7>(station))
						));
			} else {
				std::cerr << SD_ERR << "An Objenious MQTT connector is configured for station " << std::get<0>(station)
					<< " but no corresponding Objenious station has been found" << std::endl;
			}
		}

		if (subscriber) {
			subscriber->start();
		} else {
			std::cerr << SD_ERR << "Unrecognized topic " << details.topic
				<< " for MQTT station " << std::get<0>(station) << std::endl;
		}
	}

	// Start the Synop downloader worker (one for all the SYNOP stations in
	// the same group)
	auto synopDownloaderFr = std::make_shared<SynopDownloader>(_ioService, _db, SynopDownloader::GROUP_FR);
	synopDownloaderFr->start();
	auto synopDownloaderLu = std::make_shared<SynopDownloader>(_ioService, _db, SynopDownloader::GROUP_LU);
	synopDownloaderLu->start();

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
	std::vector<std::tuple<CassUuid, std::string, std::string, bool, int>> statICTxtStations;
	_db.getStatICTxtStations(statICTxtStations);
	for (auto&& station : statICTxtStations) {
		auto subscriber =
			std::make_shared<StatICTxtDownloader>(
					_ioService, _db,
					std::get<0>(station),
					std::get<1>(station),
					std::get<2>(station),
					std::get<3>(station),
					std::get<4>(station)
				);
		subscriber->start();
	}

	// Start the Weatherlink download scheduler (one for all Weatherlink stations, one downloader per station)
	auto weatherlinkScheduler = std::make_shared<WeatherlinkDownloadScheduler>(_ioService, _db, std::move(_weatherlinkAPIv2Key), std::move(_weatherlinkAPIv2Secret));
	weatherlinkScheduler->start();

	// Start the FieldClimate download scheduler (one for all Pessl stations, one downloader per station)
	auto fieldClimateScheduler = std::make_shared<FieldClimateApiDownloadScheduler>(_ioService, _db, std::move(_fieldClimateApiKey), std::move(_fieldClimateApiSecret));
	fieldClimateScheduler->start();
#endif

	// Start the Objenious download scheduler (one for all Objenious
	// stations on our account, one downloader per station)
	auto objeniousScheduler = std::make_shared<ObjeniousApiDownloadScheduler>(_ioService, _db, std::move(_objeniousApiKey));
	objeniousScheduler->start();

#if 0
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
#endif

	// Listen on the Meteodata port for incoming stations (one connector per direct-connect station)
	startAccepting();
}

void MeteoServer::startAccepting()
{
	Connector::ptr newConnector =
		Connector::create<VantagePro2Connector>(_ioService, _db);
	_acceptor.async_accept(
	        newConnector->socket(),
	        [this, newConnector](const boost::system::error_code& error) { runNewConnector(newConnector, error); }
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
