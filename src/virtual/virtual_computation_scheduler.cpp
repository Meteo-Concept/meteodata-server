/**
 * @file virtual_computation_scheduler.cpp
 * @brief Implementation of the VirtualComputationScheduler class
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

#include <iostream>
#include <chrono>
#include <thread>

#include <systemd/sd-daemon.h>
#include <boost/system/error_code.hpp>
#include <boost/asio/basic_waitable_timer.hpp>
#include <cassandra.h>
#include <dbconnection_observations.h>

#include "virtual/virtual_computation_scheduler.h"
#include "virtual/virtual_obs_computer.h"
#include "abstract_download_scheduler.h"

namespace meteodata
{
using namespace date;

VirtualComputationScheduler::VirtualComputationScheduler(asio::io_context& ioContext,
	DbConnectionObservations& db, AsyncJobPublisher* jobPublisher) :
		AbstractDownloadScheduler{chrono::minutes{POLLING_PERIOD}, ioContext, db},
		_jobPublisher{jobPublisher}
{
}

void VirtualComputationScheduler::add(const VirtualStation& station)
{
	_downloaders.emplace_back(std::make_shared<VirtualObsComputer>(station, _db, _jobPublisher));
}

void VirtualComputationScheduler::download()
{
	for (const auto & _downloader : _downloaders) {
		if (_mustStop)
			break;
		try {
			_downloader->compute();
		} catch (const std::runtime_error& e) {
			std::cerr << SD_ERR << "[Virtual] protocol: " << "Runtime error, impossible to compute " << e.what()
				  << ", moving on..." << std::endl;
		}
	}
}

void VirtualComputationScheduler::reloadStations()
{
	_downloaders.clear();

	std::vector<VirtualStation> virtualStations;
	_db.getAllVirtualStations(virtualStations);
	for (auto&& station : virtualStations) {
		add(station);
	}
}

}
