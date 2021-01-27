/**
 * @file timeoffseter.cpp
 * @brief Implementation of the TimeOffseter class
 * @author Laurent Georget
 * @date 2017-10-13
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

#include <systemd/sd-daemon.h>
#include <date/date.h>
#include <date/tz.h>

#include "time_offseter.h"

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

	void TimeOffseter::prepare(const TimeOffseter::VantagePro2TimezoneBuffer& buffer)
	{
		if (buffer.gmtOrZone == 0 && buffer.manualDST == 0) { // full automatic mode
			_byTimezone = true;
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
				std::cerr << SD_WARNING << "Station has automatic DST but the station has no clue "
					  << "about DST settings for its timezone"
					  << std::endl;
				_timezoneInfo.timeOffset = vantageTimezoneIndex2Offet(buffer.timeZone);
				_byTimezone = false;
			}
		} else if (buffer.gmtOrZone == 0 && buffer.manualDST !=0) { //timezone but manual DST
			_timezoneInfo.timeOffset = vantageTimezoneIndex2Offet(buffer.timeZone);
			_byTimezone = false;
		} else {
			int hours = buffer.gmtOffset / 100;
			int minutes = (buffer.gmtOffset < 0 ? -buffer.gmtOffset : buffer.gmtOffset) % 100;

			_timezoneInfo.timeOffset = chrono::hours(hours) +
				                   chrono::minutes(minutes);

			_byTimezone = false;
		}
	}

	TimeOffseter TimeOffseter::getTimeOffseterFor(PredefinedTimezone tz)
	{
		TimeOffseter t;
		switch (tz) {
			case PredefinedTimezone::UTC:
				t._byTimezone = false;
				t._timezoneInfo.timeOffset = chrono::minutes(0);
				break;
			case PredefinedTimezone::FRANCE:
				t._byTimezone = true;
				t._timezoneInfo.timezone = date::locate_zone("Europe/Paris");
				break;
			case PredefinedTimezone::NEW_CALEDONIA:
				t._byTimezone = true;
				t._timezoneInfo.timezone = date::locate_zone("Pacific/Noumea");
				break;
		}
		return t;
	}

	TimeOffseter TimeOffseter::getTimeOffseterFor(const std::string& tz)
	{
		TimeOffseter t;
		t._byTimezone = true;
		// may throw if tz is not a valid timezone
		t._timezoneInfo.timezone = date::locate_zone(tz);
		return t;
	}
}
