/**
 * @file vantagepro2archivemessage.h
 * @brief Definition of the VantagePro2ArchiveMessage class
 * @author Laurent Georget
 * @date 2017-10-11
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

#ifndef VANTAGEPRO2ARCHIVEMESSAGE_H
#define VANTAGEPRO2ARCHIVEMESSAGE_H

#include <cstdint>
#include <array>
#include <chrono>

#include <boost/asio.hpp>

#include <cassandra.h>

#include "message.h"
#include "timeoffseter.h"

using std::uint8_t;
using std::uint16_t;
using std::uint32_t;
using std::uint64_t;

namespace meteodata {

namespace asio = boost::asio;
namespace chrono = std::chrono;

/**
 * @brief A Message able to receive and store one raw data point from a
 * VantagePro2 (R) station, by Davis Instruments (R)
 */
class VantagePro2ArchiveMessage : public Message
{
public:
	/**
	 * @brief An archive data point, used by VantagePro2 (R) stations, and
	 * documented by Davis Instruments (R)
	 */
	struct ArchiveDataPoint
	{
		unsigned int day   : 5;
		unsigned int month : 4;
		unsigned int year  : 7;
		uint16_t time;
		uint16_t outsideTemp;
		uint16_t maxOutsideTemp;
		uint16_t minOutsideTemp;
		uint16_t rainfall;
		uint16_t maxRainRate;
		uint16_t barometer;
		uint16_t solarRad;
		uint16_t nbWindSamples;
		uint16_t insideTemp;
		uint8_t  insideHum;
		uint8_t  outsideHum;
		uint8_t  avgWindSpeed;
		uint8_t  maxWindSpeed;
		uint8_t  maxWindSpeedDir;
		uint8_t  prevailingWindDir;
		uint8_t  uv;
		uint8_t  et;
		uint16_t maxSolarRad;
		uint8_t  maxUV;
		uint8_t  forecast;
		uint8_t  leafTemp[2];
		uint8_t  leafWetness[2];
		uint8_t  soilTemp[4];
		uint8_t  recordType;
		uint8_t  extraHum[2];
		uint8_t  extraTemp[3];
		uint8_t  soilMoisture[4];
	} __attribute__((packed));

	VantagePro2ArchiveMessage(const ArchiveDataPoint& data, const TimeOffseter* timeOffseter);
	virtual void populateDataPoint(const CassUuid station, CassStatement* const statement) const override;

private:
	/**
	 * @brief The data point
	 */
	ArchiveDataPoint _data;

	const TimeOffseter* _timeOffseter;
};

}

#endif /* VANTAGEPRO2ARCHIVEMESSAGE_H */
