/**
 * @file weatherlink_apiv2_realtime_page.h
 * @brief Definition of the WeatherlinkApiv2RealtimePage class
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

#ifndef WEATHERLINK_APIV2_REALTIME_PAGE_H
#define WEATHERLINK_APIV2_REALTIME_PAGE_H

#include <iostream>
#include <chrono>
#include <vector>
#include <map>
#include <tuple>
#include <string>

#include <cassandra.h>
#include <date.h>
#include <cassobs/message.h>

#include "weatherlink_apiv2_realtime_message.h"
#include "weatherlink_apiv2_parser_trait.h"
#include "../time_offseter.h"

namespace meteodata
{

using SensorType = WeatherlinkApiv2RealtimeMessage::SensorType;
using DataStructureType = WeatherlinkApiv2RealtimeMessage::DataStructureType;

/**
 * @brief A collection of observation fragments collected from a call to
 * https://api.weatherlink.com/v2/current/...
 */
class WeatherlinkApiv2RealtimePage : public WeatherlinkApiv2ParserTrait
{
public:
	WeatherlinkApiv2RealtimePage(const TimeOffseter* timeOffseter, float& dayRain);
	void parse(std::istream& input) override;
	void parse(std::istream& input, const std::map<int, CassUuid>& substations, const CassUuid& station,
		const std::map<int, std::map<std::string, std::string>>& variables) override;
	date::sys_seconds getLastUpdateTimestamp(std::istream& input,
		const std::map<int, CassUuid>& substations, const CassUuid& station);

private:
	const TimeOffseter* _timeOffseter;
	std::vector<WeatherlinkApiv2RealtimeMessage> _messages;
	float& _dayRain;
	float _newDayRain = WeatherlinkApiv2RealtimeMessage::INVALID_FLOAT;
	void doParse(std::istream& input, const Acceptor& acceptable, const std::map<int, std::map<std::string, std::string>>& variables);

public:
	inline decltype(_messages)::const_iterator begin() const
	{ return _messages.cbegin(); }

	inline decltype(_messages)::const_iterator end() const
	{ return _messages.cend(); }
};

}

#endif /* WEATHERLINK_APIV2_REALTIME_PAGE_H */

