/**
 * @file abstractmbdatadecodingstrategy.h
 * @brief Definition of the MBDataMessageFactory class
 * @author Laurent Georget
 * @date 2019-02-21
 */
/*
 * Copyright (C) 2019  JD Environnement <contact@meteo-concept.fr>
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
#include <memory>
#include <stdexcept>
#include <regex>

#include <dbconnection_observations.h>
#include <date.h>

#include "../../time_offseter.h"
#include "abstract_mbdata_message.h"
#include "mbdata_weatherlink_message.h"
#include "mbdata_meteohub_message.h"
#include "mbdata_weathercat_message.h"
#include "mbdata_wswin_message.h"
#include "mbdata_weatherdisplay_message.h"

namespace meteodata
{

namespace chrono = std::chrono;

class MBDataMessageFactory
{
private:
	static inline std::tuple<date::sys_seconds, date::sys_seconds>
	parseDatetime(std::istream& entry, const std::string& format, const TimeOffseter& timeOffseter)
	{
		using namespace date;

		local_seconds unzonedDatetime;
		entry >> parse(format, unzonedDatetime);
		auto midnight = local_seconds(floor<days>(unzonedDatetime));
		return {floor<chrono::seconds>(timeOffseter.convertFromLocalTime(unzonedDatetime)),
				timeOffseter.convertFromLocalTime(midnight)};
	}

	static std::string cleanInput(std::istream& entry)
	{
		std::string content = std::string{std::istreambuf_iterator<char>(entry), std::istreambuf_iterator<char>()};

		std::tuple<std::regex, std::string> regexps[] = {
			{std::regex{"\\&#124;"},                       "|"},
			{std::regex{"\\%[0-9a-zA-Z\\_\\[\\]\\.]+\\%"}, ""},
			{std::regex{"\\s+"},                           ""},
			{std::regex{","},                              "."},
			{std::regex{"<!--.+?-->"},                     ""},
			{std::regex{"\\+"},                            ""},
			{std::regex{"---"},                            ""},
			{std::regex{"--"},                             ""},
			{std::regex{"\\[[^\\]]*\\]"},                  ""},
			{std::regex{"-99"},                            ""}
		};

		for (auto&& r : regexps) {
			content = std::regex_replace(content, std::get<0>(r), std::get<1>(r));
		}
		return content;
	}


public:
	static inline AbstractMBDataMessage::ptr
	chose(DbConnectionObservations& db, const CassUuid& station, const std::string& type, std::istream& entry,
		  const TimeOffseter& timeOffseter)
	{
		using namespace date;

		auto lastMeasureSpan = chrono::minutes(AbstractMBDataMessage::POLLING_PERIOD);
		std::string content = cleanInput(entry);
		std::istringstream contentStream{content};

		std::optional<float> rainfall;
		if (type == "weatherlink") {
			sys_seconds datetime, midnight;
			std::tie(datetime, midnight) = parseDatetime(contentStream, "%d/%m/%y;%H:%M;", timeOffseter);
			auto begin = chrono::system_clock::to_time_t(midnight);
			auto end = chrono::system_clock::to_time_t(datetime);
			float f;
			if (db.getRainfall(station, begin, end, f))
				rainfall = f;
			return AbstractMBDataMessage::create<MBDataWeatherlinkMessage>(datetime, content, rainfall, timeOffseter);
		} else if (type == "meteohub") {
			sys_seconds datetime;
			std::tie(datetime, std::ignore) = parseDatetime(contentStream, "%Y-%m-%d;%H:%M;", timeOffseter);
			auto begin = chrono::system_clock::to_time_t(datetime - chrono::hours(1));
			auto end = chrono::system_clock::to_time_t(datetime);
			float f;
			if (db.getRainfall(station, begin, end, f))
				rainfall = f;
			return AbstractMBDataMessage::create<MBDataMeteohubMessage>(datetime, content, rainfall, timeOffseter);
		} else if (type == "weathercat") {
			sys_seconds datetime, midnight;
			std::tie(datetime, midnight) = parseDatetime(contentStream, "%Y-%m-%d;%H:%M;", timeOffseter);
			auto begin = chrono::system_clock::to_time_t(midnight);
			auto end = chrono::system_clock::to_time_t(datetime);
			float f;
			if (db.getRainfall(station, begin, end, f))
				rainfall = f;
			return AbstractMBDataMessage::create<MBDataWeathercatMessage>(datetime, content, rainfall, timeOffseter);
		} else if (type == "wswin") {
			sys_seconds datetime;
			std::tie(datetime, std::ignore) = parseDatetime(contentStream, "%Y-%m-%d;%H:%M;", timeOffseter);
			auto begin = chrono::system_clock::to_time_t(datetime - chrono::hours(1));
			auto end = chrono::system_clock::to_time_t(datetime);
			float f;
			if (db.getRainfall(station, begin, end, f))
				rainfall = f;
			return AbstractMBDataMessage::create<MBDataWsWinMessage>(datetime, content, rainfall, timeOffseter);
		} else if (type == "weatherdisplay" || type == "cumulus" || type == "weewx") {
			sys_seconds datetime;
			std::tie(datetime, std::ignore) = parseDatetime(contentStream, "%Y-%m-%d;%H:%M;", timeOffseter);
			auto begin = chrono::system_clock::to_time_t(datetime - chrono::hours(1));
			auto end = chrono::system_clock::to_time_t(datetime);
			float f;
			if (db.getRainfall(station, begin, end, f))
				rainfall = f;
			return AbstractMBDataMessage::create<MBDataWeatherDisplayMessage>(datetime, content, rainfall, timeOffseter);
		} else {
			throw std::invalid_argument("Unknown message type");
		}
	}
};

}
