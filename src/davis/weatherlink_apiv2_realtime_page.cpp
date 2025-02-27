/**
 * @file weatherlink_apiv2_realtime_page.cpp
 * @brief Implementation of the WeatherlinkApiv1RealtimePage class
 * @author Laurent Georget
 * @date 2022-10-04
 */
/*
 * Copyright (C) 2022  SAS Météo Concept <contact@meteo-concept.fr>
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
#include <vector>
#include <algorithm>
#include <functional>
#include <cmath>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <cassandra.h>
#include <cassobs/observation.h>
#include <davis/weatherlink_apiv2_data_structures_parsers/parser_factory.h>

#include "vantagepro2_message.h"
#include "weatherlink_apiv2_realtime_page.h"
#include "weatherlink_apiv2_parser_trait.h"
#include "../time_offseter.h"
#include "../cassandra_utils.h"

namespace meteodata
{

namespace chrono = std::chrono;
namespace pt = boost::property_tree;

using SensorType = WeatherlinkApiv2RealtimeMessage::SensorType;
using DataStructureType = WeatherlinkApiv2RealtimeMessage::DataStructureType;

WeatherlinkApiv2RealtimePage::WeatherlinkApiv2RealtimePage(const TimeOffseter* timeOffseter, float& dayRain) :
	WeatherlinkApiv2ParserTrait{},
	_dayRain{dayRain},
	_timeOffseter{timeOffseter}
{}

void WeatherlinkApiv2RealtimePage::parse(std::istream& input)
{
	doParse(input, [this](auto&& entry) {
		return acceptEntry(std::forward<decltype(entry)>(entry));
	}, {});
}

void WeatherlinkApiv2RealtimePage::parse(std::istream& input,
	const std::map<int, CassUuid>& substations,
	const CassUuid& station,
	const std::map<int, std::map<std::string, std::string>>& variables)
{
	doParse(input, [this, substations, station](auto&& entry) {
		return acceptEntryWithSubstations(std::forward<decltype(entry)>(entry), substations, station);
	}, variables);
}

date::sys_seconds WeatherlinkApiv2RealtimePage::getLastUpdateTimestamp(std::istream& input,
	const std::map<int, CassUuid>& substations, const CassUuid& station)
{
	Acceptor acceptable;
	if (substations.empty())
		acceptable = [this](auto&& entry) { return acceptEntry(std::forward<decltype(entry)>(entry)); };
	else
		acceptable = [this, substations, station](auto&& entry) {
			return acceptEntryWithSubstations(std::forward<decltype(entry)>(entry), substations, station);
		};

	pt::ptree jsonTree;
	pt::read_json(input, jsonTree);

	std::vector<date::sys_seconds> updates = {date::floor<chrono::seconds>(
			chrono::system_clock::from_time_t(0))}; // put a default to simplify the logic of the max element

	for (std::pair<const std::string, pt::ptree>& reading : jsonTree.get_child("sensors")) {
		if (!acceptable(reading))
			continue;

		auto dataIt = reading.second.find("data");
		if (dataIt == reading.second.not_found() || dataIt->second.empty())
			continue;

		// we expect exactly one element, the current condition
		auto data = dataIt->second.front().second;
		updates.push_back(date::floor<chrono::seconds>(chrono::system_clock::from_time_t(data.get<int>("ts"))));
	}

	return *std::max_element(updates.begin(), updates.end());
}

void WeatherlinkApiv2RealtimePage::doParse(std::istream& input, const Acceptor& acceptable,
		  const std::map<int, std::map<std::string, std::string>>& variables)
{
	pt::ptree jsonTree;
	pt::read_json(input, jsonTree);

	std::vector<std::tuple<SensorType, DataStructureType, WeatherlinkApiv2RealtimeMessage>> entries;
	std::vector<WeatherlinkApiv2RealtimeMessage> separatelyParsedEntries;

	for (std::pair<const std::string, pt::ptree>& reading : jsonTree.get_child("sensors")) {
		if (!acceptable(reading))
			continue;

		auto dataIt = reading.second.find("data");
		if (dataIt == reading.second.not_found() || dataIt->second.empty()) // no data?! it's happened before
			continue;

		// we expect exactly one element, the current condition
		auto data = dataIt->second.front().second;

		int lsid = reading.second.get<int>("lsid", -1);
		auto customParser = variables.find(lsid);
		DataStructureType dataStructureType = static_cast<DataStructureType>(reading.second.get<int>("data_structure_type"));
		if (customParser == variables.end()) {
			// default parsing
			SensorType sensorType = static_cast<SensorType>(reading.second.get<int>("sensor_type"));
			WeatherlinkApiv2RealtimeMessage message{_timeOffseter, _dayRain};
			message.ingest(data, sensorType, dataStructureType);
			if (message._obs.time == chrono::system_clock::from_time_t(0)) {
				// nothing has been parsed, continuing
				continue;
			}
			if (!WeatherlinkApiv2RealtimeMessage::isInvalid(message._newDayRain)) {
				_newDayRain = message._newDayRain;
			}
			entries.emplace_back(sensorType, dataStructureType, std::move(message));
		} else {
			// custom parsing!
			auto parser = wlv2structures::ParserFactory::makeParser(reading.second.get<int>("sensor_type"), customParser->second, dataStructureType);
			// delay the custom parsing after the default one since it can override it
			if (parser) {
				WeatherlinkApiv2RealtimeMessage message{_timeOffseter, _dayRain};
				message.ingest(data, *parser);
				separatelyParsedEntries.push_back(std::move(message));
			}
		}
	}

	std::sort(entries.begin(), entries.end(), &WeatherlinkApiv2RealtimeMessage::compareDataPackages);

	std::transform(std::make_move_iterator(entries.begin()), std::make_move_iterator(entries.end()),
			std::back_inserter(_messages), [](auto&& entry) {
			return std::move(std::get<2>(entry));
		});
	std::move(std::make_move_iterator(separatelyParsedEntries.begin()),
		  std::make_move_iterator(separatelyParsedEntries.end()),
		  std::back_inserter(_messages));

	if (!WeatherlinkApiv2RealtimeMessage::isInvalid(_newDayRain)) {
		_dayRain = _newDayRain;
	}
}

}
