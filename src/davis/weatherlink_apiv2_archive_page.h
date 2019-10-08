/**
 * @file weatherlink_apiv2_archive_page.h
 * @brief Definition of the WeatherlinkApiv2ArchivePage class
 * @author Laurent Georget
 * @date 2019-09-19
 */
/*
 * Copyright (C) 2019  SAS Météo Concept <contact@meteo-concept.fr>
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

#ifndef WEATHERLINK_APIV2_ARCHIVE_PAGE_H
#define WEATHERLINK_APIV2_ARCHIVE_PAGE_H

#include <iostream>
#include <vector>

#include <date.h>
#include <chrono>

#include "weatherlink_apiv2_archive_message.h"

namespace meteodata {

/**
 * @brief A Message able to receive and store a JSON file resulting from a call to
 * https://api.weatherlink.com/v2/historic/... and instantiate a
 * WeatherlinkApiv2ArchiveMessage for each data point
 */
class WeatherlinkApiv2ArchivePage
{
public:
	template<typename T>
	WeatherlinkApiv2ArchivePage(date::sys_time<T> lastArchive):
		_time{date::floor<chrono::seconds>(lastArchive)}
	{}
	void parse(std::istream& input);

private:
	std::vector<WeatherlinkApiv2ArchiveMessage> _messages;
	date::sys_seconds _time;

public:
	inline decltype(_messages)::const_iterator begin() const { return _messages.cbegin(); }
	inline decltype(_messages)::const_iterator end() const { return _messages.cend(); }
	inline date::sys_seconds getNewestMessageTime() { return _time; }
};

}

#endif /* WEATHERLINK_APIV2_ARCHIVE_PAGE_H */
