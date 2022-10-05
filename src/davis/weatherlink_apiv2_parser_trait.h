/**
 * @file abstract_weatherlink_apiv2_message.h
 * @brief Definition of the AbstractWeatherlinkApiv2Message class
 * @author Laurent Georget
 * @date 2020-03-12
 */
/*
 * Copyright (C) 2020  SAS Météo Concept <contact@meteo-concept.fr>
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

#ifndef WEATHERLINK_APIV2_PARSER_TRAIT_H
#define WEATHERLINK_APIV2_PARSER_TRAIT_H

#include <cmath>
#include <cstdint>
#include <array>
#include <chrono>
#include <limits>
#include <iostream>

#include <cassandra.h>
#include <message.h>
#include <boost/property_tree/ptree.hpp>

#include "abstract_weatherlink_api_message.h"
#include "../cassandra_utils.h"

namespace meteodata
{

/**
 * @brief An abstract Message for the Weatherlink APIv2 observations route
 * (routes /current and /historic) that bring support for the substations
 * mapping from Weatherlink stations to Meteodata stations
 */
class WeatherlinkApiv2ParserTrait
{
public:
	virtual void parse(std::istream& input) = 0;
	virtual void parse(std::istream& input, const std::map<int, CassUuid>& substations, const CassUuid& station, const std::map<int, std::map<std::string, std::string>>& parsers) = 0;

protected:
	using Reading = std::pair<const std::string, pt::ptree>;
	using Acceptor = std::function<bool(Reading&)>;

	inline bool acceptEntryWithSubstations(const Reading& reading, const std::map<int, CassUuid>& substations,
		const CassUuid& station)
	{
		// lsid should not be missing, but even if it is, the entry will be rejected
		// by the test below (no lsid can be negative)
		int lsid = reading.second.get<int>("lsid", -1);
		auto allData = reading.second.get_child("data");
		auto s = substations.find(lsid);
		return !allData.empty() && s != substations.end() && s->second == station;
	}

	inline bool acceptEntry(const Reading& reading)
	{
		auto allData = reading.second.get_child("data");
		return !allData.empty();
	}
};

}

#endif /* WEATHERLINK_APIV2_PARSER_TRAIT_H */
