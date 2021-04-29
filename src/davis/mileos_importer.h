/**
 * @file mileos_importer.h
 * @brief Definition of the MileosImporter class
 * @author Laurent Georget
 * @date 2021-04-29
 */
/*
 * Copyright (C) 2020  JD Environnement <contact@meteo-concept.fr>
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

#ifndef MILEOS_IMPORTER_H
#define MILEOS_IMPORTER_H

#include <iostream>
#include <vector>

#include <boost/system/error_code.hpp>
#include <boost/asio.hpp>
#include <boost/asio/basic_waitable_timer.hpp>
#include <cassandra.h>
#include <date/date.h>
#include <date/tz.h>
#include <dbconnection_observations.h>

#include "../time_offseter.h"

namespace meteodata {

namespace asio = boost::asio;
namespace sys = boost::system; //system() is a function, it cannot be redefined
			       //as a namespace
namespace chrono = std::chrono;

using namespace std::placeholders;
using namespace meteodata;

/**
 * A MileosImporter instance is able to parse a file with extension .mileos produced
 * by the Weatherlink software to ingest meteorological data collected by
 * Davis® station
 */
class MileosImporter
{
public:
	MileosImporter(const CassUuid& station, const std::string& timezone, DbConnectionObservations& db);
	bool import(std::istream& input, date::sys_seconds& start, date::sys_seconds& end, bool updateLastArchiveDownloadTime = false);

private:
	CassUuid _station;
	DbConnectionObservations& _db;
	TimeOffseter _tz;
	std::vector<std::string> _fields;
};

}

#endif
