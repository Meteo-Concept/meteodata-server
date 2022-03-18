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

#include <systemd/sd-daemon.h>
#include <cassandra.h>
#include <date/date.h>
#include <dbconnection_observations.h>

#include "../time_offseter.h"
#include "../cassandra.h"
#include "../cassandra_utils.h"
#include "./cimel_importer.h"

namespace meteodata
{

namespace sys = boost::system;
namespace chrono = std::chrono;

using namespace std::placeholders;
using namespace meteodata;

CimelImporter::CimelImporter(const CassUuid& station, const std::string& cimelId, const std::string& timezone,
								 DbConnectionObservations& db) :
		_station{station},
		_cimelId{cimelId},
		_db{db},
		_tz{TimeOffseter::getTimeOffseterFor(timezone)}
{
}

CimelImporter::CimelImporter(const CassUuid& station, const std::string& cimelId, TimeOffseter&& timeOffseter,
								 DbConnectionObservations& db) :
		_station{station},
		_cimelId{cimelId},
		_db{db},
		_tz{timeOffseter}
{
}

std::istream& operator>>(std::istream& is, const CimelImporter::Ignorer& ignorer)
{
	std::streamsize i = ignorer.length;
	while (i > 0) {
		auto c = is.get();
		if (!std::isspace(c)) {
			i--; // do not count blank characters
		}
	}
	return is;
}

}
