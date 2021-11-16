/**
 * @file staticmessage.h
 * @brief Definition of the StatICMessage class
 * @author Laurent Georget
 * @date 2019-02-06
 */
/*
 * Copyright (C) 2019  JD Environnement <contact@meteo-concept.fr>
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

#ifndef STATIC_MESSAGE_H
#define STATIC_MESSAGE_H

#include <cstdint>
#include <array>
#include <chrono>
#include <optional>

#include <boost/asio.hpp>
#include <observation.h>
#include <date/date.h>

#include "../time_offseter.h"

namespace meteodata {

namespace asio = boost::asio;
namespace chrono = std::chrono;

/**
 * @brief A Message able to receive and store one raw data point from a
 * StatIC text file (otherwise exploited by Infoclimat)
 */
class StatICMessage
{
public:
	StatICMessage(std::istream& entry, const TimeOffseter& timeOffseter);
	void computeRainfall(float rainfall1h, float rainfallDay);
    Observation getObservation(const CassUuid station) const;

	inline operator bool() const {
		return _valid;
	}

	inline std::optional<float> getHourRainfall() const {
		return _hourRainfall;
	}

	inline std::optional<float> getDayRainfall() const {
		return _dayRainfall;
	}

	inline date::sys_seconds getDateTime() const {
		return _datetime;
	}

	constexpr static unsigned int MAXSIZE = 4 * 1024 * 1024; // 4 KiB, more than necessary

private:
	std::string _identifier; //numer_sta
	date::sys_seconds _datetime;
	std::optional<float> _airTemp;
	std::optional<float> _dewPoint;
	std::optional<int> _humidity;
	std::optional<int> _windDir;
	std::optional<float> _wind;
	std::optional<float> _pressure;
	std::optional<float> _gust;
	std::optional<float> _rainRate;
	std::optional<int> _solarRad;
	std::optional<int> _uv;
	std::optional<float> _hourRainfall;
	std::optional<float> _dayRainfall;
	std::optional<float> _computedRainfall;
	bool _valid;
	const TimeOffseter& _timeOffseter;
};

}

#endif /* STATIC_MESSAGE_H */
