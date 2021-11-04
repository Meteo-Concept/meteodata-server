/**
 * @file mileos_message.cpp
 * @brief Implementation of the MileosMessage class
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
#include <systemd/sd-daemon.h>

#include <date/date.h>
#include <message.h>

#include "mileos_message.h"
#include "vantagepro2_message.h"

namespace chrono = std::chrono;

namespace meteodata
{
MileosMessage::MileosMessage(std::istream& entry, const TimeOffseter& tz, const std::vector<std::string>& fields) :
	Message()
{
	unsigned int i=0;
	std::map<std::string, std::string> values;
	for (std::string field ; std::getline(entry, field, ';') && i<fields.size() ; i++) {
		size_t start = field.find_first_not_of(' ');
		size_t end = field.find_last_not_of(" \r");
		if (start == std::string::npos) {
			values.emplace(std::make_pair(fields[i], "--"));
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
	std::istringstream in(values["jour"]);
	in >> date::parse("%d/%m/%Y", date);
	// time
	chrono::minutes time;
	in = std::istringstream(values["heure"]);
	in >> date::parse("%H:%M:%S", time);

	date::local_seconds datetime = local_days(date) + time;
	_datetime = tz.convertFromLocalTime(datetime);

	if (_datetime > chrono::system_clock::now()) {
		_valid = false;
		return;
	}

	// Temp Out
	if (values["T"] != "--")
		_airTemp = std::stof(values["T"]);

	// Hi Temp
	if (values["TX"] != "--")
		_maxAirTemp = std::stof(values["TX"]);

	// Low Temp
	if (values["TN"] != "--")
		_minAirTemp = std::stof(values["TN"]);

	// Out Hum
	if (values["U"] != "--")
		_humidity = std::stoi(values["U"]);

	// Dew Pt.
	if (values["TD"] != "--")
		_dewPoint = std::stof(values["TD"]);

	// Wind Speed
	if (values["VT"] != "--")
		_windSpeed = std::stof(values["VT"]);

	// Wind Dir
	if (values["GI"] != "--") {
		std::string& dir = values["GI"];
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
	if (values["VX"] != "--")
		_gust = std::stof(values["VX"]);

	// Bar
	if (values["P"] != "--")
		_pressure = std::stof(values["P"]);

	// Rain
	if (values["RR"] != "--")
		_rainfall = std::stof(values["RR"]);

	// Rain Rate
	if (values["RRX"] != "--")
		_rainrate = std::stof(values["RRX"]);

	_valid = true;
}



void MileosMessage::populateDataPoint(const CassUuid, CassStatement* const) const
{
	// Let's not bother with deprecated stuff
}

void MileosMessage::populateV2DataPoint(const CassUuid station, CassStatement* const statement) const
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
	// No heat index
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
	// No wind chill
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

