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
#include <stdexcept>

#include <date/date.h>
#include <message.h>

#include "../time_offseter.h"
#include "../davis/vantagepro2_message.h"
#include "static_message.h"

namespace meteodata
{
StatICMessage::StatICMessage(std::istream& file, const TimeOffseter& timeOffseter) :
		_valid(false),
		_timeOffseter(timeOffseter)
{
	using namespace date;
	std::string st;
	date::sys_seconds date;
	chrono::seconds hour;
	bool hasDate = false;
	bool hasHour = false;
	const std::regex normalLine{"\\s*([^#=]+)=(\\s?\\S*)\\s*"};
	const std::regex dateRegex{"(\\d\\d).(\\d\\d).(\\d?\\d?\\d\\d).*"};
	const std::regex timeRegex{"([0-9 ]\\d).(\\d\\d).*"};
	int year = 0, month = 0, day = 0, h = 0, min = 0;

	while (std::getline(file, st)) {
		std::smatch baseMatch;
		if (std::regex_match(st, baseMatch, normalLine) && baseMatch.size() == 3) {
			std::string var = baseMatch[1].str();
			std::string value = baseMatch[2].str();
			// empty values are equal to zero, but it can be zero int or zero float so we leave the conversion for later
			if (value.empty() || value == "Néant")
				value = "0";

			try {
				if (var == "date_releve") {
					std::smatch dateMatch;
					if (std::regex_match(value, dateMatch, dateRegex) && dateMatch.size() == 4) {
						year = std::atoi(dateMatch[3].str().data());
						if (year < 100)
							year += 2000; // I hope this code will not survive year 2100...
						month = std::atoi(dateMatch[2].str().data());
						day = std::atoi(dateMatch[1].str().data());
						hasDate = true;
					}
				} else if (var == "heure_releve_utc" && value.size() >= 5) {
					std::smatch timeMatch;
					if (std::regex_match(value, timeMatch, timeRegex) && timeMatch.size() >= 3) {
						h = std::atoi(timeMatch[1].str().data());
						min = std::atoi(timeMatch[2].str().data());
						hasHour = true;
					}
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
				} else if (var == "pluie_cumul") {
					_dayRainfall = std::stof(value);
				} else if (var == "radiations_solaires_wlk") {
					_solarRad = std::stoi(value);
				} else if (var == "uv_wlk") {
					_uv = std::stoi(value);
				}
			} catch (const std::invalid_argument&) { // let invalid conversions fail silently
			}
		}
	}

	if (hasDate && hasHour) {
		_valid = true;
		_datetime = date::floor<chrono::seconds>(_timeOffseter.convertFromLocalTime(day, month, year, h, min));
	}
}


void StatICMessage::computeRainfall(float previousHourRainfall, float previousDayRainfall)
{
	if (_dayRainfall && !_computedRainfall) {
		_computedRainfall = *_dayRainfall - previousDayRainfall;
		if (*_computedRainfall < 0)
			_computedRainfall = 0;
	}
	if (_hourRainfall && !_computedRainfall) {
		_computedRainfall = *_hourRainfall - previousHourRainfall;
		if (*_computedRainfall < 0)
			_computedRainfall = 0;
	}
}

Observation StatICMessage::getObservation(const CassUuid station) const
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
	result.rainrate = {bool(_rainRate), *_rainRate};
	result.rainfall = {bool(_computedRainfall), *_computedRainfall};
	result.winddir = {bool(_windDir), *_windDir};
	result.windgust = {bool(_gust), *_gust};
	result.windspeed = {bool(_wind), *_wind};
	result.solarrad = {bool(_solarRad), *_solarRad};
	result.uv = {bool(_uv), *_uv};
	if (_solarRad) {
		bool ins = insolated(*_solarRad, _timeOffseter.getLatitude(), _timeOffseter.getLongitude(),
							 date::floor<chrono::seconds>(_datetime).time_since_epoch().count());
		result.insolation_time = {true, ins ? _timeOffseter.getMeasureStep() : 0};
	}

	return result;
}

}
