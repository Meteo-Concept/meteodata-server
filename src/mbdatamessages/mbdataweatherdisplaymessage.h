/**
 * @file mbdataweatherdisplaymessage.h
 * @brief Definition of the MBDataWeatherDisplayMessage class
 * @author Laurent Georget
 * @date 2019-02-21
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

#ifndef MBDATA_WEATHERDISPLAY_MESSAGE
#define MBDATA_WEATHERDISPLAY_MESSAGE

#include <cstdint>
#include <array>
#include <chrono>
#include <experimental/optional>

#include <boost/asio.hpp>
#include <message.h>
#include <date/date.h>

#include "../timeoffseter.h"
#include "abstractmbdatamessage.h"

namespace meteodata {

namespace asio = boost::asio;
namespace chrono = std::chrono;

/**
 * @brief A Message able to receive and store one raw data point from a
 * WeatherDisplay-formatted MBData text file
 */
class MBDataWeatherDisplayMessage : public AbstractMBDataMessage
{
public:
	MBDataWeatherDisplayMessage(std::istream& entry, std::experimental::optional<float> rainfallOver50Min, const TimeOffseter& timeOffseter);
	virtual void populateDataPoint(const CassUuid station, CassStatement* const statement) const override;
	virtual void populateV2DataPoint(const CassUuid station, CassStatement* const statement) const override;

	inline operator bool() const {
		return _valid;
	}

	inline date::sys_seconds getDateTime() const {
		return _datetime;
	}

private:
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
	std::experimental::optional<float> _computedRainfall;
	std::experimental::optional<float> _diffRainfall;
	bool _valid;
};

}

#endif /* MBDATA_WEATHERDISPLAY_MESSAGE */
