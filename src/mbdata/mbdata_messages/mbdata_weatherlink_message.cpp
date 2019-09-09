/**
 * @file mbdataweatherlinkmessage.cpp
 * @brief Implementation of the MBDataWeatherLinkMessage class
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
#include <date/date.h>

#include "../../time_offseter.h"
#include "../../davis/vantagepro2_message.h"
#include "abstract_mbdata_message.h"
#include "mbdata_weatherlink_message.h"

namespace meteodata {

namespace asio = boost::asio;
namespace chrono = std::chrono;

MBDataWeatherlinkMessage::MBDataWeatherlinkMessage(date::sys_seconds datetime, const std::string& content, std::experimental::optional<float> previousRainfall, const TimeOffseter& timeOffseter) :
	AbstractMBDataMessage(datetime, content, timeOffseter),
	_diffRainfall(previousRainfall)
{
	using namespace date;

	const std::regex mandatoryPart{
		"^\\d+/\\d+/\\d+;\\d+:\\d+;" // date: already parsed
		"([^\\|]*)\\|" // temperature
		"([^\\|]*)\\|" // humidite
		"([^\\|]*)\\|" // dew point
		"([^\\|]*)\\|" // pressure
		"([^\\|]*)\\|" // pressure variation
		"([^\\|]*)\\|" // rainfall since 0h
		"([^\\|]*)\\|" // wind
		"([^\\|]*)\\|" // wind direction
		"([^\\|]*)\\|" // wind gusts
		"([^\\|]*)\\|" // windchill
		"([^\\|]*)\\|" // HEATINDEX
		"([^\\|]*)\\|" // Tx over 24h
		"([^\\|]*)\\|" // Tn over 24h
		"([^\\|]*)\\|" // rainrate
		"([^\\|]*)\\|?" // solar radiation
	};

	std::smatch baseMatch;
	if (std::regex_search(_content, baseMatch, mandatoryPart) && baseMatch.size() == 16) {
		for (auto&& match : baseMatch) {
			std::cerr << "match: " << match.str() << std::endl;
		}
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
		if (baseMatch[6].length() && _diffRainfall) {
			try{
				float f = std::stof(baseMatch[6].str()) - *_diffRainfall;
				if (f >= 0 && f < 100)
					_computedRainfall = f;
			} catch(std::exception&) {
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
		// skip Tx and Tn
		if (baseMatch[14].length()) {
			try {
				_rainRate = std::stof(baseMatch[14].str());
			} catch (std::exception&) {
			}
		}
		if (baseMatch[15].length()) {
			try {
				_solarRad = std::stoi(baseMatch[15].str());
			} catch (std::exception&) {
			}
		}

		_valid = true;
	}
}

void MBDataWeatherlinkMessage::populateDataPoint(const CassUuid, CassStatement* const) const
{
	// Not implemented, no one cares
}

void MBDataWeatherlinkMessage::populateV2DataPoint(const CassUuid station, CassStatement* const statement) const
{
	/*************************************************************/
	cass_statement_bind_uuid(statement, 0, station);
	/*************************************************************/
	cass_statement_bind_uint32(statement, 1,
		cass_date_from_epoch(
			date::floor<chrono::seconds>(
				_datetime
			).time_since_epoch().count()
		)
	);
	/*************************************************************/
	cass_statement_bind_int64(statement, 2,
		date::floor<chrono::milliseconds>(
			_datetime
		).time_since_epoch().count()
	);
	/*************************************************************/
	if (_pressure)
		cass_statement_bind_float(statement, 3, *_pressure);
	/*************************************************************/
	if (_dewPoint)
		cass_statement_bind_float(statement, 4, *_dewPoint);
	else if (_airTemp && _humidity)
		cass_statement_bind_float(statement, 4,
			dew_point(
				*_airTemp,
				*_humidity
			)
		);
	/*************************************************************/
	// No extra humidity
	/*************************************************************/
	// No extra temperature
	/*************************************************************/
	// Heat index is irrelevant off-shore
	/*************************************************************/
	// No inside humidity
	/*************************************************************/
	// No inside temperature
	/*************************************************************/
	// No leaf measurements
	/*************************************************************/
	if (_humidity)
		cass_statement_bind_int32(statement, 17, *_humidity);
	/*************************************************************/
	if (_airTemp)
		cass_statement_bind_float(statement, 18, *_airTemp);
	/*************************************************************/
	if (_rainRate)
		cass_statement_bind_float(statement, 19, *_rainRate);
	/*************************************************************/
	if (_computedRainfall)
		cass_statement_bind_float(statement, 20, *_computedRainfall);
	/*************************************************************/
	// No ETP
	/*************************************************************/
	// No soil moistures
	/*************************************************************/
	// No soil temperature
	/*************************************************************/
	if (_solarRad)
		cass_statement_bind_int32(statement, 30, *_solarRad);
	/*************************************************************/
	// THSW index is irrelevant
	/*************************************************************/
	// No UV
	/*************************************************************/
	// Wind chill is irrelevant
	/*************************************************************/
	if (_windDir)
		cass_statement_bind_int32(statement, 34, *_windDir);
	/*************************************************************/
	if (_gust)
		cass_statement_bind_float(statement, 35, *_gust);
	/*************************************************************/
	if (_wind)
		cass_statement_bind_float(statement, 36, *_wind);
	/*************************************************************/
	if (_solarRad) {
		bool ins = insolated(
			*_solarRad,
			_timeOffseter.getLatitude(),
			_timeOffseter.getLongitude(),
			date::floor<chrono::seconds>(_datetime).time_since_epoch().count()
		);
		cass_statement_bind_int32(statement, 37, ins ? _timeOffseter.getMeasureStep() : 0);
	}
	/*************************************************************/
}

}
