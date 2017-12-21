/**
 * @file vantagepro2station.h
 * @brief Definition of the VantagePro2Station class
 * @author Laurent Georget
 * @date 2016-10-05
 */
/*
 * Copyright (C) 2016  SAS Météo Concept <contact@meteo-concept.fr>
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
#ifndef VANTAGEPRO2STATION_H
#define VANTAGEPRO2STATION_H

#include <iostream>
#include <memory>
#include <string>
#include <ctime>

#include <cassandra.h>
#include <date/date.h>
#include <date/tz.h>

#include "dbconnection.h"

namespace chrono = std::chrono;

namespace meteodata
{
class VantagePro2Station
{
public:
	/**
	 * @brief Store the portion of a station's EEPROM relative to the time
	 * configuration
	 */
	struct TimezoneBuffer
	{
		uint8_t timeZone;  /*!< The time zone configured for the station
				       if \a gmtOrZone is not set */
		uint8_t manualDST;  /*!< If set, the DST is handled manually and
				       the station does not set the clock
				       automatically, ignored if gmtOrZone is
				       set */
		uint8_t activeDST; /*!< If set and the DST is handled manually,
				     indicates that the DST is active */
		int16_t gmtOffset; /*!< The offset to UTC configured for this
				     station if \a gmtOrZone is set */
		uint8_t gmtOrZone; /*!< If set, \a timeZone is used, if unset
				     \a gmtOffset is used */
	} __attribute__((packed));

	void hydrate(const CassRow* row);
	bool updateLastArchiveDownloadTime(DbConnection& db, const date::local_seconds& newTimestamp);
	void prepareTimeOffseter(const TimezoneBuffer& buffer);

	inline float getLatitude() const { return _latitude; }
	inline float getLongitude() const { return _longitude; }
	inline int   getPollingPeriod() const { return _pollingPeriod; }
	inline const std::string& getName() const { return _name; }
	inline const CassUuid& getId() const { return _id; }
	inline const date::zoned_seconds& getLastArchiveDownloadTimestamp() const { return _lastArchiveDownload; }

	/**
	 * @brief Convert a timestamp given as fields and expressed as station
	 * time to POSIX time
	 *
	 * @tparam Duration The expected resolution of the returned timestamp
	 * @param d The day
	 * @param m The month
	 * @param y The year
	 * @param h The hour
	 * @param min The minutes
	 *
	 * @return A timestamp in POSIX time corresponding to the same time
	 * point as \a y-\a m-\a d \a h:\a min:00 in station time
	 */
	template<typename Duration = chrono::system_clock::duration>
	date::sys_time<Duration> convertFromLocalTime(int d, int m, int y, int h, int min) const
	{
		if (byTimezone) {
			date::local_time<Duration> local = date::local_days(date::day(d)/m/y) +
				chrono::hours(h) + chrono::minutes(min);
			return date::make_zoned(_timezoneInfo.timezone, local, date::choose::latest).get_sys_time();
		} else {
			date::sys_time<Duration> local = date::sys_days(date::day(d)/m/y) +
				chrono::hours(h) + chrono::minutes(min);
			return local - _timezoneInfo.timeOffset;
		}
	}

	/**
	 * @brief Convert a timestamp given as a duration from Epoch in station
	 * time to POSIX time
	 *
	 * @tparam Duration The expected resolution of the returned timestamp
	 * @param time The timestamp in station time
	 *
	 * @return A timestamp in POSIX time corresponding to the same time
	 * point as \a time
	 */
	template<typename Duration>
	date::sys_time<Duration> convertFromLocalTime(const date::local_time<Duration>& time) const
	{
		if (byTimezone) {
			return date::make_zoned(_timezoneInfo.timezone, time, date::choose::latest).get_sys_time();
		} else {
			return date::sys_time<Duration>{(time - _timezoneInfo.timeOffset).time_since_epoch()};
		}
	}

	/**
	 * @brief Convert a timestamp given as a duration from Epoch in POSIX
	 * time to station time
	 *
	 * @tparam Duration The expected resolution of the returned timestamp
	 * @param time The timestamp in POSIX time
	 *
	 * @return A timestamp in station time corresponding to the same time
	 * point as \a time
	 */
	template<typename Duration>
	date::local_time<Duration> convertToLocalTime(const date::sys_time<Duration>& time) const
	{
		if (byTimezone) {
			return date::make_zoned(_timezoneInfo.timezone, time).get_local_time();
		} else {
			return date::local_time<Duration>{(time + _timezoneInfo.timeOffset).time_since_epoch()};
		}
	}

protected:
	CassUuid _id;
	std::string _name;
	float _latitude;
	float _longitude;
	int _pollingPeriod;
	date::zoned_seconds _lastArchiveDownload;

	/**
	 * @brief Describe how the \a TimeOffseter should perform time
	 * conversions
	 */
	union
	{
		const date::time_zone* timezone; /*!< The station time is given by a IANA timezone */
		chrono::minutes timeOffset; /*!< The station time is given by a static offset to UTC */
	} _timezoneInfo;
	/**
	 * @brief Tell whether \a _timezoneInfo.timezone or \a _timezoneInfo.timeOffset should be
	 * taken into account
	 */
	bool byTimezone;
};
}

#endif // CONNECTOR_H
