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
#include <optional>

#include <boost/asio.hpp>
#include <date.h>

#include "../../time_offseter.h"
#include "abstract_mbdata_message.h"

namespace meteodata
{

namespace asio = boost::asio;
namespace chrono = std::chrono;

/**
 * @brief A Message able to receive and store one raw data point from a
 * WeatherDisplay-formatted MBData text file
 */
class MBDataWeatherDisplayMessage : public AbstractMBDataMessage
{
public:
	MBDataWeatherDisplayMessage(date::sys_seconds datetime,
		const std::string& content, const TimeOffseter& timeOffseter);
};

}

#endif /* MBDATA_WEATHERDISPLAY_MESSAGE */
