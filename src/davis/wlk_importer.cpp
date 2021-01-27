/**
 * @file wlk_importer.cpp
 * @brief Implementation of the WlkImporter class
 * @author Laurent Georget
 * @date 2020-10-10
 */
/*
 * Copyright (C) 2020 JD Environnement <contact@meteo-concept.fr>
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
#include <vector>

#include <systemd/sd-daemon.h>
#include <date/date.h>
#include <message.h>

#include "wlk_importer.h"
#include "wlk_message.h"

namespace chrono = std::chrono;

namespace meteodata
{
WlkImporter::WlkImporter(const CassUuid& station, const std::string& timezone, DbConnectionObservations& db) :
	_station{station},
	_db{db},
	_tz{TimeOffseter::getTimeOffseterFor(timezone)}
{
}

bool WlkImporter::import(std::istream& input, date::sys_seconds& start, date::sys_seconds& end, bool updateLastArchiveDownloadTime)
{
	std::string line;
	std::getline(input, line);
	std::istringstream lineIterator{line};
	for (std::string field ; std::getline(lineIterator, field, '\t') ;) {
		size_t start = field.find_first_not_of(' ');
		size_t end = field.find_last_not_of(" \r");
		if (start == std::string::npos) {
			_fields.push_back("");
		} else {
			std::string&& f = field.substr(start, end - start + 1);
			_fields.push_back(f);
		}
	}

	std::getline(input, line);
	lineIterator = std::istringstream{line};
	unsigned int i = 0;
	for (std::string field ; std::getline(lineIterator, field, '\t') ; i++) {
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

	std::copy(_fields.begin(), _fields.end(), std::ostream_iterator<std::string>(std::cout, "|"));
	std::cout << std::endl;

	bool valid = false;
	date::sys_seconds s = date::floor<chrono::seconds>(chrono::system_clock::now());
	date::sys_seconds e;

	int ret = 0;
	for (unsigned int lineNumber=2 ; std::getline(input, line) ; lineNumber++) {
		lineIterator = std::istringstream{line};
		WlkMessage m{lineIterator, _tz, _fields};
		if (m) {
			ret = _db.insertV2DataPoint(_station, m);
			if (!ret) {
				std::cerr << SD_DEBUG << "WlkImporter: failed to insert entry at line " << lineNumber << std::endl;
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
			std::cerr << SD_DEBUG << "WlkImporter: failed to update the last archive download datetime" << std::endl;
		}
	}

	return valid;
}

}
