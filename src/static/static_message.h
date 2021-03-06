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
#include <experimental/optional>

#include <boost/asio.hpp>
#include <message.h>
#include <date/date.h>

#include "../time_offseter.h"

namespace meteodata {

namespace asio = boost::asio;
namespace chrono = std::chrono;

/**
 * @brief A Message able to receive and store one raw data point from a
 * StatIC text file (otherwise exploited by Infoclimat)
 */
class StatICMessage : public Message
{
public:
	StatICMessage(std::istream& entry, const TimeOffseter& timeOffseter);
	virtual void populateDataPoint(const CassUuid station, CassStatement* const statement) const override;
	virtual void populateV2DataPoint(const CassUuid station, CassStatement* const statement) const override;
	void computeRainfall(float rainfall1h, float rainfallDay);

	inline operator bool() const {
		return _valid;
	}

	inline std::experimental::optional<float> getHourRainfall() const {
		return _hourRainfall;
	}

	inline std::experimental::optional<float> getDayRainfall() const {
		return _dayRainfall;
	}

	inline date::sys_seconds getDateTime() const {
		return _datetime;
	}

	constexpr static unsigned int MAXSIZE = 4 * 1024 * 1024; // 4 KiB, more than necessary

private:
	std::string _identifier; //numer_sta
	date::sys_seconds _datetime;
	std::experimental::optional<float> _airTemp;
	std::experimental::optional<float> _dewPoint;
	std::experimental::optional<int> _humidity;
	std::experimental::optional<int> _windDir;
	std::experimental::optional<float> _wind;
	std::experimental::optional<float> _pressure;
	std::experimental::optional<float> _gust;
	std::experimental::optional<float> _rainRate;
	std::experimental::optional<int> _solarRad;
	std::experimental::optional<int> _uv;
	std::experimental::optional<float> _hourRainfall;
	std::experimental::optional<float> _dayRainfall;
	std::experimental::optional<float> _computedRainfall;
	bool _valid;
	const TimeOffseter& _timeOffseter;
};

}

#endif /* STATIC_MESSAGE_H */
