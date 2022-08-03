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
#include "mbdata/mbdata_download_scheduler.h"
#include "mqtt/mqtt_subscriber.h"
#include "mqtt/vp2_mqtt_subscriber.h"
#include "mqtt/objenious_mqtt_subscriber.h"
#include "mqtt/lorain_mqtt_subscriber.h"
#include "mqtt/barani_rain_gauge_mqtt_subscriber.h"
#include "mqtt/barani_anemometer_mqtt_subscriber.h"
#include "ship_and_buoy/ship_and_buoy_downloader.h"
#include "static/static_download_scheduler.h"
#include "synop/synop_download_scheduler.h"
#include "pessl/fieldclimate_api_download_scheduler.h"
#include "rest_web_server.h"

using namespace boost::asio;
using namespace boost::asio::ip;

namespace meteodata
{

MeteoServer::MeteoServer(boost::asio::io_context& ioContext, MeteoServer::MeteoServerConfiguration&& config) :
		_ioContext{ioContext},
		_vp2DirectConnectAcceptor{ioContext},
		_db{config.address, config.user, config.password},
		_configuration{config},
		_controlAcceptor{ioContext}
{
	_configuration.password.clear();

	std::cerr << SD_INFO << "[Server] management: " << "Meteodata has started succesfully" << std::endl;
}

void MeteoServer::start()
{
	if (_configuration.startMqtt) {
		// Start the MQTT subscribers (one per server and station type/API)
		std::vector<std::tuple<CassUuid, std::string, int, std::string, std::unique_ptr<char[]>, size_t, std::string, int>> mqttStations;
		std::vector<std::tuple<CassUuid, std::string, std::map<std::string, std::string>>> objeniousStations;
		std::vector<std::tuple<CassUuid, std::string, std::string>> liveobjectsStations;
		std::map<MqttSubscriber::MqttSubscriptionDetails, std::shared_ptr<VP2MqttSubscriber>> vp2MqttSubscribers;
		std::map<MqttSubscriber::MqttSubscriptionDetails, std::shared_ptr<LorainMqttSubscriber>> lorainMqttSubscribers;
		std::map<MqttSubscriber::MqttSubscriptionDetails, std::shared_ptr<BaraniAnemometerMqttSubscriber>> baraniAnemometerMqttSubscribers;
		std::map<MqttSubscriber::MqttSubscriptionDetails, std::shared_ptr<BaraniRainGaugeMqttSubscriber>> baraniRainGaugeMqttSubscribers;
		std::map<MqttSubscriber::MqttSubscriptionDetails, std::shared_ptr<ObjeniousMqttSubscriber>> objeniousMqttSubscribers;
		_db.getMqttStations(mqttStations);
		_db.getAllObjeniousApiStations(objeniousStations);
		_db.getAllLiveobjectsStations(liveobjectsStations);
		for (auto&& station : mqttStations) {
			MqttSubscriber::MqttSubscriptionDetails details{std::get<1>(station), std::get<2>(station),
				std::get<3>(station), std::string(std::get<4>(station).get(), std::get<5>(station))};

			const CassUuid& uuid = std::get<0>(station);
			const std::string& topic = std::get<6>(station);
			TimeOffseter::PredefinedTimezone tz{std::get<7>(station)};
			if (topic.substr(0, 4) == "vp2/") {
				auto mqttSubscribersIt = vp2MqttSubscribers.find(details);
				if (mqttSubscribersIt == vp2MqttSubscribers.end()) {
					std::shared_ptr<VP2MqttSubscriber> subscriber = std::make_shared<VP2MqttSubscriber>(details, _ioContext, _db);
					mqttSubscribersIt = vp2MqttSubscribers.emplace(details, subscriber).first;
				}
				mqttSubscribersIt->second->addStation(topic, uuid, tz);
			} else if (topic.substr(0, 10) == "objenious/") {
				auto mqttSubscribersIt = objeniousMqttSubscribers.find(details);
				if (mqttSubscribersIt == objeniousMqttSubscribers.end()) {
					std::shared_ptr<ObjeniousMqttSubscriber> subscriber = std::make_shared<ObjeniousMqttSubscriber>(
							details, _ioContext, _db);
					mqttSubscribersIt = objeniousMqttSubscribers.emplace(details, subscriber).first;
				}

				auto it = std::find_if(objeniousStations.begin(), objeniousStations.end(),
									   [&uuid](auto&& objSt) { return uuid == std::get<0>(objSt); });
				if (it != objeniousStations.end()) {
					mqttSubscribersIt->second->addStation(topic, uuid, tz, std::get<1>(*it), std::get<2>(*it));
				}
			} else if (topic == "fifo/Lorain") {
				auto mqttSubscribersIt = lorainMqttSubscribers.find(details);
				if (mqttSubscribersIt == lorainMqttSubscribers.end()) {
					std::shared_ptr<LorainMqttSubscriber> subscriber = std::make_shared<LorainMqttSubscriber>(details, _ioContext, _db);
					mqttSubscribersIt = lorainMqttSubscribers.emplace(details, subscriber).first;
				}
				auto it = std::find_if(liveobjectsStations.begin(), liveobjectsStations.end(),
									   [&uuid](auto&& objSt) { return uuid == std::get<0>(objSt); });
				if (it != liveobjectsStations.end())
					mqttSubscribersIt->second->addStation(topic, uuid, tz, std::get<1>(*it));
			} else if (topic == "fifo/Barani_rain") {
				auto mqttSubscribersIt = baraniRainGaugeMqttSubscribers.find(details);
				if (mqttSubscribersIt == baraniRainGaugeMqttSubscribers.end()) {
					std::shared_ptr<BaraniRainGaugeMqttSubscriber> subscriber = std::make_shared<BaraniRainGaugeMqttSubscriber>(details, _ioContext, _db);
					mqttSubscribersIt = baraniRainGaugeMqttSubscribers.emplace(details, subscriber).first;
				}
				auto it = std::find_if(liveobjectsStations.begin(), liveobjectsStations.end(),
									   [&uuid](auto&& objSt) { return uuid == std::get<0>(objSt); });
				if (it != liveobjectsStations.end())
					mqttSubscribersIt->second->addStation(topic, uuid, tz, std::get<1>(*it));
			} else if (topic == "fifo/Barani_anemo") {
				auto mqttSubscribersIt = baraniAnemometerMqttSubscribers.find(details);
				if (mqttSubscribersIt == baraniAnemometerMqttSubscribers.end()) {
					std::shared_ptr<BaraniAnemometerMqttSubscriber> subscriber = std::make_shared<BaraniAnemometerMqttSubscriber>(details, _ioContext, _db);
					mqttSubscribersIt = baraniAnemometerMqttSubscribers.emplace(details, subscriber).first;
				}
				auto it = std::find_if(liveobjectsStations.begin(), liveobjectsStations.end(),
									   [&uuid](auto&& objSt) { return uuid == std::get<0>(objSt); });
				if (it != liveobjectsStations.end())
					mqttSubscribersIt->second->addStation(topic, uuid, tz, std::get<1>(*it));
			} else {
				std::cerr << SD_ERR << "[MQTT " << std::get<0>(station) << "] protocol: " << "Unrecognized topic "
						  << topic << " for MQTT station " << std::get<0>(station) << std::endl;
			}
		}

		for (auto&& mqttSubscriber : vp2MqttSubscribers)
			mqttSubscriber.second->start();
		for (auto&& mqttSubscriber : objeniousMqttSubscribers)
			mqttSubscriber.second->start();
		for (auto&& mqttSubscriber : lorainMqttSubscribers)
			mqttSubscriber.second->start();
		for (auto&& mqttSubscriber : baraniRainGaugeMqttSubscribers)
			mqttSubscriber.second->start();
		for (auto&& mqttSubscriber : baraniAnemometerMqttSubscribers)
			mqttSubscriber.second->start();
	}

	if (_configuration.startSynop) {
		// Start the Synop downloader worker (one for all the SYNOP stations in
		// the same group)
		auto synopDownloader = std::make_shared<SynopDownloadScheduler>(_ioContext, _db);
		synopDownloader->add(SynopDownloadScheduler::GROUP_FR, chrono::minutes(20), chrono::hours(3));
		synopDownloader->add(SynopDownloadScheduler::GROUP_LU, chrono::minutes(20), chrono::hours(3));

		// Add the deferred SYNOPs
		std::vector<std::tuple<CassUuid, std::string>> deferredSynops;
		_db.getDeferredSynops(deferredSynops);
		for (auto&& synop : deferredSynops) {
			synopDownloader->add(std::get<1>(synop), chrono::minutes(6 * 60), chrono::hours(24));
		}

		synopDownloader->start();
		_connectors.emplace("synop", synopDownloader);
	}

	if (_configuration.startShip) {
		// Start the Meteo France SHIP and BUOY downloader (one for all SHIP and BUOY messages)
		auto meteofranceDownloader = std::make_shared<ShipAndBuoyDownloader>(_ioContext, _db);
		meteofranceDownloader->start();
		_connectors.emplace("ship", meteofranceDownloader);
	}

	if (_configuration.startStatic) {
		auto statICDownloadScheduler = std::make_shared<StatICDownloadScheduler>(_ioContext, _db);
		statICDownloadScheduler->start();
		_connectors.emplace("static", statICDownloadScheduler);
	}

	if (_configuration.startWeatherlink) {
		// Start the Weatherlink download scheduler (one for all Weatherlink stations, one downloader per station but they
		// share a single HTTP client)
		auto weatherlinkScheduler = std::make_shared<WeatherlinkDownloadScheduler>(_ioContext, _db, std::move(
				_configuration.weatherlinkApiV2Key), std::move(_configuration.weatherlinkApiV2Secret));
		weatherlinkScheduler->start();
		_connectors.emplace("weatherlink", weatherlinkScheduler);
	}

	if (_configuration.startFieldclimate) {
		// Start the FieldClimate download scheduler (one for all Pessl stations, one downloader per station but they
		// share a single HTTP client)
		auto fieldClimateScheduler = std::make_shared<FieldClimateApiDownloadScheduler>(_ioContext, _db, std::move(
				_configuration.fieldClimateApiKey), std::move(_configuration.fieldClimateApiSecret));
		fieldClimateScheduler->start();
		_connectors.emplace("fieldclimate", fieldClimateScheduler);
	}

	if (_configuration.startMbdata) {
		auto mbdataDownloadScheduler = std::make_shared<MBDataDownloadScheduler>(_ioContext, _db);
		mbdataDownloadScheduler->start();
		_connectors.emplace("mbdata", mbdataDownloadScheduler);
	}

	if (_configuration.startRest) {
		// Start the Web server for the REST API
		auto restWebServer = std::make_shared<RestWebServer>(_ioContext, _db);
		restWebServer->start();
	}

	if (_configuration.startVp2) {
		// Listen on the Meteodata port for incoming stations (one connector per direct-connect station)
		_vp2DirectConnectAcceptor.open(tcp::v4());
		_vp2DirectConnectAcceptor.set_option(tcp::acceptor::reuse_address(true));
		_vp2DirectConnectAcceptor.bind(tcp::endpoint{tcp::v4(), 5886});
		_vp2DirectConnectAcceptor.listen();
		startAcceptingVp2DirectConnect();
	}
}

void MeteoServer::startAcceptingVp2DirectConnect()
{
	auto newConnector = std::make_shared<VantagePro2Connector>(_ioContext, _db);
	_vp2DirectConnectAcceptor.async_accept(newConnector->socket(), [this, newConnector](const boost::system::error_code& error) {
		runNewVp2DirectConnector(newConnector, error);
	});
}

void MeteoServer::runNewVp2DirectConnector(const std::shared_ptr<VantagePro2Connector>& c, const boost::system::error_code& error)
{
	if (!error) {
		startAcceptingVp2DirectConnect();
		c->start();
	}
}
}
