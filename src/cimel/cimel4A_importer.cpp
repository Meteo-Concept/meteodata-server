/**
 * @file cimel4A_importer.cpp
 * @brief Implementation of the Cimel4AImporter class
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

#include <iostream>
#include <string>

#include <systemd/sd-daemon.h>
#include <cassandra.h>
#include <date/date.h>
#include <dbconnection_observations.h>

#include "../time_offseter.h"
#include "../cassandra.h"
#include "../cassandra_utils.h"
#include "./cimel4A_importer.h"

namespace meteodata
{

namespace sys = boost::system;
namespace chrono = std::chrono;

using namespace std::placeholders;
using namespace meteodata;

Cimel4AImporter::Cimel4AImporter(const CassUuid& station, const std::string& cimelId, const std::string& timezone,
								 DbConnectionObservations& db) :
		_station{station},
		_cimelId{cimelId},
		_db{db},
		_tz{TimeOffseter::getTimeOffseterFor(timezone)}
{
}

Cimel4AImporter::Cimel4AImporter(const CassUuid& station, const std::string& cimelId, TimeOffseter&& timeOffseter,
								 DbConnectionObservations& db) :
		_station{station},
		_cimelId{cimelId},
		_db{db},
		_tz{timeOffseter}
{
}

bool Cimel4AImporter::import(std::istream& input, date::sys_seconds& start, date::sys_seconds& end, date::year year,
							 bool updateLastArchiveDownloadTime)
{
	// Parse header "S0\s+"
	char header[4];
	input >> header[0] >> header[1] >> std::ws >> header[2] >> header[3];
	if (header[0] != 'S' || header[1] != '0') {
		std::cerr << SD_ERR << "[Cimel4AImporter] protocol: wrong header \"" << header[0] << header[1]
				  << "\" (expected \"S0\")" << std::endl;
		return false;
	}

	if (header[2] != '4' || header[3] != 'A') {
		std::cerr << SD_ERR << "[Cimel4AImporter] protocol: wrong station type \"" << header[2] << header[3]
				  << "\" (expected \"4A\")" << std::endl;
		return false;
	}

	int dept, city, nb;
	input >> parse(dept, 2, 16) >> parse(city, 4, 16) >> ignore(1) >> parse(nb, 1, 16);

	std::string detectedId = std::to_string(dept * 10000 + city * 10 + nb);
	if (detectedId != _cimelId) {
		std::cerr << SD_ERR << "[Cimel4AImporter] protocol: wrong station id \"" << detectedId << "\" (expected \""
				  << _cimelId << "\")" << std::endl;
		return false;
	}

	auto now = chrono::system_clock::now();
	start = date::floor<chrono::seconds>(now);
	end = date::floor<chrono::seconds>(chrono::system_clock::from_time_t(0));

	for (;;) {
		Observation o;

		// Ignore until next paragraph
		input.ignore(std::numeric_limits<std::streamsize>::max(), 'S');
		input.unget();
		input >> header[0] >> header[1] >> std::ws;
		if (header[0] != 'S' || header[1] != '0') {
			std::cerr << SD_ERR << "[Cimel4AImporter] protocol: wrong daily value header \"" << header[0] << header[1]
					  << "\" (expected \"S0\")" << std::endl;
			return false;
		}
		if (!input)
			break;

		unsigned int day, month;
		int tn, tx, rainfall;
		input >> parse(day, 2, 10) >> parse(month, 2, 10) >> parse(tn, 4, 16) >> parse(tx, 4, 16) >> ignore(24)
			  >> parse(rainfall, 4, 16) >> ignore(88);

		date::year_month_day ymd{year, date::month{month}, date::day{day}};
		date::sys_days date{ymd};
		auto timestamp = chrono::system_clock::to_time_t(date);

		if (tn != 0xFFFF)
			_db.insertV2Tn(_station, timestamp, static_cast<float>(tn - 400) / 10.f);
		if (tx != 0xFFFF)
			_db.insertV2Tx(_station, timestamp, static_cast<float>(tx - 400) / 10.f);

		if (rainfall != 0xFFFF)
			_db.insertV2EntireDayValues(_station, timestamp, {true, static_cast<float>(rainfall) / 10.f}, {false, 0});

		for (int hour = 0 ; hour < 24 ; hour++) { // No DST? Nothing visible in the example data files anyway.
			o.station = _station;
			// + chrono::seconds(0) prevents a compilation issue where the time subdivision doesn't match
			auto t = _tz.convertFromLocalTime(date::local_days{ymd} + chrono::hours(hour) + chrono::seconds(0));
			o.day = date::floor<date::days>(t);
			o.time = date::floor<chrono::seconds>(t);
			if (start > o.time)
				start = o.time;
			if (o.time > end)
				end = o.time;

			int temp, hum, rain, rainrate, leafwetness;
			input >> parse(temp, 4, 16) >> parse(hum, 2, 16) >> ignore(2) >> parse(rain, 4, 16)
				  >> parse(rainrate, 2, 16) >> parse(leafwetness, 2, 16);

			o.outsidetemp = {temp != 0xFFFF, static_cast<float>(temp - 400) / 10.f};
			o.outsidehum = {hum != 0xFF, static_cast<float>(hum) / 2.f};
			o.rainfall = {rain != 0xFFFF, static_cast<float>(rain) / 10.};
			o.rainrate = {rainrate != 0xFF, static_cast<float>(rainrate)};
			o.leafwetness_timeratio1 = {leafwetness != 0xFF, static_cast<float>(leafwetness) * 60.};

			bool ret = _db.insertV2DataPoint(o);
			if (!ret) {
				std::cerr << SD_ERR << "[Cimel4A " << _station << "] measurement: failed to insert datapoint"
						  << std::endl;
			}
		};
	}

	if (updateLastArchiveDownloadTime) {
		bool ret = _db.updateLastArchiveDownloadTime(_station, chrono::system_clock::to_time_t(end));
		if (!ret) {
			std::cerr << SD_ERR << "[Cimel4A " << _station
					  << "] management: failed to update the last archive download datetime" << std::endl;
		}
	}
	return true;
}

std::istream& operator>>(std::istream& is, const Cimel4AImporter::Ignorer& ignorer)
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
