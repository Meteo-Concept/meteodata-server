/**
 * @file weatherlink_apiv2_realtime_message.h
 * @brief Definition of the WeatherlinkApiv2RealtimeMessage class
 * @author Laurent Georget
 * @date 2019-09-09
 */
/*
 * Copyright (C) 2019  SAS Météo Concept <contact@meteo-concept.fr>
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

#ifndef WEATHERLINK_APIV2_REALTIME_MESSAGE_H
#define WEATHERLINK_APIV2_REALTIME_MESSAGE_H

#include <cmath>
#include <cstdint>
#include <array>
#include <chrono>
#include <limits>
#include <iostream>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <cassandra.h>
#include <date/date.h>
#include <message.h>

#include "abstract_weatherlink_api_message.h"
#include "../time_offseter.h"

using std::uint8_t;
using std::uint16_t;
using std::uint32_t;
using std::uint64_t;

namespace meteodata {

namespace chrono = std::chrono;
namespace pt = boost::property_tree;

/**
 * @brief A Message able to receive and store a JSON file resulting from a call to
 * https://api.weatherlink.com/v2/current/...
 */
class WeatherlinkApiv2RealtimeMessage : public AbstractWeatherlinkApiMessage
{
public:
	WeatherlinkApiv2RealtimeMessage(const TimeOffseter* timeOffseter);
	virtual void parse(std::istream& input);
};

}

#endif /* WEATHERLINK_APIV2_REALTIME_MESSAGE_H */
