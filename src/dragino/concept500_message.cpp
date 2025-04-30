/**
 * @file concept500_message.cpp
 * @brief Implementation of the Concept500Message class
 * @author Laurent Georget
 * @date 2025-04-30
 */
/*
 * Copyright (C) 2025  SAS JD Environnement <contact@meteo-concept.fr>
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

#include <string>
#include <cmath>

#include <systemd/sd-daemon.h>
#include <cassandra.h>
#include <cassobs/observation.h>
#include <boost/json.hpp>

#include "concept500_message.h"
#include "hex_parser.h"
#include "cassandra_utils.h"
#include "davis/vantagepro2_message.h"

namespace meteodata
{

namespace chrono = std::chrono;
namespace json = boost::json;

Concept500Message::Concept500Message(DbConnectionObservations& db):
	_db{db}
{}

void Concept500Message::ingest(const CassUuid& station, const std::string& payload, const date::sys_seconds& datetime)
{
	using namespace hex_parser;

	if (!validateInput(payload, 24)) {
		_obs.valid = false;
		return;
	}

	_obs.time = datetime;
	uint16_t battery;
	uint16_t temp;
	uint16_t hum;
	uint16_t windPulses = 0U;
	uint8_t gustPulses = 0U;
	uint8_t minPulses = 0U;
	uint16_t windDir = 0xFFFFU;

	std::istringstream is{payload};
	is >> parse(battery, 4, 16)
	   >> parse(temp, 4, 16)
	   >> parse(hum, 4, 16)
	   >> parse(windPulses, 4, 16)
	   >> parse(gustPulses, 2, 16)
	   >> parse(minPulses, 2, 16)
	   >> parse(windDir, 4, 16);

	_obs.battery = float(battery) / 1000;

	_obs.humidity = float(hum) / 10;
	if (temp == 0xFFFF && hum == 0xFFFF) {
		_obs.temperature = NAN;
		_obs.humidity = NAN;
	} else if ((temp & 0x8000) == 0) {
		_obs.temperature = float(temp) / 10;
	} else {
		_obs.temperature = (float(temp) - 65536) / 10;
	}


	float latitude, longitude;
	int elevation;
	int pollingPeriod;
	std::string name;
	bool res = _db.getStationCoordinates(station, latitude, longitude, elevation, name, pollingPeriod);
	if (!res) {
		std::cerr << SD_ERR << "[MQTT " << station << "] management: "
			  << "Couldn't get the polling period of the station, assuming 10 minutes"
			  << std::endl;
		pollingPeriod = 10;
	}
	_obs.windSpeed = from_mph_to_kph(windPulses * 2.25 / (pollingPeriod * 60));
	_obs.gustSpeed = from_mph_to_kph(gustPulses);
	_obs.minSpeed = from_mph_to_kph(gustPulses);
	if (windDir != 0xFFFF) {
		_obs.windDir = windDir % 360;
	}

	_obs.valid = true;
}

Observation Concept500Message::getObservation(const CassUuid& station) const
{
	Observation obs;
	obs.station = station;
	obs.day = date::floor<date::days>(_obs.time);
	obs.time = _obs.time;
	obs.outsidetemp = {!std::isnan(_obs.temperature), _obs.temperature};
	obs.outsidehum = {!std::isnan(_obs.humidity), int(std::round(_obs.humidity))};
	if (!std::isnan(_obs.temperature) && !std::isnan(_obs.humidity)) {
		obs.dewpoint = {true, dew_point(_obs.temperature, _obs.humidity)};
		obs.heatindex = {true, heat_index(from_Celsius_to_Farenheit(_obs.temperature), _obs.humidity)};
	}
	obs.windspeed = {!std::isnan(_obs.windSpeed), _obs.windSpeed};
	obs.windgust = {!std::isnan(_obs.gustSpeed), _obs.gustSpeed};
	obs.min_windspeed = {!std::isnan(_obs.minSpeed), _obs.minSpeed};
	obs.winddir = {!std::isnan(_obs.windDir), int(std::round(_obs.windDir))};
	obs.voltage_battery = {!std::isnan(_obs.battery), _obs.battery};
	return obs;
}

json::object Concept500Message::getDecodedMessage() const
{
	return json::object{
		{ "model", "CONCEPT_500-20250430" },
		{ "value", {
			{ "battery", _obs.battery },
			{ "temperature", _obs.temperature },
			{ "humidity", _obs.humidity },
			{ "wind_speed", _obs.windSpeed },
			{ "wind_gust", _obs.gustSpeed },
			{ "wind_min", _obs.minSpeed },
			{ "wind_direction", _obs.windDir },
		} }
	};
}

}
