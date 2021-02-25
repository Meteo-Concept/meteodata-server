/**
 * @file objenious_api_archive_message_collection.cpp
 * @brief Implementation of the ObjeniousApiArchiveMessageCollection class
 * @author Laurent Georget
 * @date 2021-02-23
 */
/*
 * Copyright (C) 2021  SAS JD Environnement <contact@meteo-concept.fr>
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

#include "objenious_archive_message_collection.h"
#include "objenious_archive_message.h"
#include "objenious_api_downloader.h"

namespace meteodata {

namespace chrono = std::chrono;
namespace pt = boost::property_tree;

ObjeniousApiArchiveMessageCollection::ObjeniousApiArchiveMessageCollection(const std::map<std::string, std::string>* variables) :
	_variables{variables}
{
}

void ObjeniousApiArchiveMessageCollection::parse(std::istream& input)
{
	pt::ptree jsonTree;
	pt::read_json(input, jsonTree);

	auto&& data = jsonTree.get_child("values");
	int nb = data.size();
	if (nb == ObjeniousApiDownloader::PAGE_SIZE) {
		_mayHaveMore = true;
		_cursor = jsonTree.get<std::string>("cursor");
	}

	for (auto&& dataPoint : data) {
		ObjeniousApiArchiveMessage message{_variables};
		message.ingest(dataPoint.second);
		_messages.push_back(std::move(message));
	}
}


}
