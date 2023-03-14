/**
 * @file mbdataweatherdisplaymessage.cpp
 * @brief Implementation of the MBDataWeatherDisplayMessage class
 * @author Laurent Georget
 * @date 2019-02-26
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
#include <array>
#include <chrono>
#include <regex>

#include <message.h>
#include <date.h>

#include "../../time_offseter.h"
#include "../../davis/vantagepro2_message.h"
#include "abstract_mbdata_message.h"
#include "mbdata_weatherdisplay_message.h"

namespace meteodata
{

namespace asio = boost::asio;
namespace chrono = std::chrono;

MBDataWeatherDisplayMessage::MBDataWeatherDisplayMessage(
	date::sys_seconds datetime,
	const std::string& content,
	const TimeOffseter& timeOffseter) :
		AbstractMBDataMessage(datetime, content, timeOffseter)
{
	using namespace date;

	const std::regex mandatoryPart{"^\\d+-\\d+-\\d+;\\d+:\\d+;" // date: already parsed
		"([^\\|]*)\\|" // temperature
		"([^\\|]*)\\|" // humidite
		"([^\\|]*)\\|" // dew point
		"([^\\|]*)\\|" // pressure
		"([^\\|]*)\\|" // pressure variable, should be null
		"([^\\|]*)\\|" // rainfall over 1 hour
		"([^\\|]*)\\|" // wind
		"([^\\|]*)\\|" // wind direction
		"([^\\|]*)\\|" // wind gusts
		"([^\\|]*)\\|" // windchill
		"([^\\|]*)(?:\\||$)" // HEATINDEX
	};

	const std::regex optionalPart{"([^\\|]*)\\|" // Tx since midnight
		"([^\\|]*)\\|" // Tn since midnight
		"([^\\|]*)\\|" // rainrate
		"([^\\|]*)\\|" // solar radiation
		"([^\\|]*)\\|" // hour of Tx
		"([^\\|]*)\\|?" // hour of Tn
	};

	std::smatch baseMatch;
	if (std::regex_search(_content, baseMatch, mandatoryPart) && baseMatch.size() == 12) {
		if (baseMatch[1].length()) {
			try {
				_airTemp = std::stof(baseMatch[1].str());
			} catch (std::exception&) {
			}
		}
		if (baseMatch[2].length()) {
			try {
				_humidity = std::stoi(baseMatch[2].str());
			} catch (std::exception&) {
			}
		}
		if (baseMatch[3].length()) {
			try {
				_dewPoint = std::stof(baseMatch[3].str());
			} catch (std::exception&) {
			}
		}
		if (baseMatch[4].length()) {
			try {
				_pressure = std::stof(baseMatch[4].str());
			} catch (std::exception&) {
			}
		}
		// skip pressure tendency
		// Store rainfall only at the top of the hour since we get it
		// over the last hour
		bool topOfTheHour = (datetime - date::floor<chrono::hours>(datetime)) < chrono::minutes(POLLING_PERIOD);
		if (baseMatch[6].length() && topOfTheHour) {
			try {
				_computedRainfall = std::stof(baseMatch[6].str());
			} catch (std::exception&) {
			}
		}
		if (baseMatch[7].length()) {
			try {
				_wind = std::stof(baseMatch[7].str());
			} catch (std::exception&) {
			}
		}
		if (baseMatch[8].length()) {
			try {
				_windDir = std::stoi(baseMatch[8].str());
			} catch (std::exception&) {
			}
		}
		if (baseMatch[9].length()) {
			try {
				_gust = std::stof(baseMatch[9].str());
			} catch (std::exception&) {
			}
		}
		// skip heatindex and windchill

		_valid = true;
	}

	std::smatch supplementaryMatch;
	if (std::regex_search(baseMatch[0].second, _content.cend(), supplementaryMatch, optionalPart) &&
		supplementaryMatch.size() == 7) {
		// skip Tx and Tn
		if (supplementaryMatch[3].length())
			_rainRate = std::stof(supplementaryMatch[3].str());
		if (supplementaryMatch[4].length())
			_solarRad = std::stoi(supplementaryMatch[4].str());
		// skip hours of Tx and Tn
	}
}

}
