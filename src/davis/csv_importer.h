/**
 * @file csv_importer.h
 * @brief Definition of the CsvImporter class
 * @author Laurent Georget
 * @date 2021-04-29
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

#ifndef CSV_IMPORTER_H
#define CSV_IMPORTER_H

#include <iostream>
#include <vector>

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
using namespace meteodata;

/**
 * A CsvImporter instance is able to parse a CSV-like weather data file exported
 * by a software or a website
 *
 * Individual lines are parsed by an instance of the parameter class Msg.
 */
template<typename Msg, char separator = ',', int headerLines = 1>
class CsvImporter
{
public:
	CsvImporter(const CassUuid& station, const std::string& timezone, DbConnectionObservations& db) :
			_station{station},
			_db{db},
			_tz{TimeOffseter::getTimeOffseterFor(timezone)}
	{
	}

	bool import(std::istream& input, date::sys_seconds& start, date::sys_seconds& end,
				bool updateLastArchiveDownloadTime = false)
	{
		std::string line;
		std::getline(input, line);
		std::istringstream lineIterator{line};
		for (std::string field ; std::getline(lineIterator, field, separator) ;) {
			size_t start = field.find_first_not_of(' ');
			size_t end = field.find_last_not_of(" \r");
			if (start == std::string::npos) {
				_fields.push_back("");
			} else {
				std::string&& f = field.substr(start, end - start + 1);
				_fields.push_back(f);
			}
		}

		for (int h = headerLines - 1 ; h > 0 ; h--) {
			std::getline(input, line);
			lineIterator = std::istringstream{line};
			unsigned int i = 0;
			for (std::string field ; std::getline(lineIterator, field, separator) ; i++) {
				size_t start = field.find_first_not_of(' ');
				size_t end = field.find_last_not_of(" \r");
				if (start != std::string::npos) {
					std::string&& f = field.substr(start, end - start + 1);
					if (i >= _fields.size()) {
						_fields.push_back(f);
					} else {
						if (!_fields[i].empty())
							_fields[i] += " ";
						_fields[i] += f;
					}
				}
			}
		}

		std::cerr << SD_DEBUG << "[CsvImporter] measurement: ";
		std::copy(_fields.begin(), _fields.end(), std::ostream_iterator<std::string>(std::cerr, "|"));
		std::cerr << std::endl;

		bool valid = false;
		date::sys_seconds s = date::floor<chrono::seconds>(chrono::system_clock::now());
		date::sys_seconds e;

		int ret = 0;
		for (unsigned int lineNumber = headerLines + 1 ; std::getline(input, line) ; lineNumber++) {
			lineIterator = std::istringstream{line};
			Msg m{lineIterator, _tz, _fields};
			if (m) {
				ret = _db.insertV2DataPoint(m.getObservation(_station));
				if (!ret) {
					std::cerr << SD_ERR << "[CsvImporter] measurement: failed to insert entry at line " << lineNumber
							  << std::endl;
				} else {
					valid = true;
					date::sys_seconds newDate = m.getDateTime();
					s = std::min(s, newDate);
					e = std::max(e, newDate);
				}
			}
		}

		if (valid && updateLastArchiveDownloadTime) {
			start = s;
			end = e;
			ret = _db.updateLastArchiveDownloadTime(_station, chrono::system_clock::to_time_t(end));
			if (!ret) {
				std::cerr << SD_ERR << "[CsvImporter] management: failed to update the last archive download datetime"
						  << std::endl;
			}
		}

		return valid;
	}

private:
	CassUuid _station;
	DbConnectionObservations& _db;
	TimeOffseter _tz;
	std::vector<std::string> _fields;
};

}

#endif
