/**
 * @file thlora_thermohygrometer_message.cpp
 * @brief Implementation of the OserenSoilStationMessage class
 * @author Laurent Georget
 * @date 2022-10-04
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
#include <sstream>
#include <vector>
#include <cmath>

#include <boost/json.hpp>
#include <cassandra.h>
#include <cassobs/observation.h>

#include "oseren_soil_station_message.h"
#include "hex_parser.h"
#include "davis/vantagepro2_message.h"

namespace meteodata
{

namespace chrono = std::chrono;
namespace json = boost::json;

void OserenSoilStationMessage::ingest(const CassUuid&, const std::string& payload, const date::sys_seconds& datetime)
{
	using namespace hex_parser;
	using namespace date;

	if (!validateInput(payload, 18)) {
		_obs.valid = false;
		return;
	}

	_obs.time = datetime;

	// parse and fill in the obs
	_obs.valid = true;

	std::istringstream is{payload};

	std::vector<uint32_t> raw(21);
	for (int byte=0 ; byte<21 ; byte++) {
		is >> parse(raw[byte], 4, 16);
	}

	// word 0 (0-4): header
	_obs.header = raw[0];

	// word 1 (5-8): year
	// word 2 (9-12): month
	// word 3 (13-16): day
	// word 4 (17-20): hours
	// word 5 (21-24): minutes
	_obs.time = date::sys_days{date::year_month_day{date::year{int(raw[1])}/raw[2]/raw[3]}}
		+ chrono::hours{raw[4]} + chrono::minutes{raw[5]};

	// word 6 (25-28): air temperature
	_obs.temperature = raw[6] / 100.f;

	// word 7 (29-32): air relative humidity
	_obs.humidity = raw[7];

	// word 8 (33-36): atmospheric pressure
	_obs.pressure = raw[8];

	// word 9 (37-40): rainfall
	_obs.rainfall = raw[9] / 10.f;

	// word 10 (41-44): windspeed
	_obs.windspeed = raw[10] * 3.6f / 100.f;

	// word 11 (45-48): winddir
	_obs.winddir = raw[11];

	// word 12 (49-52): VWC 10cm
	_obs.soilVWC10 = raw[12] / 100.f;

	// word 13 (53-56): soil temperature 10cm
	_obs.soilTemp10 = raw[13] / 100.f;

	// word 14 (57-60): VWC 50cm
	_obs.soilVWC50 = raw[14] / 100.f;

	// word 15 (61-64): soil temperature 10cm
	_obs.soilTemp50 = raw[15] / 100.f;

	// word 16 (65-68): VWC 50cm
	_obs.soilVWC100 = raw[16] / 100.f;

	// word 17 (69-72): soil temperature 10cm
	_obs.soilTemp100 = raw[17] / 100.f;

	// word 18 (73-76): enclosure temperature
	_obs.enclosureTemp = raw[18] / 100.f;

	// word 19 (77-80): battery
	_obs.battery = raw[19] / 100.f;

	// word 20 (81-83): enclosure relative humidity
	_obs.enclosureHum = raw[20];
}

Observation OserenSoilStationMessage::getObservation(const CassUuid& station) const
{
	Observation result;

	if (_obs.valid) {
		result.station = station;
		result.day = date::floor<date::days>(_obs.time);
		result.time = _obs.time;
		result.outsidetemp = {true, _obs.temperature};
		result.outsidehum = {true, _obs.humidity};
		result.dewpoint = {true, dew_point(_obs.temperature, _obs.humidity)};
		result.heatindex = {true, heat_index(from_Celsius_to_Farenheit(_obs.temperature), _obs.humidity)};
		result.barometer = {true, _obs.pressure};
		result.rainfall = {true, _obs.rainfall};
		result.windspeed = {true, _obs.windspeed};
		result.winddir = {true, _obs.winddir};
		result.soiltemp[0] = {true, _obs.soilTemp10};
		result.soiltemp[1] = {true, _obs.soilTemp50};
		result.soiltemp[2] = {true, _obs.soilTemp100};
		result.soilmoistures[0] = {true, _obs.soilVWC10};
		result.soilmoistures[1] = {true, _obs.soilVWC50};
		result.soilmoistures[2] = {true, _obs.soilVWC100};
		result.insidetemp = {true, _obs.enclosureTemp};
		result.insidehum = {true, _obs.enclosureHum};
		result.voltage_battery = {true, _obs.battery};
	}

	return result;
}

json::object OserenSoilStationMessage::getDecodedMessage() const
{
	return json::object{
		{ "model", "oseren_soil_station_20250709" },
		{ "value", {
			{ "header", _obs.header },
			{ "temperature", _obs.temperature },
			{ "humidity", _obs.humidity },
			{ "atmospheric_pressure", _obs.pressure },
			{ "rainfall", _obs.rainfall },
			{ "wind_speed", _obs.windspeed },
			{ "wind_direction", _obs.winddir },
			{ "soil_temperature_10cm", _obs.soilTemp10 },
			{ "soil_temperature_50cm", _obs.soilTemp50 },
			{ "soil_temperature_100cm", _obs.soilTemp100 },
			{ "soil_vwc_10cm", _obs.soilVWC10 },
			{ "soil_vwc_50cm", _obs.soilVWC50 },
			{ "soil_vwc_100cm", _obs.soilVWC100 },
			{ "enclosure_temperature", _obs.enclosureTemp },
			{ "enclosure_rh", _obs.enclosureHum },
			{ "battery", _obs.battery }
		} }
	};
}

}
