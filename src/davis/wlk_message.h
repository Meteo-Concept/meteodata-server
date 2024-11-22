/**
 * @file wlk_message.h
 * @brief Definition of the WlkMessage class
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

#ifndef WLK_MESSAGE_H
#define WLK_MESSAGE_H

#include <vector>
#include <chrono>
#include <optional>

#include <cassobs/observation.h>
#include <date.h>
#include <cassandra.h>

#include "../time_offseter.h"

namespace meteodata
{

namespace chrono = std::chrono;

/**
 * @brief A Message able to receive and store one line from the .wlk files
 * exported by the Weatherlink software.
 */
class WlkMessage
{
public:
	WlkMessage(std::istream& entry, const TimeOffseter& tz, const std::vector<std::string>& fields);
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
	// WLK files have at least the following fields:
	//Date|Time|Temp Out|Hi Temp|Low Temp|Out Hum|Dew Pt.|Wind Speed|Wind Dir|Wind Run|Hi Speed|Hi Dir|Wind Chill|Heat Index|THW Index|Bar|Rain|Rain Rate|Heat D-D|Cool D-D|In Temp|In Hum|In Dew|In Heat|In EMC|In Air Density|Wind Samp|Wind Tx|ISS Recept|Arc. Int.
	date::sys_seconds _datetime; //Date + Time, dd/mm/yy H:MM
	std::optional<float> _airTemp; // Temp Out, °C
	std::optional<float> _maxAirTemp; // Hi Temp, °C
	std::optional<float> _minAirTemp; // Low Temp, °C
	std::optional<int> _humidity; // Out Hum, %
	std::optional<float> _dewPoint; // Dew Pt., °C
	std::optional<float> _windSpeed; // Wind Speed, km/h
	std::optional<float> _windDir; // Wind Dir, cardinal point
	std::optional<float> _gust; // Hi Speed, km/h
	std::optional<float> _windChill; // Wind Chill, °C
	std::optional<float> _heatIndex; // Heat Index, °C
	std::optional<float> _pressure; // Bar, hPa
	std::optional<float> _rainfall; // Rain, mm
	std::optional<float> _rainrate; // Rain Rate, mm
	std::optional<float> _solarRad; // Global Solar Radiation, W/m²
	std::optional<float> _et;       // Evapotranspiration, mm
	bool _valid;

	friend std::ostream& operator<<(std::ostream& out, const WlkMessage& m);
};

}

#endif /* WLK_MESSAGE_H */

