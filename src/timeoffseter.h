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

#include <message.h>

using std::uint8_t;
using std::uint16_t;
using std::uint32_t;
using std::uint64_t;

namespace meteodata {

namespace chrono = std::chrono;

/**
 * @brief Perform conversion between station time and server (POSIX) time
 *
 * The VantagePro2 and VantageVue stations can be configured in local time or
 * in UTC time, and the timestamp of the archived data are to be interpreted
 * according to this configuration. There are two ways to configure the time
 * zone of a station: either by giving an index in an array of timezones
 * hardcoded in the station's firmware or by giving the offset to UTC. In the
 * former case, the station can automatically handle Daylight Saving Time (DST)
 * for some timezones (in Europe and USA). The rules to do so are hardcoded in
 * the firmware and we have no guarantee they are correct. Of course, for a lot
 * of complex cases (I'm looking at you Arizona...), it is mandatory to
 * manually handle DST, there is also a setting for that.
 *
 * This class attempts to detect the setting currently used by the station to
 * convert the archive timestamps to UTC. It is also used to compute the
 * station local time to set the clock.
 */
class TimeOffseter
{
public:
	/**
	 * @brief Store the portion of a station's EEPROM relative to the time
	 * configuration
	 */
	struct VantagePro2TimezoneBuffer
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

	enum class PredefinedTimezone
	{
		UTC = 0,
		FRANCE = 1,
		NEW_CALEDONIA = 2
	};

	/**
	 * @brief Initialize the \a TimeOffseter with a
	 * \a VantagePro2TimezoneBuffer received from a station
	 *
	 * The \a TimeOffseter must be initialized before it can converts times,
	 * this method is responsible for parsing the \a buffer and preparing
	 * this \a TimeOffseter for incoming timestamp conversions.
	 */
	void prepare(const VantagePro2TimezoneBuffer& buffer);

	static TimeOffseter getTimeOffseterFor(PredefinedTimezone tz);

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
		if (_byTimezone) {
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
		if (_byTimezone) {
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
		if (_byTimezone) {
			return date::make_zoned(_timezoneInfo.timezone, time).get_local_time();
		} else {
			return date::local_time<Duration>{(time + _timezoneInfo.timeOffset).time_since_epoch()};
		}
	}

	inline float getLatitude() const { return _latitude; }
	inline void setLatitude(float lat) {
		_latitude = lat;
	}
	inline float getLongitude() const { return _longitude; }
	inline void setLongitude(float lon) {
		_longitude = lon;
	}
	inline int getMeasureStep() const { return _measureStep; }
	inline void setMeasureStep(int step) {
		_measureStep = step;
	}

private:
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
	bool _byTimezone;

	float _latitude;
	float _longitude;
	int _measureStep;
};

}

#endif /* VANTAGEPRO2ARCHIVEMESSAGE_H */
