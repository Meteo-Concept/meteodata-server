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

	virtual void stop() = 0;

	virtual void reload() = 0;

	virtual std::string getStatus();

protected:
	/**
	 * @brief Construct a connector
	 *
	 * This method is only callable by actual child classes
	 * connectors.
	 * @param ioContext The Boost::Asio service to use for all
	 * Boost::Asio asynchronous operations
	 * @param db The handle to the database
	 */
	Connector(boost::asio::io_context& ioContext, DbConnectionObservations& db);

	/**
	 * @brief The Boost::Asio service to use for asynchronous
	 * operations
	 */
	boost::asio::io_context& _ioContext;
	/**
	 * @brief The connection to the database
	 */
	DbConnectionObservations& _db;

	struct Status {
		date::sys_seconds activeSince;
		date::sys_seconds lastReloaded;
		date::sys_seconds lastDownload;
		unsigned int nbDownloads = 0;
		std::string shortStatus;
		date::sys_seconds nextDownload;
	};

	Status _status;
};
}

#endif // CONNECTOR_H
