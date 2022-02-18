/**
 * @file mbdataweatherlinkmessage.h
 * @brief Definition of the MBDataWeatherlinkMessage class
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

#ifndef MBDATA_WEATHERLINK_MESSAGE
#define MBDATA_WEATHERLINK_MESSAGE

#include <cstdint>
#include <array>
#include <chrono>
#include <optional>

#include <boost/asio.hpp>
#include <date/date.h>

#include "../../time_offseter.h"
#include "abstract_mbdata_message.h"

namespace meteodata
{

namespace asio = boost::asio;
namespace chrono = std::chrono;

/**
 * @brief A Message able to receive and store one raw data point from a
 * Weatherlink-formatted MBData text file
 */
class MBDataWeatherlinkMessage : public AbstractMBDataMessage
{
public:
	MBDataWeatherlinkMessage(date::sys_seconds datetime, const std::string& content,
							 std::optional<float> previousRainfall, const TimeOffseter& timeOffseter);

	inline std::optional<float> getDailyRainfall() const
	{
		return _dailyRainfall;
	}

private:
	std::optional<float> _dailyRainfall;
};

}

#endif /* MBDATA_WEATHERLINK_MESSAGE */
