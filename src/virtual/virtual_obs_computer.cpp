/**
 * @file virtual_obs_computer.cpp
 * @brief Implementation of the VirtualObsComputer class
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

#include <chrono>
#include <iostream>
#include <string>
#include <map>
#include <optional>
#include <utility>

#include <boost/asio.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <cassobs/dbconnection_observations.h>
#include <date.h>
#include <systemd/sd-daemon.h>
#include <cassandra.h>

#include "virtual/virtual_obs_computer.h"
#include "http_utils.h"
#include "cassandra_utils.h"
#include "async_job_publisher.h"


namespace meteodata
{
namespace chrono = std::chrono;

using namespace meteodata;

VirtualObsComputer::VirtualObsComputer(const VirtualStation& station,
		DbConnectionObservations& db, AsyncJobPublisher* jobPublisher) :
		_station{station},
		_db{db},
		_jobPublisher{jobPublisher}
{
	time_t lastArchiveDownloadTime;
	int period;
	db.getStationDetails(station.station, _stationName, period, lastArchiveDownloadTime);
	float latitude, longitude;
	int elevation;
	db.getStationLocation(station.station, latitude, longitude, elevation);
	_lastArchive = date::sys_seconds(chrono::seconds(lastArchiveDownloadTime));
	std::cout << SD_DEBUG << "[Virtual " << _station.station << "] connection: " << "Discovered Virtual station "
		  << _stationName << std::endl;
}

date::sys_seconds VirtualObsComputer::getLastDatetimeAvailable()
{
	std::cout << SD_INFO << "[Virtual " << _station.station << "] management: "
			  << "Checking if new data is available for virtual station " << _stationName << std::endl;

	using namespace date;
	date::sys_seconds dateInUTC;
	auto now = chrono::system_clock::now();

	date::sys_seconds lastDateFromSource = _lastArchive;
	for (auto&& [s,vars] : _station.sources) {
		std::string name;
		time_t t;
		int p;
		_db.getStationDetails(s, name, p, t);
		date::sys_seconds lastAvailable = date::floor<chrono::seconds>(chrono::system_clock::from_time_t(t));
		std::cout << SD_DEBUG << "[Virtual " << s << "] connection: "
			<< "Source station " << name << " has data available until "
			<< date::format("%Y-%m-%dT%H:%M:%SZ", lastAvailable) << "\n"
			<< "Last archive is at "
			<< date::format("%Y-%m-%dT%H:%M:%SZ", _lastArchive) << "\n"
			<< "now - 4h is at "
			<< date::format("%Y-%m-%dT%H:%M:%SZ", now - chrono::hours{4}) << "\n";
		if (lastAvailable < _lastArchive || lastAvailable < now - chrono::hours{4}) {
			std::cout << SD_WARNING << "[Virtual " << s << "] connection: "
				<< "No data in the last 4h for source station " << name
				<< ", advancing anyway" << std::endl;
		} else if (lastDateFromSource == _lastArchive || lastAvailable < lastDateFromSource) {
			lastDateFromSource = lastAvailable;
		}
	}

	return lastDateFromSource;
}

void VirtualObsComputer::doCompute(const date::sys_seconds& begin, const date::sys_seconds& end, bool updateLastArchive)
{
	date::sys_seconds oldestArchive = begin;
	date::sys_seconds newestArchive = end;

	bool insertionOk = true;

	for (date::sys_seconds target = begin - begin.time_since_epoch() % chrono::minutes{10} ; target <= end && insertionOk ; target += chrono::minutes{_station.period}) {
		Observation final;
		final.station = _station.station;
		final.day = date::floor<date::days>(target);
		final.time = target;
		time_t targetAsTime_t = chrono::system_clock::to_time_t(target);
		for (auto&& [s,vars] : _station.sources) {
			Observation obs;
			bool ret = _db.getLastDataBefore(s, targetAsTime_t, obs);
			if (!ret || obs.time < target - chrono::minutes{10}) {
				continue;
			}
			for (auto&& v : vars) {
				if (obs.isPresent(v)) {
					if (Observation::isValidIntVariable(v)) {
						int value = obs.get<int>(v);
						final.set(v, value);
					} else if (Observation::isValidFloatVariable(v)) {
						float value = obs.get<float>(v);
						final.set(v, value);
					}
				}
			}
		}

		insertionOk = _db.insertV2DataPoint(final) && _db.insertV2DataPointInTimescaleDB(final);

		if (insertionOk) {
			if (target < oldestArchive)
				oldestArchive = target;
			if (target > newestArchive)
				newestArchive = target;
			std::cout << SD_DEBUG << "[Virtual " << _station.station << "] measurement: "
				  << "Archive data stored for virtual station " << _stationName << std::endl;
			if (updateLastArchive) {
				insertionOk = _db.updateLastArchiveDownloadTime(_station.station, chrono::system_clock::to_time_t(newestArchive));
				if (!insertionOk) {
					std::cerr << SD_ERR << "[Virtual " << _station.station << "] management: "
						  << "couldn't update last archive download time for station " << _stationName << std::endl;
				} else {
					_lastArchive = target;
				}
			}
		}
	}

	if (insertionOk && _jobPublisher) {
		_jobPublisher->publishJobsForPastDataInsertion(_station.station, oldestArchive, newestArchive);
	}
}

void VirtualObsComputer::compute(const date::sys_seconds& begin, const date::sys_seconds& end)
{
	std::cout << SD_INFO << "[Virtual " << _station.station << "] measurement: "
		  << "Computing observations for station " << _stationName << std::endl;

	doCompute(begin, end, false);
}

void VirtualObsComputer::compute()
{
	std::cout << SD_INFO << "[Virtual " << _station.station << "] measurement: "
			  << "Computing observations for station " << _stationName << std::endl;

	// may throw
	date::sys_seconds lastAvailable = getLastDatetimeAvailable();
	if (lastAvailable <= _lastArchive) {
		std::cout << SD_DEBUG << "[Virtual " << _station.station << "] management: "
			  << "No new data available for any source of virtual station " << _stationName << ", bailing off" << std::endl;
		return;
	}

	using namespace date;
	std::cout << SD_DEBUG << "[Virtual " << _station.station << "] management: " << "Last archive dates back from "
			  << _lastArchive << "; last available is " << lastAvailable << "\n" << "(approximately "
			  << date::floor<date::days>(lastAvailable - _lastArchive) << " days)" << std::endl;


	doCompute(_lastArchive, date::floor<chrono::seconds>(chrono::system_clock::now()), true);
}

}
