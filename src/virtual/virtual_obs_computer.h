/**
 * @file virtual_obs_computer.h
 * @brief Definition of the VirtualObsComputer class
 * @author Laurent Georget
 * @date 2021-02-23
 */
/*
 * Copyright (C) 2021  SAS JD Environnement <contact@meteo-concept.fr>
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

#ifndef VIRTUAL_OBS_COMPUTER_H
#define VIRTUAL_OBS_COMPUTER_H

#include <date/date.h>
#include <map>
#include <string>
#include <tuple>

#include <boost/system/error_code.hpp>
#include <cassobs/dbconnection_observations.h>
#include <cassobs/virtual_station.h>
#include <cassandra.h>

#include "async_job_publisher.h"

namespace meteodata
{

using namespace meteodata;

/**
 * @brief A virtual station, composed of various independent sensors for which
 * we synchronize and merge the measurements
 */
class VirtualObsComputer
{
public:
	/**
	 * @brief Construct the downloader
	 *
	 * @param station the virtual station
	 * @param db the observations database to insert (meta-)data into
	 * @param jobPublisher an optional component used to schedule climatology
	 * and monitoring computations
	 */
	VirtualObsComputer(const VirtualStation& station, DbConnectionObservations& db,
		AsyncJobPublisher* jobPublisher = nullptr);

	date::sys_seconds getLastDatetimeAvailable();

	/**
	 * @brief Compute new observation points for the virtual station from
	 * past source observations
	 */
	void compute();

	/**
	 * @brief Compute observation points for the virtual station from
	 * past source observations between two datetimes
	 */
	void compute(const date::sys_seconds& begin, const date::sys_seconds& end);

private:
	/**
	 * @brief The station in MétéoData
	 */
	VirtualStation _station;

	/**
	 * @brief The observations database (part Cassandra, part SQL) connector
	 */
	DbConnectionObservations& _db;

	/**
	 * @brief The human-readable name given to the station
	 */
	std::string _stationName;

	/**
	 * @brief The last datetime for which data is stored in the Météodata
	 * database
	 */
	date::sys_seconds _lastArchive;

	AsyncJobPublisher* _jobPublisher;

	/**
	 * @brief Inner function for the computation of observation points
	 */
	void doCompute(const date::sys_seconds& begin, const date::sys_seconds& end, bool updateLastArchive);
};

}

#endif
