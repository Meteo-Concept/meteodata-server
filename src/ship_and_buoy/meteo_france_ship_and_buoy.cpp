/**
 * @file meteofranceshipandbuoy.cpp
 * @brief Implementation of the MeteoFranceShipAndBuoy class
 * @author Laurent Georget
 * @date 2019-01-16
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
#include <map>

#include <date.h>
#include <cassobs/message.h>

#include "meteo_france_ship_and_buoy.h"
#include "../davis/vantagepro2_message.h"

namespace meteodata
{
MeteoFranceShipAndBuoy::MeteoFranceShipAndBuoy(std::istream& entry, const std::vector<std::string>& fields)
{
	unsigned int i = 0;
	std::map<std::string, std::string> values;
	for (std::string field ; std::getline(entry, field, ';') && i < fields.size() ; i++) {
		values.emplace(std::make_pair(fields[i], field));
	}

	if (i < fields.size()) {
		_valid = false;
		return;
	}

	// numeric_sta
	_identifier = values["numer_sta"];
	// time
	std::istringstream in(values["date"]);
	in >> date::parse("%Y%m%d%H", _datetime);
	// lat
	_latitude = std::stof(values["lat"]);
	// lon
	_longitude = std::stof(values["lon"]);
	// t
	if (values["t"] != "mq")
		_airTemp = from_Kelvin_to_Celsius(std::stof(values["t"]));
	// td
	if (values["td"] != "mq")
		_dewPoint = from_Kelvin_to_Celsius(std::stof(values["td"]));
	// u
	if (values["u"] != "mq")
		_humidity = std::stoi(values["u"]);
	// dd
	if (values["dd"] != "mq")
		_windDir = std::stoi(values["dd"]);
	// ff
	if (values["ff"] != "mq")
		_wind = from_mps_to_kph(std::stof(values["ff"]));
	// pmer
	if (values["pmer"] != "mq")
		_pressure = std::stoi(values["pmer"]) / 100.0; // Pa to hPa
	// tmer
	if (values["tmer"] != "mq")
		_seaTemp = from_Kelvin_to_Celsius(std::stof(values["tmer"]));
	// HwaHwa
	if (values["HwaHwa"] != "mq")
		_seaWindHeight = std::stof(values["HwaHwa"]);
	// PwaPwa
	if (values["PwaPwa"] != "mq")
		_seaWindPeriod = std::stof(values["PwaPwa"]);
	// dwadwa
	if (values["dwadwa"] != "mq")
		_seaWindDirection = std::stof(values["dwadwa"]);
	// Hw1Hw1
	if (values["Hw1Hw1"] != "mq")
		_swellHeight1 = std::stof(values["Hw1Hw1"]);
	// Pw1Pw1
	if (values["Pw1Pw1"] != "mq")
		_swellPeriod1 = std::stof(values["Pw1Pw1"]);
	// dw1dw1
	if (values["dw1dw1"] != "mq")
		_swellDirection1 = std::stof(values["dw1dw1"]);
	// Hw2Hw2
	if (values["Hw2Hw2"] != "mq")
		_swellHeight2 = std::stof(values["Hw2Hw2"]);
	// Pw2Pw2
	if (values["Pw2Pw2"] != "mq")
		_swellPeriod2 = std::stof(values["Pw2Pw2"]);
	// dw2dw2
	if (values["dw2dw2"] != "mq")
		_swellDirection2 = std::stof(values["dw2dw2"]);
	// rafper
	if (values["rafper"] != "mq")
		_gust = std::stof(values["rafper"]);

	_valid = true;
}

Observation MeteoFranceShipAndBuoy::getObservation(const CassUuid station) const
{
	Observation result;

	result.station = station;
	result.day = date::floor<date::days>(_datetime);
	result.time = _datetime;
	result.barometer = {bool(_pressure), *_pressure};
	result.outsidehum = {bool(_humidity), *_humidity};
	result.outsidetemp = {bool(_airTemp), *_airTemp};
	if (_dewPoint) {
		result.dewpoint = {true, *_dewPoint};
	} else if (_airTemp && _humidity) {
		result.dewpoint = {true, dew_point(*_airTemp, *_humidity)};
	}
	result.winddir = {bool(_windDir), *_windDir};
	result.windgust = {bool(_gust), *_gust};
	result.windspeed = {bool(_wind), *_wind};
	return result;
}

}
