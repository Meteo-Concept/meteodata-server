/**
 * @file weatherlink_api_realtime_message.h
 * @brief Definition of the WeatherlinkApiRealtimeMessage class
 * @author Laurent Georget
 * @date 2018-07-23
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

#ifndef WEATHERLINK_API_REALTIME_MESSAGE_H
#define WEATHERLINK_API_REALTIME_MESSAGE_H

#include <cmath>
#include <cstdint>
#include <array>
#include <chrono>
#include <limits>
#include <iostream>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

#include <cassandra.h>
#include <date/date.h>

#include "message.h"

using std::uint8_t;
using std::uint16_t;
using std::uint32_t;
using std::uint64_t;

namespace meteodata {

namespace chrono = std::chrono;
namespace pt = boost::property_tree;

/* in a nutshell:
	asio::read(socket, response);
	pt::ptree obs;
	pt::read_xml(responseStream, obs, pt::no_comments | pt:trim_whitespaces);
*/

/**
 * @brief A Message able to receive and store a XML file resulting from a call to
 * https://api.weatherlink.com/NoaaExt.xml?...
 */
class WeatherlinkApiRealtimeMessage : public Message
{
public:
	virtual void populateDataPoint(const CassUuid station, CassStatement* const statement) const override;
	virtual void populateV2DataPoint(const CassUuid station, CassStatement* const statement) const override;
	WeatherlinkApiRealtimeMessage();

	void parse(std::istream& input);
	constexpr static int MAXSIZE = (2 << 20);

private:
	constexpr static int INVALID_INT = std::numeric_limits<int>::min();
	constexpr static float INVALID_FLOAT = std::numeric_limits<float>::quiet_NaN();

	constexpr static bool isInvalid(float v) {
		return std::isnan(v); // /!\ NaN never compares equal to itself
	}

	constexpr static bool isInvalid(int v) {
		return v == INVALID_INT;
	}

	struct Observation {
		date::sys_time<chrono::milliseconds> time;
		float pressure;
		int humidity;
		float temperature;
		float temperatureF;
		int windDir;
		float windSpeed;
		float windGustSpeed;
		int solarRad;
		float uvIndex;
	};
	Observation _obs;


};

}

#endif /* WEATHERLINK_API_REALTIME_MESSAGE_H */
