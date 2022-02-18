/**
 * @file mileos_message.h
 * @brief Definition of the MileosMessage class
 * @author Laurent Georget
 * @date 2020-10-09
 */
/*
 * Copyright (C) 2020  JD Environnement <contact@meteo-concept.fr>
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

#ifndef MILEOS_MESSAGE_H
#define MILEOS_MESSAGE_H

#include <vector>
#include <chrono>
#include <optional>

#include <date/date.h>
#include <cassandra.h>
#include <observation.h>

#include "../time_offseter.h"

namespace meteodata
{

namespace chrono = std::chrono;

/**
 * @brief A Message able to receive and store one line from the Mileos xlsx
 * files exported from the platform.
 */
class MileosMessage
{
public:
	MileosMessage(std::istream& entry, const TimeOffseter& tz, const std::vector<std::string>& fields);
	Observation getObservation(const CassUuid station) const;

	inline operator bool()
	{
		return _valid;
	}

	inline date::sys_seconds getDateTime()
	{
		return _datetime;
	}

private:
	// Mileos files have the following fields:
	//jour|heure|T|TX|TN|RR|RRX|U|TD|VT|GI|VX|GIX|P
	//day|hour|Temp|Max temp|Min temp|rainfall|rainrate|hum|dewpoint|wind speed|wind dir|wind gust speed|gust wind dir|pressure
	date::sys_seconds _datetime; //Date + Time, dd/mm/yyyy HH:MM
	std::optional<float> _airTemp; // Temp Out, °C
	std::optional<float> _maxAirTemp; // Hi Temp, °C
	std::optional<float> _minAirTemp; // Low Temp, °C
	std::optional<float> _rainfall; // Rain, mm
	std::optional<float> _rainrate; // Rain Rate, mm
	std::optional<int> _humidity; // Out Hum, %
	std::optional<float> _dewPoint; // Dew Pt., °C
	std::optional<float> _windSpeed; // Wind Speed, km/h
	std::optional<float> _windDir; // Wind Dir, cardinal point
	std::optional<float> _gust; // Hi Speed, km/h
	std::optional<float> _pressure; // Bar, hPa
	bool _valid;

	friend std::ostream& operator<<(std::ostream& out, const MileosMessage& m);
};

}

#endif /* MILEOS_MESSAGE_H */

