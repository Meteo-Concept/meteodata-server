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

#include <iostream>
#include <chrono>

#include <date/date.h>
#include <date/tz.h>
#include <cassobs/dbconnection_observations.h>

#include "vantagepro2_archive_page.h"
#include "vantagepro2_message.h"
#include "vantagepro2_archive_message.h"

#include "../time_offseter.h"

namespace chrono = std::chrono;

namespace meteodata
{

constexpr int VantagePro2ArchivePage::NUMBER_OF_DATA_POINTS_PER_PAGE;

bool VantagePro2ArchivePage::isValid() const
{
	return VantagePro2Message::validateCRC(&_page, sizeof(ArchivePage));
}

bool VantagePro2ArchivePage::isRelevant(const VantagePro2ArchiveMessage::ArchiveDataPoint& point)
{
	if (*reinterpret_cast<const uint32_t*>(&point) == 0xFFFFFFFF) // dash value
		return false;

	auto time = _timeOffseter->convertFromLocalTime(point.day, point.month,
			point.year + 2000, point.time / 100, point.time % 100);
	auto now = chrono::system_clock::now();
	if (time > _beginning && time <= now) {
		if (time > _mostRecent)
			_mostRecent = date::floor<chrono::seconds>(time);
		return true;
	}
	return false;
}

bool VantagePro2ArchivePage::store(DbConnectionObservations& db, const CassUuid& station)
{
	bool ret = true;
	std::vector<Observation> allObs;
	for (int i = 0 ; i < NUMBER_OF_DATA_POINTS_PER_PAGE && ret ; i++) {
		if (isRelevant(_page.points[i])) {
			VantagePro2ArchiveMessage msg{_page.points[i], _timeOffseter};
			if (msg.looksValid(_beginning)) {
				Observation o = msg.getObservation(station);
				ret = db.insertV2DataPoint(o);
				allObs.push_back(std::move(o));
			}
		}
	}
	ret = ret && db.insertV2DataPointsInTimescaleDB(allObs.begin(), allObs.end());
	return ret;
}

void VantagePro2ArchivePage::prepare(const date::sys_seconds& beginning, const TimeOffseter* timeOffseter)
{
	_timeOffseter = timeOffseter;
	_beginning = beginning;
	_mostRecent = _beginning;
}

}
