/**
 * @file wlk_message.cpp
 * @brief Implementation of the WlkMessage class
 * @author Laurent Georget
 * @date 2020-10-10
 */
/*
 * Copyright (C) 2020 JD Environnement <contact@meteo-concept.fr>
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
#include <map>
#include <chrono>

#include <date/date.h>
#include <message.h>

#include "wlk_message.h"
#include "vantagepro2_message.h"

namespace chrono = std::chrono;

namespace meteodata
{
WlkMessage::WlkMessage(std::istream& entry, const TimeOffseter& tz, const std::vector<std::string>& fields) :
	Message()
{
	unsigned int i=0;
	std::map<std::string, std::string> values;
	for (std::string field ; std::getline(entry, field, '\t') && i<fields.size() ; i++) {
		size_t start = field.find_first_not_of(' ');
		size_t end = field.find_last_not_of(" \r");
		if (start == std::string::npos) {
			values.emplace(std::make_pair(fields[i], "---"));
		} else {
			values.emplace(std::make_pair(fields[i], field.substr(start, end - start + 1)));
		}
	}

	if (i < fields.size()) {
		_valid = false;
		return;
	}

	using namespace date;

	// date
	date::year_month_day date;
	std::istringstream in(values["Date"]);
	in >> date::parse("%d/%m/%y", date);
	// time
	chrono::minutes time;
	in = std::istringstream(values["Time"]);
	in >> date::parse("%H:%M", time);

	date::local_seconds datetime = local_days(date) + time;
	_datetime = tz.convertFromLocalTime(datetime);

	if (_datetime > chrono::system_clock::now()) {
		_valid = false;
		return;
	}

	// Temp Out
	if (values["Temp Out"] != "---")
		_airTemp = std::stof(values["Temp Out"]);

	// Hi Temp
	if (values["Hi Temp"] != "---")
		_maxAirTemp = std::stof(values["Hi Temp"]);

	// Low Temp
	if (values["Low Temp"] != "---")
		_minAirTemp = std::stof(values["Low Temp"]);

	// Out Hum
	if (values["Out Hum"] != "---")
		_humidity = std::stoi(values["Out Hum"]);

	// Dew Pt.
	if (values["Dew Pt."] != "---")
		_dewPoint = std::stof(values["Dew Pt."]);

	// Wind Speed
	if (values["Wind Speed"] != "---")
		_windSpeed = std::stof(values["Wind Speed"]);

	// Wind Dir
	if (values["Wind Dir"] != "---") {
		std::string& dir = values["Wind Dir"];
		if (dir == "N")
			_windDir = 0.f;
		else if (dir == "NNE")
			_windDir = 22.5f;
		else if (dir == "NE")
			_windDir = 45.f;
		else if (dir == "ENE")
			_windDir = 67.5f;
		else if (dir == "E")
			_windDir = 90.f;
		else if (dir == "ESE")
			_windDir = 112.5f;
		else if (dir == "SE")
			_windDir = 135.f;
		else if (dir == "SSE")
			_windDir = 157.5f;
		else if (dir == "S")
			_windDir = 180.f;
		else if (dir == "SSW")
			_windDir = 202.5f;
		else if (dir == "SW")
			_windDir = 225.f;
		else if (dir == "WSW")
			_windDir = 247.5f;
		else if (dir == "W")
			_windDir = 270.f;
		else if (dir == "WNW")
			_windDir = 292.5f;
		else if (dir == "NW")
			_windDir = 315.f;
		else if (dir == "NNW")
			_windDir = 337.5f;
	}

	// Hi Speed
	if (values["Hi Speed"] != "---")
		_gust = std::stof(values["Hi Speed"]);

	// Wind Chill
	if (values["Wind Chill"] != "---")
		_windChill = std::stof(values["Wind Chill"]);

	// Heat Index
	if (values["Heat Index"] != "---")
		_heatIndex = std::stof(values["Heat Index"]);

	// Bar
	if (values["Bar"] != "---")
		_pressure = std::stof(values["Bar"]);

	// Rain
	if (values["Rain"] != "---")
		_rainfall = std::stof(values["Rain"]);

	// Rain Rate
	if (values["Rain Rate"] != "---")
		_rainrate = std::stof(values["Rain Rate"]);

	_valid = true;
}



void WlkMessage::populateDataPoint(const CassUuid, CassStatement* const) const
{
	// Let's not bother with deprecated stuff
}

void WlkMessage::populateV2DataPoint(const CassUuid station, CassStatement* const statement) const
{
	/*************************************************************/
	cass_statement_bind_uuid(statement, 0, station);
	/*************************************************************/
	cass_statement_bind_uint32(statement, 1,
		cass_date_from_epoch(
			chrono::system_clock::to_time_t(_datetime)
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
	if (_heatIndex)
		cass_statement_bind_float(statement, 10, *_heatIndex);
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
	if (_rainrate)
		cass_statement_bind_float(statement, 19, *_rainrate);
	/*************************************************************/
	if (_rainfall)
		cass_statement_bind_float(statement, 20, *_rainfall);
	/*************************************************************/
	// No ETP
	/*************************************************************/
	// No soil moistures
	/*************************************************************/
	// No soil temperature
	/*************************************************************/
	// No solar radiation
	/*************************************************************/
	// THSW index is irrelevant
	/*************************************************************/
	// No UV index
	/*************************************************************/
	if (_windChill)
		cass_statement_bind_float(statement, 33, *_windChill);
	/*************************************************************/
	if (_windDir)
		cass_statement_bind_int32(statement, 34, *_windDir);
	/*************************************************************/
	if (_gust)
		cass_statement_bind_float(statement, 35, *_gust);
	/*************************************************************/
	if (_windSpeed)
		cass_statement_bind_float(statement, 36, *_windSpeed);
	/*************************************************************/
	// No insolation
	/*************************************************************/
}

}

