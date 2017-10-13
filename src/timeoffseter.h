/**
 * @file timeoffseter.h
 * @brief Definition of the TimeOffseter class
 * @author Laurent Georget
 * @date 2017-10-11
 */
/*
 * Copyright (C) 2017  SAS Météo Concept <contact@meteo-concept.fr>
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

#ifndef TIMEOFFSETER_H
#define TIMEOFFSETER_H

#include <chrono>

#include <date/date.h>
#include <date/tz.h>

#include "message.h"

using std::uint8_t;
using std::uint16_t;
using std::uint32_t;
using std::uint64_t;

namespace meteodata {

namespace chrono = std::chrono;

class TimeOffseter
{
public:

	struct VantagePro2TimezoneBuffer
	{
		uint8_t timeZone;
		uint8_t manualDST;
		uint8_t activeDST;
		int16_t gmtOffset;
		uint8_t gmtOrZone;
	} __attribute__((packed));

	void prepare(const VantagePro2TimezoneBuffer& buffer);

	template<typename Duration = chrono::system_clock::duration>
	date::sys_time<Duration> convertFromLocalTime(int d, int m, int y, int h, int min) const
	{
		if (byTimezone) {
			date::local_time<Duration> local = date::local_days(date::day(d)/m/y) +
			       	chrono::hours(h) + chrono::minutes(min);
			return date::make_zoned(_timezoneInfo.timezone, local).get_sys_time();
		} else {
			date::sys_time<Duration> local = date::sys_days(date::day(d)/m/y) +
			       	chrono::hours(h) + chrono::minutes(min);
			return local - _timezoneInfo.timeOffset;
		}
	}

	template<typename Duration>
	date::sys_time<Duration> convertFromLocalTime(const date::local_time<Duration>& time) const
	{
		if (byTimezone) {
			return date::make_zoned(_timezoneInfo.timezone, time).get_sys_time();
		} else {
			return date::sys_time<Duration>{(time - _timezoneInfo.timeOffset).time_since_epoch()};
		}
	}

	template<typename Duration>
	date::local_time<Duration> convertToLocalTime(const date::sys_time<Duration>& time) const
	{
		if (byTimezone) {
			return date::make_zoned(_timezoneInfo.timezone, time).get_local_time();
		} else {
			return date::local_time<Duration>{(time + _timezoneInfo.timeOffset).time_since_epoch()};
		}
	}

private:
	union
	{
		const date::time_zone* timezone;
		chrono::minutes timeOffset;
	} _timezoneInfo;
	bool byTimezone;
};

}

#endif /* VANTAGEPRO2ARCHIVEMESSAGE_H */
