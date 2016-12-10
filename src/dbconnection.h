/**
 * @file dbconnection.h
 * @brief Definition of the DbConnection class
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

#ifndef DBCONNECTION_H
#define DBCONNECTION_H

#include <cassandra.h>

#include <functional>
#include <tuple>
#include <memory>
#include <mutex>

#include "message.h"

namespace meteodata {
	/**
	 * @brief A handle to the database to insert meteorological measures
	 *
	 * An instance of this class is to be used by each meteo station
	 * connector to query details about the station and insert measures in
	 * the database periodically.
	 */
	class DbConnection
	{
	public:
		/**
		 * @brief Construct a connection to the database
		 *
		 * @param user the username to use
		 * @param password the password corresponding to the username
		 */
		DbConnection(const std::string& user = "", const std::string& password = "");
		/**
		 * @brief Close the connection and destroy the database handle
		 */
		virtual ~DbConnection();
		/**
		 * @brief Get the identifier of a station given its coordinates
		 *
		 * @param latitude The latitude of the station
		 * @param longitude The longitude of the station
		 * @param altitude The elevation of the station
		 * @param station Where to store UUID corresponding to the station
		 *
		 * @return The unique identifier of the station
		 */
		bool getStationByCoords(int latitude, int longitude, int altitude, CassUuid& station);

		/**
		 * @brief Insert a new data point in the database
		 *
		 * This method is only appropriate for VantagePro2 (R) stations
		 * connectors, we might have to genericize it later.
		 *
		 * @param station The identifier of the station
		 * @param message A message from a meteo station connector
		 * containing the measurements to insert in the database
		 *
		 * @return True is the measure data point could be succesfully
		 * inserted, false otherwise
		 */
		bool insertDataPoint(const CassUuid station, const Message& message);

	private:
		/**
		 * @brief The Cassandra connection handle
		 */
		CassFuture* _futureConn;
		/**
		 * @brief The Cassandra cluster
		 */
		CassCluster* _cluster;
		/**
		 * @brief The Cassandra session data
		 */
		CassSession* _session;
		/**
		 * @brief The prepared statement for the getStationByCoords()
		 * method
		 */
		std::unique_ptr<const CassPrepared, std::function<void(const CassPrepared*)>> _selectStationByCoords;
		/**
		 * @brief The prepared statement for the insetDataPoint() method
		 */
		std::unique_ptr<const CassPrepared, std::function<void(const CassPrepared*)>> _insertDataPoint;
		/**
		 * @brief A mutual exclusion semaphore to protect _insertDataPoint
		 */
		std::mutex _insertMutex;
		/**
		 * @brief A mutual exclusion semaphore to protect _selectStationByCoords
		 */
		std::mutex _selectMutex;
		/**
		 * @brief Prepare the Cassandra query/insert statements
		 */
		void prepareStatements();
	};
}

#endif
