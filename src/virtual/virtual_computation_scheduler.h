/**
 * @file virtual_computation_scheduler.h
 * @brief Definition of the VirtualComputationScheduler class
 * @author Laurent Georget
 * @date 2024-04-12
 */
/*
 * Copyright (C) 2024  SAS JD Environnement <contact@meteo-concept.fr>
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

#ifndef VIRTUAL_COMPUTATION_SCHEDULER_H
#define VIRTUAL_COMPUTATION_SCHEDULER_H

#include <memory>
#include <vector>
#include <string>
#include <chrono>
#include <map>

#include <boost/system/error_code.hpp>
#include <boost/asio/basic_waitable_timer.hpp>
#include <cassandra.h>
#include <cassobs/dbconnection_observations.h>
#include <cassobs/dto/virtual_station.h>

#include "async_job_publisher.h"
#include "abstract_download_scheduler.h"
#include "virtual/virtual_obs_computer.h"

namespace meteodata
{
/**
 * @brief The orchestrator for all virtual stations operations
 */
class VirtualComputationScheduler : public AbstractDownloadScheduler
{
public:
	/**
	 * @brief Construct the download scheduler
	 *
	 * @param ioContext the Boost object used to process asynchronous
	 * events, timers, and callbacks
	 * @param db the MétéoData observations database connector
	 * @param dbJobs the MétéoData asynchronous jobs database connector
	 */
	VirtualComputationScheduler(asio::io_context& ioContext, DbConnectionObservations& db,
		AsyncJobPublisher* jobPublisher = nullptr);

	/**
	 * @brief Add a station to download the data for
	 *
	 * @param station The station Météodata UUID
	 * @param variables All the variables registered for that station (see
	 * the VirtualComputationer class for details)
	 */
	void add(const VirtualStation& station);

private:
	/**
	 * @brief The list of all computer (one per station)
	 */
	std::vector<std::shared_ptr<VirtualObsComputer>> _downloaders;

	/**
	 * @brief The component used to schedule climatology recomputations
	 */
	AsyncJobPublisher* _jobPublisher{};

private:
	/**
	 * @brief Reload the list of virtual stations from the database and
	 * recreate all virtual observations computers
	 */
	void reloadStations() override;

	/**
	 * @brief "Download", i.e. fetch source data to compute virtual
	 * observations, for all stations
	 *
	 * Archive data are computed since the last timestamp the data is
	 * previously available for the station.
	 */
	void download() override;

	/**
	 * @brief The fixed polling period in minutes
	 */
	static constexpr int POLLING_PERIOD = 5;
};

}

#endif
