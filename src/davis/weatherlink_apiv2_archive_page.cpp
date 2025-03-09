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

#include <string>
#include <algorithm>
#include <functional>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <cassandra.h>

#include "weatherlink_apiv2_archive_page.h"
#include "weatherlink_apiv2_archive_message.h"
#include "weatherlink_apiv2_data_structures_parsers/parser_factory.h"

namespace meteodata
{

namespace chrono = std::chrono;
namespace pt = boost::property_tree;

using SensorType = WeatherlinkApiv2ArchiveMessage::SensorType;
using DataStructureType = WeatherlinkApiv2ArchiveMessage::DataStructureType;

namespace
{
constexpr bool compareMessages(const std::tuple<SensorType, DataStructureType, WeatherlinkApiv2ArchiveMessage>& entry1,
		const std::tuple<SensorType, DataStructureType, WeatherlinkApiv2ArchiveMessage>& entry2)
{
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
	doParse(
		input,
		[this](auto&& entry) { return acceptEntry(entry); },
		{}
	);
}

void WeatherlinkApiv2ArchivePage::parse(std::istream& input, const std::map<int, CassUuid>& substations,
		const CassUuid& station,
		const std::map<int, std::map<std::string, std::string>>& variables)
{
	doParse(
		input,
		[this, substations, station](auto&& entry) { return acceptEntryWithSubstations(entry, substations, station); },
		variables
	);
}


void WeatherlinkApiv2ArchivePage::doParse(std::istream& input, const Acceptor& acceptable,
		const std::map<int, std::map<std::string, std::string>>& variables)
{
	pt::ptree jsonTree;
	pt::read_json(input, jsonTree);

	std::vector<std::tuple<SensorType, DataStructureType, WeatherlinkApiv2ArchiveMessage>> entries;
	std::vector<WeatherlinkApiv2ArchiveMessage> separatelyParsedEntries;

	for (std::pair<const std::string, pt::ptree>& reading : jsonTree.get_child("sensors")) {
		if (!acceptable(reading))
			continue;

		auto dataIt = reading.second.find("data");
		if (dataIt == reading.second.not_found() || dataIt->second.empty())
			continue;

		int lsid = reading.second.get<int>("lsid", -1);
		auto customParser = variables.find(lsid);
		DataStructureType dataStructureType = static_cast<DataStructureType>(reading.second.get<int>( "data_structure_type"));
		if (customParser == variables.end()) {
			// conventional parsing

			SensorType sensorType = static_cast<SensorType>(reading.second.get<int>("sensor_type"));
			for (std::pair<const std::string, pt::ptree>& data : dataIt->second) {
				WeatherlinkApiv2ArchiveMessage message(_timeOffseter);
				message.ingest(data.second, sensorType, dataStructureType);
				if (message._obs.time == chrono::system_clock::from_time_t(0)) {
					// nothing has been parsed, continuing
					continue;
				}
				if (message._obs.time > _newest)
					_newest = date::floor<chrono::seconds>(message._obs.time);
				if (message._obs.time < _oldest)
					_oldest = date::floor<chrono::seconds>(message._obs.time);
				entries.emplace_back(sensorType, dataStructureType, std::move(message));
			}
		} else {
			// custom parsing
			auto parser = wlv2structures::ParserFactory::makeParser(reading.second.get<int>("sensor_type"), customParser->second, dataStructureType);
			// delay the custom parsing after the default one since it can override it
			if (parser) {
				for (std::pair<const std::string, pt::ptree>& data : dataIt->second) {
					WeatherlinkApiv2ArchiveMessage message(_timeOffseter);
					message.ingest(data.second, *parser);
				std::cerr << "Message obs time " << date::format("%F %TZ", message._obs.time) << "\n";
					if (message._obs.time > _newest)
						_newest = date::floor<chrono::seconds>(message._obs.time);
					if (message._obs.time < _oldest)
						_oldest = date::floor<chrono::seconds>(message._obs.time);
					separatelyParsedEntries.push_back(std::move(message));
				}
			}
		}
	}
	std::sort(entries.begin(), entries.end(), &compareMessages);
	std::transform(std::make_move_iterator(entries.begin()), std::make_move_iterator(entries.end()),
			std::back_inserter(_messages), [](auto&& entry) {
			return std::move(std::get<2>(entry));
		});
	std::move(std::make_move_iterator(separatelyParsedEntries.begin()), std::make_move_iterator(separatelyParsedEntries.end()),
		  std::back_inserter(_messages));
}

}
