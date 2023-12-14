/**
 * @file mbdata_meteobridge_message.cpp
 * @brief Implementation of the MBDataMeteobridgeMessage class
 * @author Laurent Georget
 * @date 2023-12-14
 */
/*
 * Copyright (C) 2023 JD Environnement <contact@meteo-concept.fr>
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
#include <chrono>
#include <algorithm>
#include <regex>
#include <stdexcept>
#include <string>

#include <date.h>
#include <message.h>

#include "time_offseter.h"
#include "davis/vantagepro2_message.h"
#include "mbdata_meteobridge_message.h"

namespace meteodata
{
MBDataMeteobridgeMessage::MBDataMeteobridgeMessage(std::istream& file,
	std::optional<float> dayRainfall, const TimeOffseter& timeOffseter) :
		AbstractMBDataMessage{timeOffseter},
		_rainfallSince0h{dayRainfall}
{
	using namespace date;
	std::string st;
	date::sys_seconds date;
	chrono::seconds hour;
	bool hasDate = false;
	const std::regex normalLine{R"(\s*(\S+) (\S*)\s*)"};
	const std::regex dateTimeRegex{R"((\d\d\d\d)(\d\d)(\d\d)(\d\d)(\d\d)(\d\d))"};
	int year = 0, month = 0, day = 0, h = 0, min = 0, sec = 0;

	while (std::getline(file, st)) {
		std::smatch baseMatch;
		if (std::regex_match(st, baseMatch, normalLine) && baseMatch.size() == 3) {
			std::string var = baseMatch[1].str();
			std::string value = baseMatch[2].str();
			if (value.empty())
				continue;

			try {
				if (var == "actual_utcdate") {
					std::smatch dateTimeMatch;
					if (std::regex_match(value, dateTimeMatch, dateTimeRegex) && dateTimeMatch.size() == 7) {
						year = std::atoi(dateTimeMatch[1].str().data());
						month = std::atoi(dateTimeMatch[2].str().data());
						day = std::atoi(dateTimeMatch[3].str().data());
						h = std::atoi(dateTimeMatch[4].str().data());
						min = std::atoi(dateTimeMatch[5].str().data());
						sec = std::atoi(dateTimeMatch[6].str().data());
						hasDate = true;
					}
				} else if (var == "actual_th0_temp_c") {
					_airTemp = std::stof(value);
				} else if (var == "actual_th0_hum_rel") {
					_humidity = std::stoi(value);
				} else if (var == "actual_th0_dew_c") {
					_dewPoint = std::stof(value);
				} else if (var == "actual_thb0_press_hpa") {
					_pressure = std::stof(value);
				} else if (var == "last15m_wind0_maindir_deg") {
					_windDir = std::stoi(value);
				} else if (var == "last15m_wind0_speed_kmh") {
					_wind = std::stof(value);
				} else if (var == "last15m_wind0_gustspeedmax_kmh") {
					_gust = std::stof(value);
				} else if (var == "last15m_rain0_ratemax_mm") {
					_rainRate = std::stof(value);
				} else if (var == "day1_rain0_total_mm") {
					try {
						float f = std::stof(value);
						if (_rainfallSince0h) {
							if (f >= 0 && f < 100)
								_computedRainfall = f - *_rainfallSince0h;
						}
						_rainfallSince0h = f;
					} catch (std::exception&) {
					}
				} else if (var == "actual_sol0_radiation_wqm") {
					_solarRad = std::stoi(value);
				} else if (var == "actual_uv0_index") {
					_uv = std::stoi(value);
				}
			} catch (const std::invalid_argument&) { // let invalid conversions fail silently
			}
		}
	}

	if (hasDate) {
		_valid = true;
		_datetime = date::floor<chrono::seconds>(_timeOffseter.convertFromLocalTime(day, month, year, h, min) + chrono::seconds{sec});
	}
}

std::optional<float> MBDataMeteobridgeMessage::getRainfallSince0h() const
{
	return _rainfallSince0h;
}

}
