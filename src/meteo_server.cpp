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

#include <memory>
#include <tuple>
#include <functional>
#include <vector>
#include <filesystem>

#include <boost/asio.hpp>
#include <systemd/sd-daemon.h>
#include <unistd.h>
#include <csignal>

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
#include "mqtt/lsn50v2_thermohygrometer_mqtt_subscriber.h"
#include "mqtt/thlora_thermohygrometer_mqtt_subscriber.h"
#include "mqtt/oy1110_thermohygrometer_mqtt_subscriber.h"
#include "ship_and_buoy/ship_and_buoy_downloader.h"
#include "static/static_download_scheduler.h"
#include "synop/synop_download_scheduler.h"
#include "pessl/fieldclimate_api_download_scheduler.h"
#include "rest_web_server.h"
#include "control/control_connector.h"

namespace asio = boost::asio;
namespace ip = boost::asio::ip;
namespace local = boost::asio::local;
namespace sys = boost::system;
namespace chrono = std::chrono;

namespace {

extern "C" {

volatile sig_atomic_t signalCaught = 0;
void catchSignal(int signum) {
	if (signum == SIGINT || signum == SIGTERM) {
		signalCaught = 1;
	}
}
}

}

namespace meteodata
{

MeteoServer::MeteoServer(boost::asio::io_context& ioContext, MeteoServer::MeteoServerConfiguration&& config) :
	_ioContext{ioContext},
	_vp2DirectConnectAcceptor{ioContext},
	_db{config.address, config.user, config.password},
	_vp2DirectConnectorStopped{true},
	_controlConnectionStopped{true},
	_signalTimer{ioContext},
	_configuration{config},
	_controlAcceptor{ioContext}
{
	_configuration.password.clear();
	signal(SIGINT, catchSignal);
	signal(SIGTERM, catchSignal);
	pollSignal(sys::errc::make_error_code(sys::errc::success));

	std::cerr << SD_INFO << "[Server] management: " << "Meteodata has started succesfully" << std::endl;
}

MeteoServer::~MeteoServer()
{
	if (_vp2DirectConnectAcceptor.is_open()) {
		_vp2DirectConnectorStopped = true;
		_vp2DirectConnectAcceptor.close();
	}
	if (_controlAcceptor.is_open()) {
		_controlConnectionStopped = true;
		_controlAcceptor.close();
	}

	_lockFileDescriptor = open(SOCKET_LOCK_PATH, O_WRONLY);
	if (_lockFileDescriptor >= 0) {
		int lock = lockf(_lockFileDescriptor, F_TEST, 0);
		if (lock == 0) {
			// either we own the lock or nobody does; in either case, it's safe
			// to remove everything
			std::filesystem::remove(CONTROL_SOCKET_PATH);
			std::filesystem::remove(SOCKET_LOCK_PATH);
			close(_lockFileDescriptor);
		}
	}
}

void MeteoServer::pollSignal(const sys::error_code& e)
{
	if (e == sys::errc::operation_canceled)
		return;

	if (signalCaught) {
		std::cerr << SD_ERR << "[Server] management: " << "Signal caught, stopping" << std::endl;
		stop();
	} else {
		_signalTimer.expires_from_now(SIGNAL_POLLING_PERIOD);
		_signalTimer.async_wait([this](const sys::error_code& e) { pollSignal(e); });
	}
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
		std::map<MqttSubscriber::MqttSubscriptionDetails, std::shared_ptr<ThloraThermohygrometerMqttSubscriber>> thloraThermohygrometerMqttSubscribers;
		std::map<MqttSubscriber::MqttSubscriptionDetails, std::shared_ptr<Oy1110ThermohygrometerMqttSubscriber>> oy1110ThermohygrometerMqttSubscribers;
		std::map<MqttSubscriber::MqttSubscriptionDetails, std::shared_ptr<Lsn50v2ThermohygrometerMqttSubscriber>> lsn50v2ThermohygrometerMqttSubscribers;
		std::map<MqttSubscriber::MqttSubscriptionDetails, std::shared_ptr<LiveobjectsMqttSubscriber>> liveobjectsMqttSubscribers;
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
			} else if (topic == "fifo/liveobjects") { // TODO: will eventually contain all the following sections
				auto mqttSubscribersIt = liveobjectsMqttSubscribers.find(details);
				if (mqttSubscribersIt == liveobjectsMqttSubscribers.end()) {
					std::shared_ptr<LiveobjectsMqttSubscriber> subscriber = std::make_shared<LiveobjectsMqttSubscriber>(details, _ioContext, _db);
					mqttSubscribersIt = liveobjectsMqttSubscribers.emplace(details, subscriber).first;
				}
				auto it = std::find_if(liveobjectsStations.begin(), liveobjectsStations.end(),
									   [&uuid](auto&& objSt) { return uuid == std::get<0>(objSt); });
				if (it != liveobjectsStations.end())
					mqttSubscribersIt->second->addStation(topic, uuid, tz, std::get<1>(*it));
			} else if (topic == "fifo/Lorain") { //TODO: refactor the following six sections with an abstract factory
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
			} else if (topic == "fifo/Thlora_thermohygrometer") {
				auto mqttSubscribersIt = thloraThermohygrometerMqttSubscribers.find(details);
				if (mqttSubscribersIt == thloraThermohygrometerMqttSubscribers.end()) {
					std::shared_ptr<ThloraThermohygrometerMqttSubscriber> subscriber = std::make_shared<ThloraThermohygrometerMqttSubscriber>(details, _ioContext, _db);
					mqttSubscribersIt = thloraThermohygrometerMqttSubscribers.emplace(details, subscriber).first;
				}
				auto it = std::find_if(liveobjectsStations.begin(), liveobjectsStations.end(),
									   [&uuid](auto&& objSt) { return uuid == std::get<0>(objSt); });
				if (it != liveobjectsStations.end())
					mqttSubscribersIt->second->addStation(topic, uuid, tz, std::get<1>(*it));
			} else if (topic == "fifo/Oy1110") {
				auto mqttSubscribersIt = oy1110ThermohygrometerMqttSubscribers.find(details);
				if (mqttSubscribersIt == oy1110ThermohygrometerMqttSubscribers.end()) {
					std::shared_ptr<Oy1110ThermohygrometerMqttSubscriber> subscriber = std::make_shared<Oy1110ThermohygrometerMqttSubscriber>(details, _ioContext, _db);
					mqttSubscribersIt = oy1110ThermohygrometerMqttSubscribers.emplace(details, subscriber).first;
				}
				auto it = std::find_if(liveobjectsStations.begin(), liveobjectsStations.end(),
									   [&uuid](auto&& objSt) { return uuid == std::get<0>(objSt); });
				if (it != liveobjectsStations.end())
					mqttSubscribersIt->second->addStation(topic, uuid, tz, std::get<1>(*it));
			} else if (topic == "fifo/DraginoLSN50v2") {
				auto mqttSubscribersIt = lsn50v2ThermohygrometerMqttSubscribers.find(details);
				if (mqttSubscribersIt == lsn50v2ThermohygrometerMqttSubscribers.end()) {
					std::shared_ptr<Lsn50v2ThermohygrometerMqttSubscriber> subscriber = std::make_shared<Lsn50v2ThermohygrometerMqttSubscriber>(details, _ioContext, _db);
					mqttSubscribersIt = lsn50v2ThermohygrometerMqttSubscribers.emplace(details, subscriber).first;
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

		for (auto&& mqttSubscriber : vp2MqttSubscribers) {
			mqttSubscriber.second->start();
			_connectors.emplace("mqtt_vp2_" + mqttSubscriber.first.host, mqttSubscriber.second);
		}
		for (auto&& mqttSubscriber : objeniousMqttSubscribers) {
			mqttSubscriber.second->start();
			_connectors.emplace("mqtt_objenious_" + mqttSubscriber.first.host, mqttSubscriber.second);
		}
		for (auto&& mqttSubscriber : liveobjectsMqttSubscribers) {
			mqttSubscriber.second->start();
			_connectors.emplace("mqtt_liveobjects_" + mqttSubscriber.first.host, mqttSubscriber.second);
		}
		for (auto&& mqttSubscriber : lorainMqttSubscribers) {
			mqttSubscriber.second->start();
			_connectors.emplace("mqtt_lorain_" + mqttSubscriber.first.host, mqttSubscriber.second);
		}
		for (auto&& mqttSubscriber : baraniRainGaugeMqttSubscribers) {
			mqttSubscriber.second->start();
			_connectors.emplace("mqtt_barani_rain_" + mqttSubscriber.first.host, mqttSubscriber.second);
		}
		for (auto&& mqttSubscriber : baraniAnemometerMqttSubscribers) {
			mqttSubscriber.second->start();
			_connectors.emplace("mqtt_barani_anemo_" + mqttSubscriber.first.host, mqttSubscriber.second);
		}
		for (auto&& mqttSubscriber : thloraThermohygrometerMqttSubscribers) {
			mqttSubscriber.second->start();
			_connectors.emplace("mqtt_thlora_thermohygrometer_" + mqttSubscriber.first.host, mqttSubscriber.second);
		}
		for (auto&& mqttSubscriber : oy1110ThermohygrometerMqttSubscribers) {
			mqttSubscriber.second->start();
			_connectors.emplace("mqtt_oy1110_" + mqttSubscriber.first.host, mqttSubscriber.second);
		}
		for (auto&& mqttSubscriber : lsn50v2ThermohygrometerMqttSubscribers) {
			mqttSubscriber.second->start();
			_connectors.emplace("mqtt_lsn50v2_" + mqttSubscriber.first.host, mqttSubscriber.second);
		}
	}

	if (_configuration.startSynop) {
		// Start the Synop downloader worker (one for all the SYNOP stations in
		// the same group)
		auto synopDownloader = std::make_shared<SynopDownloadScheduler>(_ioContext, _db);
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
		auto fieldClimateScheduler = std::make_shared<FieldClimateApiDownloadScheduler>(_ioContext, _db,
				_configuration.fieldClimateApiKey, _configuration.fieldClimateApiSecret);
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
		_connectors.emplace("rest", restWebServer);
	}

	if (_configuration.startVp2) {
		_vp2DirectConnectorStopped = false;
		// Listen on the Meteodata port for incoming stations (one connector per direct-connect station)
		_vp2DirectConnectAcceptor.open(ip::tcp::v4());
		_vp2DirectConnectAcceptor.set_option(ip::tcp::acceptor::reuse_address(true));
		_vp2DirectConnectAcceptor.bind(ip::tcp::endpoint{ip::tcp::v4(), 5886});
		_vp2DirectConnectAcceptor.listen();
		startAcceptingVp2DirectConnect();
	}

	int lock = -1;
	_lockFileDescriptor = open(SOCKET_LOCK_PATH, O_WRONLY | O_CREAT, 0644);
	if (_lockFileDescriptor >= 0) {
		lock = lockf(_lockFileDescriptor, F_TLOCK, 0);
		if (lock == 0) {
			_controlConnectionStopped = false;
			// no-op if it doesn't exist
			std::filesystem::remove(CONTROL_SOCKET_PATH);

			_controlAcceptor.open();
			_controlAcceptor.bind(local::stream_protocol::endpoint{CONTROL_SOCKET_PATH});
			_controlAcceptor.listen();
			startAcceptingControlConnection();
		} else {
			std::cerr << SD_ERR << "[Server] management: " << "Couldn't get the lock at " << SOCKET_LOCK_PATH << ", is meteodata-server already starded ? Continuing anyway, without the control socket." << std::endl;
		}
	} else {
		std::cerr << SD_ERR << "[Server] management: " << "Couldn't open the lockfile at " << SOCKET_LOCK_PATH << ", is meteodata-server started with insufficient permissions ? Continuing anyway, without the control socket." << std::endl;
	}
}

void MeteoServer::stop()
{
	for (auto&& connector : _connectors) {
		auto c = connector.second.lock();
		if (c)
			c->stop();
	}
	if (_vp2DirectConnectAcceptor.is_open()) {
		_vp2DirectConnectorStopped = true;
		_vp2DirectConnectAcceptor.close();
	}

	if (_controlAcceptor.is_open()) {
		_controlConnectionStopped = true;
		_controlAcceptor.close();
	}
}

void MeteoServer::startAcceptingVp2DirectConnect()
{
	if (_vp2DirectConnectorStopped)
		return;

	auto newConnector = std::make_shared<VantagePro2Connector>(_ioContext, _db);
	_vp2DirectConnectAcceptor.async_accept(newConnector->socket(), [this, newConnector](const boost::system::error_code& error) {
		runNewVp2DirectConnector(newConnector, error);
	});
}

void MeteoServer::runNewVp2DirectConnector(const std::shared_ptr<VantagePro2Connector>& c, const boost::system::error_code& error)
{
	startAcceptingVp2DirectConnect();
	if (!error) {
		c->start();
	} else {
		std::cerr << SD_ERR << "[Direct] protocol: " << "Failed to launch a direct VP2 connector: " << error.message() << std::endl;
	}
}

void MeteoServer::startAcceptingControlConnection()
{
	if (_controlConnectionStopped)
		return;

	auto controlConnection = std::make_shared<ControlConnector>(_ioContext, *this);
	_controlAcceptor.async_accept(controlConnection->socket(), [this, controlConnection](const boost::system::error_code& error) {
		runNewControlConnector(controlConnection, error);
	});
}

void MeteoServer::runNewControlConnector(const std::shared_ptr<ControlConnector>& c, const boost::system::error_code& error)
{
	startAcceptingControlConnection();
	if (!error) {
		c->start();
	} else {
		std::cerr << SD_ERR << "[Control] protocol: " << "Failed to open a controller socket: " << error.message() << std::endl;
	}
}
}
