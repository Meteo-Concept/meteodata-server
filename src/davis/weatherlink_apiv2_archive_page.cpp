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
#include <algorithm>
#include <functional>

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

namespace {
	constexpr bool compareMessages(
		const std::tuple<SensorType, DataStructureType, WeatherlinkApiv2ArchiveMessage>& entry1,
		const std::tuple<SensorType, DataStructureType, WeatherlinkApiv2ArchiveMessage>& entry2
	) {
		SensorType sensorType2 = std::get<0>(entry2);
		// Inject first the aux. sensor suites and then the ISS to have the possibility to
		// override the aux. sensor suites data with the ISS data.
		if (std::get<0>(entry1) == SensorType::SENSOR_SUITE &&
		    WeatherlinkApiv2ArchiveMessage::isMainStationType(sensorType2))
			return true;

		return false;
	}
}

void WeatherlinkApiv2ArchivePage::parse(std::istream& input)
{
	doParse(input, std::bind(&WeatherlinkApiv2ArchivePage::acceptEntry, this, std::placeholders::_1));
}

void WeatherlinkApiv2ArchivePage::parse(std::istream& input, const std::map<int, CassUuid>& substations, const CassUuid& station)
{
	doParse(input, std::bind(&WeatherlinkApiv2ArchivePage::acceptEntryWithSubstations, this, std::placeholders::_1, substations, station));
}


void WeatherlinkApiv2ArchivePage::doParse(std::istream& input, const Acceptor& acceptable)
{
	pt::ptree jsonTree;
	pt::read_json(input, jsonTree);

	std::vector<std::tuple<SensorType, DataStructureType, WeatherlinkApiv2ArchiveMessage>> entries;

	for (std::pair<const std::string, pt::ptree>& reading : jsonTree.get_child("sensors")) {
		if (!acceptable(reading))
			continue;

		SensorType sensorType = static_cast<SensorType>(reading.second.get<int>("sensor_type"));
		DataStructureType dataStructureType = static_cast<DataStructureType>(reading.second.get<int>("data_structure_type"));
        auto dataIt = reading.second.find("data");
        if (dataIt == reading.second.not_found() || dataIt->second.empty())
            continue;
		for (std::pair<const std::string, pt::ptree>& data : dataIt->second) {
			WeatherlinkApiv2ArchiveMessage message(_timeOffseter);
			message.ingest(data.second, sensorType, dataStructureType);
			if (message._obs.time > _time)
				_time = date::floor<chrono::seconds>(message._obs.time);
			entries.push_back(std::make_tuple(sensorType, dataStructureType, std::move(message)));
		}
	}
	std::sort(entries.begin(), entries.end(), &compareMessages);
	std::transform(
		std::make_move_iterator(entries.begin()),
		std::make_move_iterator(entries.end()),
		std::back_inserter(_messages),
		[](auto&& entry) {
			return std::move(std::get<2>(entry));
		}
	);
}

}
