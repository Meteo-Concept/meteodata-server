/**
 * @file staticmessage.cpp
 * @brief Implementation of the StatICMessage class
 * @author Laurent Georget
 * @date 2019-02-06
 */
/*
 * Copyright (C) 2019 JD Environnement <contact@meteo-concept.fr>
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

#include <date/date.h>
#include <message.h>

#include "staticmessage.h"
#include "vantagepro2message.h"
#include "timeoffseter.h"

namespace meteodata
{
StatICMessage::StatICMessage(std::istream& file, std::experimental::optional<float> previousRainfall, const TimeOffseter& timeOffseter) :
	Message(),
	_previousRainfall(previousRainfall),
	_timeOffseter(timeOffseter)
{
	using namespace date;
	std::string st;
	date::sys_seconds date;
	chrono::seconds hour;
	bool hasDate = false;
	bool hasHour = false;
	const std::regex normalLine{"\\s*([^#=]+)=(\\S*)"};
	std::istringstream dateValue;

	while (std::getline(file, st)) {
		std::smatch baseMatch;
		if (std::regex_match(st, baseMatch, normalLine) && baseMatch.size() == 3) {
			std::string var = baseMatch[1].str();
			std::string value = baseMatch[2].str();
			// empty values are equal to zero, but it can be zero int or zero float so we leave the conversion for later
			if (value == "")
				value = "0";

			if (var == "date_releve") {
				dateValue.clear();
				dateValue.str(value);
				dateValue >> parse("%d/%m/%Y", date);
				hasDate = true;
			} else if (var == "heure_releve_utc" && value.size() == 5) {
				hour = chrono::hours(std::atoi(value.data())) + chrono::minutes(std::atoi(value.data() + 3));
				hasHour = true;
			} else if (var == "temperature") {
				_airTemp = std::stof(value);
			} else if (var == "pression") {
				_pressure = std::stof(value);
			} else if (var == "humidite") {
				_humidity = std::stoi(value);
			} else if (var == "point_de_rosee") {
				_dewPoint = std::stof(value);
			} else if (var == "vent_dir_moy") {
				_windDir = std::stoi(value);
			} else if (var == "vent_moyen") {
				_wind = std::stof(value);
			} else if (var == "vent_rafales") {
				_gust = std::stof(value);
			} else if (var == "pluie_intensite") {
				_rainRate = std::stof(value);
			} else if (var == "pluie_cumul_1h") {
				_hourRainfall = std::stof(value);
				if (_previousRainfall) {
					_computedRainfall = *_hourRainfall - *_previousRainfall;
					if (*_computedRainfall < 0)
						_computedRainfall = *_hourRainfall;
				}
			} else if (var == "radiations_solaires_wlk") {
				_solarRad = std::stoi(value);
			} else if (var == "uv_wlk") {
				_uv = std::stoi(value);
			}
		}
	}

	if (hasDate && hasHour) {
		_valid = true;
		_datetime = date + hour;
	}
}



void StatICMessage::populateDataPoint(const CassUuid station, CassStatement* const statement) const
{
	// Let's not bother with deprecated stuff
}

void StatICMessage::populateV2DataPoint(const CassUuid station, CassStatement* const statement) const
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
	if (_uv)
		cass_statement_bind_int32(statement, 32, *_uv);
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
