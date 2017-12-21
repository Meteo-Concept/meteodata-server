/**
 * @file vantagepro2connector.cpp
 * @brief Implementation of the VantagePro2Connector class
 * @author Laurent Georget
 * @date 2017-12-20
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

#include "vantagepro2station.h"
#include "dbconnection.h"

namespace
{
namespace chrono = std::chrono;
using namespace std::literals::chrono_literals;

/**
 * @brief Map a VantagePro2 timezone identifier with a static offset to UTC
 *
 * @param index A VantagePro2 timezone index from Davis Instruments (R) documentation
 *
 * @return The offset between the identified timezone and UTC, in minutes
 */
chrono::minutes vantageTimezoneIndex2Offet(int index)
{
	constexpr chrono::minutes timeOffsets[47] =
	{         -12h,         -11h,         -10h,         -9h,         -8h, -7h, -6h, -6h,  -6h,  -5h,
	           -5h,          -4h,          -4h, -3h - 30min,         -3h, -3h, -2h, -1h, 0min, 0min,
	            1h,           1h,           1h,          2h,          2h,  2h,  2h,  2h,   3h,   3h,
	    3h + 30min,           4h,   4h + 30min,          5h,  5h + 30min,  6h,  7h,  8h,   8h,   9h,
	    9h + 30min,   9h + 30min,          10h,         10h,         11h, 12h, 12h
	};

	return timeOffsets[index];
}
}

namespace meteodata
{
namespace chrono = std::chrono;

void VantagePro2Station::hydrate(const CassRow* row)
{
	const char *stationName;
	size_t size;
	cass_int64_t timeMillisec;

	cass_value_get_uuid(cass_row_get_column_by_name(row, "id"), &_id);
	cass_value_get_string(cass_row_get_column_by_name(row,"name"), &stationName, &size);
	_name.clear();
	_name.insert(0, stationName, size);
	cass_value_get_int64(cass_row_get_column_by_name(row, "last_archive_download"), &timeMillisec);
	_lastArchiveDownload = convertFromLocalTime(date::local_seconds(chrono::seconds(timeMillisec / 1000)));
	cass_value_get_float(cass_row_get_column_by_name(row, "latitude"), &_latitude);
	cass_value_get_float(cass_row_get_column_by_name(row, "longitude"), &_longitude);
}

bool VantagePro2Station::updateLastArchiveDownloadTime(DbConnection& db, const date::local_seconds& newTimestamp)
{
	_lastArchiveDownload = date::make_zoned(_timezoneInfo.timezone, newTimestamp, date::choose::latest);
	return db.updateLastArchiveDownloadTime(_id, _lastArchiveDownload.get_sys_time().time_since_epoch().count());
}

void VantagePro2Station::prepareTimeOffseter(const VantagePro2Station::TimezoneBuffer& buffer)
{
	if (buffer.gmtOrZone == 0 && buffer.manualDST == 0) { // full automatic mode
		byTimezone = true;
		if (buffer.timeZone == 4) {
			_timezoneInfo.timezone = date::locate_zone("America/Tijuana");
		} else if (buffer.timeZone == 5) {
			_timezoneInfo.timezone = date::locate_zone("America/Denver");
		} else if (buffer.timeZone == 6) {
			_timezoneInfo.timezone = date::locate_zone("America/Chicago");
		} else if (buffer.timeZone == 7) {
			_timezoneInfo.timezone = date::locate_zone("America/Mexico_City");
		} else if (buffer.timeZone == 10) {
			_timezoneInfo.timezone = date::locate_zone("America/New_York");
		} else if (buffer.timeZone == 11) {
			_timezoneInfo.timezone = date::locate_zone("America/Halifax");
		} else if (buffer.timeZone == 13) {
			_timezoneInfo.timezone = date::locate_zone("America/St_Johns");
		} else if (buffer.timeZone == 18) {
			_timezoneInfo.timezone = date::locate_zone("Europe/London");
		} else if (buffer.timeZone == 20) {
			_timezoneInfo.timezone = date::locate_zone("Europe/Berlin");
		} else if (buffer.timeZone == 21) {
			_timezoneInfo.timezone = date::locate_zone("Europe/Paris");
		} else if (buffer.timeZone == 22) {
			_timezoneInfo.timezone = date::locate_zone("Europe/Prague");
		} else if (buffer.timeZone == 23) {
			_timezoneInfo.timezone = date::locate_zone("Europe/Athens");
		} else if (buffer.timeZone == 25) {
			_timezoneInfo.timezone = date::locate_zone("Europe/Bucharest");
		} else {
			std::cerr << "Station has automatic DST but the station has no clue "
				  << "about DST settings for its timezone (or so we believe)"
				  << std::endl;
			_timezoneInfo.timeOffset = vantageTimezoneIndex2Offet(buffer.timeZone);
			byTimezone = false;
		}
	} else if (buffer.gmtOrZone == 0 && buffer.manualDST !=0) { //timezone but manual DST
		_timezoneInfo.timeOffset = vantageTimezoneIndex2Offet(buffer.timeZone);
		byTimezone = false;
	} else {
		int hours = buffer.gmtOffset / 100;
		int minutes = (buffer.gmtOffset < 0 ? -buffer.gmtOffset : buffer.gmtOffset) % 100;

		_timezoneInfo.timeOffset = chrono::hours(hours) +
			                   chrono::minutes(minutes);

		byTimezone = false;
	}
}
}
