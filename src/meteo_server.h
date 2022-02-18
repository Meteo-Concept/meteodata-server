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
#include <dbconnection_observations.h>

#include "meteo_server.h"
#include "connector.h"

namespace meteodata
{
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
	MeteoServer(boost::asio::io_service& io, MeteoServerConfiguration&& config);
	/**
	 * @brief Launch all operations: start the SYNOP messages
	 * downloader, the Weatherlink archive downloader and listen
	 * for incoming stations.
	 */
	void start();

private:
	boost::asio::io_service& _ioService;
	/**
	 * @brief The Boost::Asio object that handles the accept()
	 * operations
	 */
	boost::asio::ip::tcp::acceptor _acceptor;
	/**
	 * @brief The connection to the database
	 */
	DbConnectionObservations _db;

	MeteoServerConfiguration _configuration;

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
	void startAccepting();
	/**
	 * @brief Dispatch a new connector and resume listening on the
	 * port
	 *
	 * @param c the connector to start
	 * @param error an error code returned from the accept()
	 * operation
	 */
	void runNewConnector(Connector::ptr c, const boost::system::error_code& error);
};
}

#endif /* ifndef METEO_SERVER_H */
