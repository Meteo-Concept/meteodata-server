/**
 * @file vantagepro2archivepage.cpp
 * @brief Implementation of the VantagePro2ArchivePage class
 * @author Laurent Georget
 * @date 2017-10-11
 */
/*
 * Copyright (C) 2017 SAS Météo Concept <contact@meteo-concept.fr>
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

#include "vantagepro2archivepage.h"
#include "vantagepro2message.h"
#include "vantagepro2archivemessage.h"

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/gregorian/greg_date.hpp>

namespace chrono = boost::posix_time;
namespace greg = boost::gregorian;

namespace meteodata
{

bool VantagePro2ArchivePage::isValid() const
{
	return VantagePro2Message::validateCRC(&_page, sizeof(ArchivePage));
}

bool VantagePro2ArchivePage::isRelevant(const VantagePro2ArchiveMessage::ArchiveDataPoint& point)
{
	if (*reinterpret_cast<const uint32_t*>(&point) == 0xFFFFFFFF) // dash value
		return false;

	std::cerr << "Year: " << point.year << " | month: " << point.month << " | day: " << point.day << " | time: " << point.time << std::endl;
	std::cerr << "This archive records apparently dates from " << chrono::ptime(greg::date(point.year + 2000, point.month, point.day), chrono::hours(point.time / 100) + chrono::minutes(point.time % 100)) << std::endl;

	chrono::ptime datetime(greg::date(point.year + 2000, point.month, point.day), chrono::hours(point.time / 100) + chrono::minutes(point.time % 100));
	if (datetime > _beginning && datetime <= _now) {
		if (datetime > _mostRecent)
			_mostRecent = datetime;
		return true;
	}
}

void VantagePro2ArchivePage::storeToMessages()
{
	for (int i=0 ; i < 4 ; i++) {
		if (isRelevant(_page.points[i]))
			_archiveMessages.emplace_back(_page.points[i]);
	}
}

chrono::ptime VantagePro2ArchivePage::lastArchiveRecordDateTime() const
{
	return _mostRecent;
}

void VantagePro2ArchivePage::prepare(const chrono::ptime& beginning)
{
	_beginning = beginning;
	_now = chrono::second_clock::universal_time();
	_mostRecent = beginning;

	std::cerr << "Archive page size: " << sizeof(ArchivePage) << " bytes" << std::endl;
	std::cerr << "Archive data point size: " << sizeof(VantagePro2ArchiveMessage::ArchiveDataPoint) << " bytes" << std::endl;
}

void VantagePro2ArchivePage::clear()
{
	_archiveMessages.clear();
}

};
