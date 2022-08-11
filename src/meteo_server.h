/**
 * @file meteo_server.h
 * @brief Definition of the MeteoServer class
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

#ifndef METEO_SERVER_H
#define METEO_SERVER_H

#include <boost/asio.hpp>
#include <boost/system/error_code.hpp>
#include <dbconnection_observations.h>

#include "meteo_server.h"
#include "connector.h"
#include "davis/vantagepro2_connector.h"
#include "control/control_connector.h"
#include "config.h"

namespace meteodata
{

class ConnectorIterator {
private:
	using Container = std::map<std::string, std::weak_ptr<Connector>>;
	const Container* _container = nullptr;
	Container::const_iterator _it;
	std::shared_ptr<Connector> _owned;

public:
	using difference_type = Container::const_iterator::difference_type;
	using value_type = std::tuple<std::string, std::shared_ptr<Connector>>;
	using iterator_category = std::forward_iterator_tag;
	using pointer = value_type*;
	using reference = value_type&;

	ConnectorIterator(const Container* container) {
		_container = container;
		_it = _container->cbegin();
		if (_it != _container->cend()) {
			_owned = _it->second.lock();
			if (!_owned)
				operator++();
		}
	}

	ConnectorIterator() = default;
	ConnectorIterator(const ConnectorIterator& other) {
		_container = other._container;
		_it = other._it;
	}
	ConnectorIterator& operator++() {
		if (_owned)
			_owned.reset();
		do {
			++_it;
		} while ((_it != _container->cend()) && !(_owned = _it->second.lock()));
		return *this;
	}

	ConnectorIterator operator++(int) {
		ConnectorIterator ret = *this;
		operator++();
		return ret;
	}

	value_type operator*() {
		if (!_owned) {
			_owned = _it->second.lock();
		}
		return {_it->first, _owned};
	}

	bool operator!=(const ConnectorIterator& other) const {
		return (_container == other._container && _it != other._it) ||
				(other._container == nullptr && _it != _container->cend());
	}
};

/**
 * @brief Main class and orchestrator
 *
 * One instance of this class is launched when the program starts and
 * runs indefinitely. The MeteoServer listens on port 5886 (for now)
 * and starts a meteo station connector on demand, each time a station
 * opens a connection.
 */
class MeteoServer
{

public:
	struct MeteoServerConfiguration
	{
		std::string address;
		std::string user;
		std::string password;
		std::string weatherlinkApiV2Key;
		std::string weatherlinkApiV2Secret;
		std::string fieldClimateApiKey;
		std::string fieldClimateApiSecret;
		std::string objeniousApiKey;
		bool startMqtt = true;
		bool startSynop = true;
		bool startShip = true;
		bool startStatic = true;
		bool startWeatherlink = true;
		bool startFieldclimate = true;
		bool startMbdata = true;
		bool startRest = true;
		bool startVp2 = true;
	};

	/**
	 * @brief Construct the MeteoServer
	 *
	 * @param io the Boost::Asio service that will handle all
	 * network operations done by the MeteoServer
	 * @param config the credentials and other configuration to use
	 */
	MeteoServer(boost::asio::io_context& io, MeteoServerConfiguration&& config);

	~MeteoServer();

	/**
	 * @brief Launch all operations: start the SYNOP messages
	 * downloader, the Weatherlink archive downloader and listen
	 * for incoming stations.
	 */
	void start();

	void stop();

private:
	boost::asio::io_context& _ioContext;
	/**
	 * @brief The Boost::Asio object that handles the accept()
	 * operations
	 */
	boost::asio::ip::tcp::acceptor _vp2DirectConnectAcceptor;
	/**
	 * @brief The connection to the database
	 */
	DbConnectionObservations _db;

	MeteoServerConfiguration _configuration;

	bool _vp2DirectConnectorStopped;

	bool _controlConnectionStopped;

	/**
	 * @brief The Boost::Asio object representing the endpoint receiving commands
	 * from the meteodatactl program
	 */
	boost::asio::local::stream_protocol::acceptor _controlAcceptor;

	int _lockFileDescriptor = -1;

	asio::basic_waitable_timer<chrono::steady_clock> _signalTimer;

	void pollSignal(const boost::system::error_code& e);

	constexpr static std::chrono::seconds SIGNAL_POLLING_PERIOD{3};

	/**
	 * @brief Start listening on the port, construct a connector,
	 * and wait for a station to present itself
	 *
	 * In the current version, the connectors each own their socket,
	 * therefore, the MeteoServer must build the connector first,
	 * and then do an accept() on the connector's socket.
	 *
	 * @todo In a future version, if we want to support various
	 * types of meteo stations, we will need either:
	 * - to open one port per type of station,
	 * - or to start a temporary connector, find a way to
	 * discover the type of station, start the real connector and
	 * pass it over the socket.
	 */
	void startAcceptingVp2DirectConnect();

	/**
	 * @brief Dispatch a new VP2 connector and resume listening on the port
	 *
	 * @param c the connector to start
	 * @param error an error code returned from the accept()
	 * operation
	 */
	void runNewVp2DirectConnector(const std::shared_ptr<VantagePro2Connector>& c, const boost::system::error_code& error);

	/**
	 * @brief Start listening on a UNIX socket to receive control queries (such
	 * as reloading a connector, asking for the status of a station, etc.)
	 */
	void startAcceptingControlConnection();

	/**
	 * @brief Dispatch a new control connector and resume listening on the socket
	 *
	 * @param error an error code returned from the accept()
	 * operation
	 */
	void runNewControlConnector(const std::shared_ptr<ControlConnector>& c, const boost::system::error_code& error);

	std::map<std::string, std::weak_ptr<Connector>> _connectors;

public:
	ConnectorIterator beginConnectors() { return ConnectorIterator(&_connectors); }
	ConnectorIterator endConnectors() { return ConnectorIterator(); };

	friend class ConnectorIterator;
};

}

#endif /* METEO_SERVER_H */
