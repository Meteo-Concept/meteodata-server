/**
 * @file abstractmbdatamessage.cpp
 * @brief Definition of the AbstractMBDataMessage class
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

#include <algorithm>
#include <iterator>
#include <regex>

#include "../timeoffseter.h"
#include "abstractmbdatamessage.h"

namespace meteodata {

namespace chrono = std::chrono;

constexpr int AbstractMBDataMessage::POLLING_PERIOD;

AbstractMBDataMessage::AbstractMBDataMessage(date::sys_seconds datetime, const std::string& content, const TimeOffseter& timeOffseter) :
	Message(),
	_datetime(datetime),
	_content(content),
	_timeOffseter(timeOffseter)
{
}

}
