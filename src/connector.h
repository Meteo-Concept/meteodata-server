/**
 * @file connector.h
 * @brief Definition of the Connector class
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
#ifndef CONNECTOR_H
#define CONNECTOR_H

#include <iostream>
#include <memory>

#include <boost/asio.hpp>
#include <dbconnection_observations.h>

namespace meteodata
{
	/**
	 * @brief Parent class of all meteo stations connectors
	 */
	class Connector : public std::enable_shared_from_this<Connector>
	{
	public:
		/**
		 * @brief Connector::ptr is the type to use to manipulate a
		 * generic connector
		 */
		typedef std::shared_ptr<Connector> ptr;

		/**
		 * @brief Instantiate a new connector for a given meteo station
		 * type
		 *
		 * Meteo stations connectors should never be instantiated
		 * directly: use this method instead. This lets the connector
		 * be deallocated automatically once it is no longer used.
		 *
		 * @tparam T The actual meteo station connector type, e.g.
		 * VantagePro2Connector for a VantagePro2 (R) station
		 * @param ioService The Boost::Asio asynchronous service that
		 * the connector will have to use for all Boost:Asio operations
		 * @param db The handle to the database
		 *
		 * @return An auto-managed shared pointer to the connector
		 */
		template<typename T>
		static
		typename std::enable_if<std::is_base_of<Connector,T>::value,
				Connector::ptr>::type
		create(boost::asio::io_service& ioService, DbConnectionObservations& db)
		{
			return Connector::ptr(new T(std::ref(ioService), std::ref(db)));
		}

		/**
		 * @brief Destroy the connector
		 */
		virtual ~Connector() = default;
		/**
		 * @brief Connect to the database and start polling the meteo
		 * station periodically for data
		 *
		 * This method should basically be an infinite loop and return
		 * when the connection to the meteo station is lost.
		 */
		virtual void start() = 0;
		/**
		 * @brief Give the TCP socket allocated for the connector to
		 * communicate with the meteo station
		 *
		 * The connector should terminate all operations when it detects
		 * a connectivity loss.
		 *
		 * @return The socket used to communicate with the meteo station
		 */
		boost::asio::ip::tcp::socket& socket() { return _sock; }

	protected:
		/**
		 * @brief Construct a connector
		 *
		 * This method is only callable by actual child classes
		 * connectors.
		 * @param ioService The Boost::Asio service to use for all
		 * Boost::Asio asynchronous operations
		 * @param db The handle to the database
		 */
		Connector(boost::asio::io_service& ioService, DbConnectionObservations& db);
		/**
		 * @brief The TCP socket used to communicate to the meteo
		 * station
		 */
		boost::asio::ip::tcp::socket _sock;
		/**
		 * @brief The Boost::Asio service to use for asynchronous
		 * operations
		 */
		boost::asio::io_service& _ioService;
		/**
		 * @brief The connection to the database
		 */
		DbConnectionObservations& _db;
	};
}

#endif // CONNECTOR_H
