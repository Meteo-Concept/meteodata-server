/**
 * @file weatherlink_api_message.h
 * @brief Definition of the WeatherlinkApiMessage class
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

#ifndef ABSTRACT_WEATHERLINK_API_MESSAGE_H
#define ABSTRACT_WEATHERLINK_API_MESSAGE_H

#include <cmath>
#include <cstdint>
#include <array>
#include <chrono>
#include <limits>
#include <iostream>

#include <boost/property_tree/ptree.hpp>
#include <cassandra.h>
#include <date/date.h>
#include <message.h>

#include "../time_offseter.h"

using std::uint8_t;
using std::uint16_t;
using std::uint32_t;
using std::uint64_t;
using std::size_t;

namespace meteodata {

namespace chrono = std::chrono;
namespace pt = boost::property_tree;

/**
 * @brief A Message able to receive and store a file resulting from a call to
 * a Weatherlink APIhttps://api.weatherlink.com/NoaaExt.xml?...
 */
class AbstractWeatherlinkApiMessage : public Message
{
public:
	virtual void populateDataPoint(const CassUuid station, CassStatement* const statement) const override;
	virtual void populateV2DataPoint(const CassUuid station, CassStatement* const statement) const override;
	AbstractWeatherlinkApiMessage(const TimeOffseter* _timeOffseter);

	virtual void parse(std::istream& input) = 0;
	constexpr static size_t MAXSIZE = (2 << 20);

	enum class SensorType {
		GRO_WEATHER_FAN_CABLED = 2,
		VANTAGE_PRO_2_CABLED = 23,
		VANTAGE_PRO_2_PLUS_CABLED = 24,
		GRO_WEATHER_FAN_CABLED_METRIC = 26,
		VANTAGE_PRO_2_CABLED_METRIC = 27,
		VANTAGE_PRO_2_PLUS_CABLED_METRIC = 28,
		GRO_WEATHER_CABLED_METRIC = 30,
		GRO_WEATHER_PLUS_CABLED_METRIC = 31,
		VANTAGE_PRO = 33,
		VANTAGE_PRO_PLUS = 34,
		VANTAGE_PRO_2 = 43,
		VANTAGE_PRO_2_FAN = 44,
		VANTAGE_PRO_2_PLUS = 45,
		VANTAGE_PRO_2_PLUS_FAN = 46,
		VANTAGE_PRO_2_ISS = 48,
		VANTAGE_PRO_2_FAN_ISS = 49,
		VANTAGE_PRO_2_PLUS_ISS = 50,
		VANTAGE_PRO_2_PLUS_FAN_ISS = 51,
		VANTAGE_PRO_2_DAYTIME_FAN_ISS = 52,
		GRO_WEATHER_WIRELESS = 53,
		GRO_WEATHER_FAN_WIRELESS = 54,
		GRO_WEATHER_FAN_WIRELESS_METRIC = 70,
		GRO_WEATHER_FAN_WIRELESS_OV = 71,
		GRO_WEATHER_WIRELESS_METRIC = 72,
		GRO_WEATHER_WIRELESS_OV = 73,
		VANTAGE_PRO_2_FAN_WIRELESS_METRIC = 76,
		VANTAGE_PRO_2_FAN_WIRELESS_OV = 77,
		VANTAGE_PRO_2_WIRELESS_METRIC = 78,
		VANTAGE_PRO_2_WIRELESS_OV = 79,
		VANTAGE_PRO_2_PLUS_FAN_WIRELESS_METRIC = 80,
		VANTAGE_PRO_2_PLUS_FAN_WIRELESS_OV = 81,
		VANTAGE_PRO_2_PLUS_WIRELESS_METRIC = 82,
		VANTAGE_PRO_2_PLUS_WIRELESS_OV = 83,
		SENSOR_SUITE = 55,
		VANTAGE_VUE_ISS = 37,
		BAROMETER = 242,
		LEAF_SOIL_SUBSTATION = 56
	};

	enum class DataStructureType {
		WEATHERLINK_IP_CURRENT_READING_REVISION_B = 2,
		WEATHERLINK_IP_ARCHIVE_RECORD_REVISION_B = 4,
		WEATHERLINK_LIVE_CURRENT_READING = 10,
		WEATHERLINK_LIVE_NON_ISS_CURRENT_READING = 12,
		WEATHERLINK_LIVE_ISS_ARCHIVE_RECORD = 11,
		WEATHERLINK_LIVE_NON_ISS_ARCHIVE_RECORD = 13,
	};

protected:
	constexpr static int INVALID_INT = std::numeric_limits<int>::min();
	constexpr static float INVALID_FLOAT = std::numeric_limits<float>::quiet_NaN();

	constexpr static bool isInvalid(float v) {
		return std::isnan(v); // /!\ NaN never compares equal to itself
	}

	constexpr static bool isInvalid(int v) {
		return v == INVALID_INT;
	}

	constexpr bool isMainStationType(SensorType sensorType)
	{
		return
			sensorType == SensorType::GRO_WEATHER_FAN_CABLED ||
			sensorType == SensorType::VANTAGE_PRO_2_CABLED ||
			sensorType == SensorType::VANTAGE_PRO_2_PLUS_CABLED ||
			sensorType == SensorType::GRO_WEATHER_FAN_CABLED_METRIC ||
			sensorType == SensorType::VANTAGE_PRO_2_CABLED_METRIC ||
			sensorType == SensorType::VANTAGE_PRO_2_PLUS_CABLED_METRIC ||
			sensorType == SensorType::GRO_WEATHER_CABLED_METRIC ||
			sensorType == SensorType::GRO_WEATHER_PLUS_CABLED_METRIC ||
			sensorType == SensorType::VANTAGE_PRO ||
			sensorType == SensorType::VANTAGE_PRO_PLUS ||
			sensorType == SensorType::VANTAGE_PRO_2 ||
			sensorType == SensorType::VANTAGE_PRO_2_FAN ||
			sensorType == SensorType::VANTAGE_PRO_2_PLUS ||
			sensorType == SensorType::VANTAGE_PRO_2_PLUS_FAN ||
			sensorType == SensorType::VANTAGE_PRO_2_ISS ||
			sensorType == SensorType::VANTAGE_PRO_2_FAN_ISS ||
			sensorType == SensorType::VANTAGE_PRO_2_PLUS_ISS ||
			sensorType == SensorType::VANTAGE_PRO_2_PLUS_FAN_ISS ||
			sensorType == SensorType::VANTAGE_PRO_2_DAYTIME_FAN_ISS ||
			sensorType == SensorType::VANTAGE_VUE_ISS ||
		        sensorType == SensorType::GRO_WEATHER_WIRELESS ||
		        sensorType == SensorType::GRO_WEATHER_FAN_WIRELESS ||
		        sensorType == SensorType::GRO_WEATHER_FAN_WIRELESS_METRIC ||
		        sensorType == SensorType::GRO_WEATHER_FAN_WIRELESS_OV ||
		        sensorType == SensorType::GRO_WEATHER_WIRELESS_METRIC ||
		        sensorType == SensorType::GRO_WEATHER_WIRELESS_OV ||
		        sensorType == SensorType::VANTAGE_PRO_2_FAN_WIRELESS_METRIC ||
		        sensorType == SensorType::VANTAGE_PRO_2_FAN_WIRELESS_OV ||
		        sensorType == SensorType::VANTAGE_PRO_2_WIRELESS_METRIC ||
		        sensorType == SensorType::VANTAGE_PRO_2_WIRELESS_OV ||
		        sensorType == SensorType::VANTAGE_PRO_2_PLUS_FAN_WIRELESS_METRIC ||
		        sensorType == SensorType::VANTAGE_PRO_2_PLUS_FAN_WIRELESS_OV ||
		        sensorType == SensorType::VANTAGE_PRO_2_PLUS_WIRELESS_METRIC ||
		        sensorType == SensorType::VANTAGE_PRO_2_PLUS_WIRELESS_OV;
	}


	struct Observation {
		date::sys_time<chrono::milliseconds> time;
		float pressure = INVALID_FLOAT;
		int humidity = INVALID_INT;
		float temperature = INVALID_FLOAT;
		float temperatureF = INVALID_FLOAT;
		int windDir = INVALID_INT;
		float windSpeed = INVALID_FLOAT;
		float windGustSpeed = INVALID_FLOAT;
		float rainRate = INVALID_FLOAT;
		float rainFall = INVALID_FLOAT;
		int solarRad = INVALID_INT;
		float uvIndex = INVALID_FLOAT;
		int extraHumidity[2] = { INVALID_INT, INVALID_INT };
		float extraTemperature[3] = { INVALID_FLOAT, INVALID_FLOAT, INVALID_FLOAT };
		float leafTemperature[2] = { INVALID_FLOAT, INVALID_FLOAT };
		int leafWetness[2] = { INVALID_INT, INVALID_INT };
		int soilMoisture[4] = { INVALID_INT, INVALID_INT, INVALID_INT, INVALID_INT };
		float soilTemperature[4] = { INVALID_FLOAT, INVALID_FLOAT, INVALID_FLOAT, INVALID_FLOAT };
	};
	Observation _obs;

	/**
	 * @brief The \a TimeOffseter able to convert the archive entries' timestamps to
	 * POSIX time
	 */
	const TimeOffseter* _timeOffseter;
};

}

#endif /* ABSTRACT_WEATHERLINK_API_MESSAGE_H */
