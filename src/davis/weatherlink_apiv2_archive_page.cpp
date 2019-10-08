/**
 * @file weatherlink_apiv2_archive_page.cpp
 * @brief Implementation of the WeatherlinkApiv2ArchivePage class
 * @author Laurent Georget
 * @date 2019-09-19
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

#include <iostream>
#include <sstream>
#include <string>
#include <limits>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <cassandra.h>
#include <message.h>

#include "vantagepro2_message.h"
#include "weatherlink_apiv2_archive_page.h"
#include "weatherlink_apiv2_archive_message.h"

namespace meteodata {

namespace chrono = std::chrono;
namespace pt = boost::property_tree;

using SensorType = WeatherlinkApiv2ArchiveMessage::SensorType;
using DataStructureType = WeatherlinkApiv2ArchiveMessage::DataStructureType;

void WeatherlinkApiv2ArchivePage::parse(std::istream& input)
{
	pt::ptree jsonTree;
	pt::read_json(input, jsonTree);

	for (std::pair<const std::string, pt::ptree>& reading : jsonTree.get_child("sensors")) {
		SensorType sensorType = static_cast<SensorType>(reading.second.get<int>("sensor_type"));
		DataStructureType dataStructureType = static_cast<DataStructureType>(reading.second.get<int>("data_structure_type"));
		for (std::pair<const std::string, pt::ptree>& data : reading.second.get_child("data")) {
			WeatherlinkApiv2ArchiveMessage message;
			message.ingest(data.second, sensorType, dataStructureType);

			if (message._obs.time > _time) {
				_time = date::floor<chrono::seconds>(message._obs.time);
				_messages.emplace_back(std::move(message));
			}
		}

	}
}

}
