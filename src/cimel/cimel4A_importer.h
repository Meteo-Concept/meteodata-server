/**
 * @file cimel4A_importer.h
 * @brief Definition of the Cimel4AImporter class
 * @author Laurent Georget
 * @date 2021-12-21
 */
/*
 * Copyright (C) 2021  JD Environnement <contact@meteo-concept.fr>
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

#ifndef CIMEL4A_IMPORTER_H
#define CIMEL4A_IMPORTER_H

#include <iostream>
#include <vector>
#include <functional>

#include <systemd/sd-daemon.h>
#include <boost/system/error_code.hpp>
#include <boost/asio.hpp>
#include <boost/asio/basic_waitable_timer.hpp>
#include <cassandra.h>
#include <date/date.h>
#include <date/tz.h>
#include <dbconnection_observations.h>
#include <message.h>

#include "../time_offseter.h"

namespace meteodata
{

namespace asio = boost::asio;
namespace sys = boost::system;
namespace chrono = std::chrono;

using namespace std::placeholders;

/**
 * A Cimel4AImporter instance is able to parse weather data file exported
 * by the CIMEL software for a type 4A station
 */
class Cimel4AImporter
{
public:
	/**
	 * Constructs a Cimel4AImporter
	 * @param station The station identifier in Meteodata
	 * @param cimelId The CIMEL identifier of the station (to check for mixed up files), the CIMEL id is defined as
	 * the INSEE code of the city followed by the station number (it's also the prefix of export filenames)
	 * @param timezone The timezone of the station (all stations should be in UTC but with old stations, it's difficult
	 * to be sure of the configuration)
	 * @param db The database connection to insert data
	 */
	Cimel4AImporter(const CassUuid& station, const std::string& cimelId, const std::string& timezone,
					DbConnectionObservations& db);

	/**
	 * Constructs a Cimel4AImporter
	 * @param station The station identifier in Meteodata
	 * @param cimelId The CIMEL identifier of the station (to check for mixed up files), the CIMEL id is defined as
	 * the INSEE code of the city followed by the station number (it's also the prefix of export filenames)
	 * @param timeOffseter The timeOffseter this instance should use
	 * @param db The database connection to insert data
	 */
	Cimel4AImporter(const CassUuid& station, const std::string& cimelId, TimeOffseter&& timeOffseter,
					DbConnectionObservations& db);

	bool import(std::istream& input, date::sys_seconds& start, date::sys_seconds& end, date::year year,
				bool updateLastArchiveDownloadTime = true);

	template<typename T>
	struct Parser
	{
		int length = 2;
		int base = 10;

		T& destination;
	};

	template<typename T>
	static Parser<T> parse(T& destination, int length, int base)
	{
		return Parser<T>{length, base, destination};
	}

	struct Ignorer
	{
		std::streamsize length;
	};

	static Ignorer ignore(std::streamsize length)
	{
		return Ignorer{length};
	}

private:
	CassUuid _station;
	std::string _cimelId;
	DbConnectionObservations& _db;
	TimeOffseter _tz;
};

template<typename T>
std::istream& operator>>(std::istream& is, Cimel4AImporter::Parser<T>&& p)
{
	p.destination = 0;
	for (int i = p.length ; i > 0 ; i--) {
		auto c = is.get();
		if (std::isspace(c)) {
			i++; // do as if the blank character was not there at all
			continue;
		}
		if (c >= '0' && c <= '9') {
			p.destination = p.destination * p.base + c - '0';
		} else if (c >= 'A' && c <= 'F') {
			p.destination = p.destination * p.base + c - 'A' + 10;
		}
	}

	return is;
}

std::istream& operator>>(std::istream& is, const Cimel4AImporter::Ignorer& ignorer);

}

#endif