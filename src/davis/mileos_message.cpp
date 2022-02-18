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
#include <observation.h>

#include "mileos_message.h"
#include "vantagepro2_message.h"

namespace chrono = std::chrono;

namespace meteodata
{
MileosMessage::MileosMessage(std::istream& entry, const TimeOffseter& tz, const std::vector<std::string>& fields)
{
	unsigned int i = 0;
	std::map<std::string, std::string> values;
	for (std::string field ; std::getline(entry, field, ';') && i < fields.size() ; i++) {
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

Observation MileosMessage::getObservation(const CassUuid station) const
{
	Observation result;

	result.station = station;
	result.day = date::floor<date::days>(_datetime);
	result.time = date::floor<chrono::seconds>(_datetime);
	result.barometer = {bool(_pressure), *_pressure};
	if (_dewPoint) {
		result.dewpoint = {true, *_dewPoint};
	} else if (_airTemp && _humidity) {
		result.dewpoint = {true, dew_point(*_airTemp, *_humidity)};
	}
	result.outsidehum = {bool(_humidity), *_humidity};
	result.outsidetemp = {bool(_airTemp), *_airTemp};
	result.rainrate = {bool(_rainrate), *_rainrate};
	result.rainfall = {bool(_rainfall), *_rainfall};
	result.winddir = {bool(_windDir), *_windDir};
	result.windgust = {bool(_gust), *_gust};
	result.windspeed = {bool(_windSpeed), *_windSpeed};

	return result;
}

}

