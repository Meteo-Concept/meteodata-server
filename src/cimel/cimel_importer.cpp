/**
 * @file cimel_importer.cpp
 * @brief Implementation of (part of) the abstract CimelImporter class
 * @author Laurent Georget
 * @date 2022-03-18
 */
/*
 * Copyright (C) 2022  JD Environnement <contact@meteo-concept.fr>
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
#include <string>
#include <utility>

#include <systemd/sd-daemon.h>
#include <cassandra.h>
#include <date.h>
#include <dbconnection_observations.h>

#include "time_offseter.h"
#include "async_job_publisher.h"
#include "cassandra_utils.h"
#include "cimel/cimel_importer.h"

namespace meteodata
{

namespace sys = boost::system;
namespace chrono = std::chrono;

using namespace std::placeholders;
using namespace meteodata;

CimelImporter::CimelImporter(const CassUuid& station, std::string cimelId, const std::string& timezone,
							 DbConnectionObservations& db, AsyncJobPublisher* jobPublisher) :
		_station{station},
		_cimelId{std::move(cimelId)},
		_db{db},
		_jobPublisher{jobPublisher},
		_tz{TimeOffseter::getTimeOffseterFor(timezone)}
{
}

CimelImporter::CimelImporter(const CassUuid& station, std::string cimelId, TimeOffseter&& timeOffseter,
							 DbConnectionObservations& db, AsyncJobPublisher* jobPublisher) :
		_station{station},
		_cimelId{std::move(cimelId)},
		_db{db},
		_jobPublisher{jobPublisher},
		_tz{timeOffseter}
{
}

bool CimelImporter::import(std::istream& input, date::sys_seconds& start, date::sys_seconds& end, date::year year,
						   bool updateLastArchiveDownloadTime)
{
	bool importSucceeded = doImport(input, start, end, year);
	if (importSucceeded && updateLastArchiveDownloadTime) {
		bool ret = _db.updateLastArchiveDownloadTime(_station, chrono::system_clock::to_time_t(end));
		if (!ret) {
			std::cerr << SD_ERR << "[Cimel " << _station << "]"
					  << " management: failed to update the last archive download datetime" << std::endl;
		}

		if (_jobPublisher) {
			_jobPublisher->publishJobsForPastDataInsertion(_station, start, end);
		}
	}
	return true;
}



}
