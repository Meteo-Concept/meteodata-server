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
#include <regex>
#include <string>

#include <systemd/sd-daemon.h>
#include <cassandra.h>
#include <date/date.h>
#include <dbconnection_observations.h>

#include "../time_offseter.h"
#include "../cassandra.h"
#include "../cassandra_utils.h"
#include "./cimel4A_importer.h"

namespace meteodata {

namespace sys = boost::system;
namespace chrono = std::chrono;

using namespace std::placeholders;
using namespace meteodata;

Cimel4AImporter::Cimel4AImporter(const CassUuid& station, const std::string& cimelId, const std::string& timezone, DbConnectionObservations& db) :
		_station{station},
		_cimelId{cimelId},
		_db{db},
		_tz{TimeOffseter::getTimeOffseterFor(timezone)}
{
}

bool Cimel4AImporter::import(std::istream& input, date::sys_seconds& start, date::sys_seconds& end, bool updateLastArchiveDownloadTime)
{
	std::string paragraph = readParagraph(input);

	std::regex id{"^S0\\s+(.{2})(.{2})(.{4}).(.)"};
	std::smatch match;
	auto result = std::regex_search(paragraph, match, id);

	if (result) {
		int dept = parseInt(match[2].str());
		int city = parseInt(match[3].str());
		int nb   = parseInt(match[4].str());

		if (match[1].str() != "4A") {
			std::cerr << SD_ERR << "[Cimel4AImporter] protocol: wrong station type \"" << match[1].str() << "\" (expected \"4A\")" << std::endl;
			return false;
		}

		std::string detectedId = std::to_string(dept * 10000 + city * 10 + nb);
		if (detectedId != _cimelId) {
			std::cerr << SD_ERR << "[Cimel4AImporter] protocol: wrong station id \"" << detectedId << "\" (expected \"" << _cimelId << "\")" << std::endl;
			return false;
		}
	}

	auto now = chrono::system_clock::now();
	auto currentYear = date::year_month_day{date::floor<date::days>(now)}.year();
	start = date::floor<chrono::seconds>(now);
	end = date::floor<chrono::seconds>(chrono::system_clock::from_time_t(0));

	for (;;) {
		if (!input)
			break;

		Observation o;

		paragraph = readParagraph(input);

		std::regex dailyValues{
			"^S0\\s+"            // start
			"([0-9]{2})([0-9]{2})"       // day / month
			"([0-9A-F]{4})([0-9A-F]{4})([0-9A-F]{4})" // Tn / Tx / Tm
			"[0-9A-F]{2}[0-9A-F]{2}" // hour of Tn / hour of Tx
			"[0-9A-F]{16}"              // ignored: humidity
			"([0-9A-F]{4})"             // rainfall
			"[0-9A-F]{88}"              // ignored
		};
		result = std::regex_search(paragraph, match, dailyValues);
		if (result) {
			auto ymd = date::year_month_day{
					currentYear,
					date::month(std::stoi(match[2].str(), nullptr, 10)),
					date::day(std::stoi(match[1].str(), nullptr, 10))
			};
			date::sys_days date{ymd};
			if (match[3].str() != "FFFF")
				_db.insertV2Tn(_station, chrono::system_clock::to_time_t(date), (parseInt(match[4].str()) - 400) / 10.);
			if (match[4].str() != "FFFF")
				_db.insertV2Tx(_station, chrono::system_clock::to_time_t(date), (parseInt(match[3].str()) - 400) / 10.);
			if (match[6].str() != "FFFF")
				_db.insertV2EntireDayValues(_station, chrono::system_clock::to_time_t(date), {true, parseInt(match[6].str()) / 10.}, {false, 0});

			std::regex hourlyValues{
					"(.{4})(.{2}).{2}(.{4})(.{2})(.{2})"
			};
			auto hourlyDataBegin = std::sregex_iterator(match[0].second, paragraph.end(), hourlyValues);
			auto hourlyDataEnd   = std::sregex_iterator();

			int hour = 0;
			for (auto i = hourlyDataBegin ; i != hourlyDataEnd ; ++i) {
				auto&& m = *i;
				o.station = _station;
				// + chrono::seconds(0) prevents a compilation issue where the time subdivision doesn't match
				auto timestamp = _tz.convertFromLocalTime(date::local_days{ymd} + chrono::hours(hour) + chrono::seconds(0));
				o.day = date::floor<date::days>(timestamp);
				o.time = date::floor<chrono::seconds>(timestamp);
				if (start > o.time)
					start = o.time;
				if (o.time > end)
					end = o.time;

				o.outsidetemp = { m[1].str() != "FFFF", (parseInt(m[1].str()) - 400) / 10. };
				o.outsidehum  = { m[2].str() != "FF"  , parseInt(m[2].str()) / 2. };
				o.rainfall    = { m[3].str() != "FFFF", parseInt(m[3].str()) / 10. };
				o.rainrate    = { m[4].str() != "FF"  , parseInt(m[4].str()) };
				o.leafwetness_timeratio1 = { m[5].str() != "FF", parseInt(m[5].str()) * 60. };

				bool ret = _db.insertV2DataPoint(o);
				if (!ret) {
					std::cerr << SD_ERR << "[Cimel4A " << _station << "] measurement: failed to insert datapoint" << std::endl;
				}
				hour++;
			}

			if (updateLastArchiveDownloadTime) {
				bool ret = _db.updateLastArchiveDownloadTime(_station, chrono::system_clock::to_time_t(end));
				if (!ret) {
					std::cerr << SD_ERR << "[Cimel4A " << _station << "] management: failed to update the last archive download datetime" << std::endl;
				}
			}
		}
	}
	return true;
}

int Cimel4AImporter::parseInt(const std::string& s)
{
	int result = 0;
	for (char c: s) {
		if (c >= '0' && c <= '9') {
			result = result * 16 + c - '0';
		} else if (c >= 'A' && c <= 'F') {
			result = result * 16 + c - 'A' + 10;
		}
	}
	return result;
}

 std::string Cimel4AImporter::readParagraph(std::istream& in)
{
	char c = in.get();
	if (c != 'S')
		return "";

	std::string paragraph;
	do {
		if (c != '\n' && c != '\r')
			paragraph += c;
		c = in.get();
	} while (in && c != 'S');
	in.unget();

	return paragraph;
}

}
