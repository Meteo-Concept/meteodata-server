/**
 * @file fieldclimate_api_archive_message_collection.cpp
 * @brief Implementation of the FieldClimateApiArchiveMessageCollection class
 * @author Laurent Georget
 * @date 2020-09-01
 */
/*
 * Copyright (C) 2020  SAS JD Environnement <contact@meteo-concept.fr>
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
#include <cassobs/message.h>

#include "fieldclimate_archive_message_collection.h"
#include "fieldclimate_archive_message.h"

namespace meteodata
{

namespace chrono = std::chrono;
namespace pt = boost::property_tree;

FieldClimateApiArchiveMessageCollection::FieldClimateApiArchiveMessageCollection(const TimeOffseter* timeOffseter,
																				 const std::map<std::string, std::string>* sensors)
		:
		_timeOffseter(timeOffseter),
		_sensors(sensors)
{
}

void FieldClimateApiArchiveMessageCollection::parse(std::istream& input)
{
	pt::ptree jsonTree;
	pt::read_json(input, jsonTree);

	// Every individual message will receive the full tree, but a specific
	// index (one index = one date = one datapoint)
	auto dates = jsonTree.get_child("dates");
	auto nb = dates.size();
	for (int i = 0 ; i < nb ; i++) {
		FieldClimateApiArchiveMessage message{_timeOffseter, _sensors};
		message.ingest(jsonTree, i);
		_messages.push_back(message);
	}
}


}
