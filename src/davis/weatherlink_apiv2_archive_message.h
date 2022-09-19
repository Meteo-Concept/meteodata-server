/**
 * @file weatherlink_apiv2_archive_message.h
 * @brief Definition of the WeatherlinkApiv2ArchiveMessage class
 * @author Laurent Georget
 * @date 2019-09-17
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

#ifndef WEATHERLINK_APIV2_ARCHIVE_MESSAGE_H
#define WEATHERLINK_APIV2_ARCHIVE_MESSAGE_H

#include <cmath>
#include <cstdint>
#include <array>
#include <chrono>
#include <limits>
#include <iostream>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <cassandra.h>
#include <date.h>
#include <message.h>

#include "abstract_weatherlink_api_message.h"
#include "../time_offseter.h"
#include "weatherlink_apiv2_data_structures_parsers/abstract_parser.h"

namespace meteodata
{

namespace pt = boost::property_tree;

class WeatherlinkApiv2ArchivePage;

/**
 * @brief A Message able to receive and store a JSON file resulting from a call to
 * https://api.weatherlink.com/v2/historic/...
 */
class WeatherlinkApiv2ArchiveMessage : public AbstractWeatherlinkApiMessage
{
public:
	WeatherlinkApiv2ArchiveMessage(const TimeOffseter* timeOffseter);
	void parse(std::istream& input) override;

private:
	void ingest(const pt::ptree& data, SensorType sensorType, DataStructureType dataStructureType);
	void ingest(const pt::ptree& data, wlv2structures::AbstractParser& dedicatedParser);
	float extractRainFall(const pt::ptree& data);
	float extractRainRate(const pt::ptree& data);

	friend WeatherlinkApiv2ArchivePage;
};

}

#endif /* WEATHERLINK_APIV2_ARCHIVE_MESSAGE_H */
