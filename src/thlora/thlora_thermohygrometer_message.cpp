/**
 * @file thlora_thermohygrometer_message.cpp
 * @brief Implementation of the ThloraThermohygrometerMessage class
 * @author Laurent Georget
 * @date 2022-10-04
 */
/*
 * Copyright (C) 2022  SAS JD Environnement <contact@meteo-concept.fr>
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

#include <cassandra.h>
#include <observation.h>

#include "thlora_thermohygrometer_message.h"
#include "hex_parser.h"

namespace meteodata
{

namespace chrono = std::chrono;


void ThloraThermohygrometerMessage::ingest(const CassUuid&, const std::string& payload, const date::sys_seconds& datetime)
{
	using namespace hex_parser;

	if (!validateInput(payload, 18)) {
		_obs.valid = false;
		return;
	}

	_obs.time = datetime;

	// parse and fill in the obs
	_obs.valid = true;

	std::istringstream is{payload};

	std::vector<uint16_t> raw(9);
	for (int byte=0 ; byte<9 ; byte++) {
		is >> parse(raw[byte], 2, 16);
	}

	// bytes 0-7: header
	_obs.header = raw[0];
	// bytes 8-23: temperature, 16 bits, little endian
	uint32_t temperature = raw[1] + (raw[2] << 8);
	_obs.temperature = (175.72 * temperature) / (1 << 16) - 46.85;
	// bytes 24-31: humidity
	uint16_t humidity = raw[3];
	_obs.humidity = (125 * humidity) / (1 << 8) - 6;
	// bytes 32-39: period of measurement, 16 bits, little endian
	uint16_t period = raw[4] + (raw[5] << 8);
	_obs.period = period * 2;
	// byte 40-47: rssi
	uint16_t rssi = raw[6];
	if (raw[6] == 0xFF)
		_obs.rssi = -180;
	else
		_obs.rssi = -180 + rssi;
	// byte 48-55: snr, signed (2's-complement)
	uint16_t snr = raw[7];
	if (snr >= 0xF0)
		_obs.snr = - (0xFF - snr + 1) / 4.f;
	else
		_obs.snr = snr / 4.f;
	// byte 56-63: battery, in unit of 0.01V
	uint16_t battery = raw[8];
	_obs.battery = (battery + 150) * 0.01f;
}

Observation ThloraThermohygrometerMessage::getObservation(const CassUuid& station) const
{
	Observation result;

	if (_obs.valid) {
		result.station = station;
		result.day = date::floor<date::days>(_obs.time);
		result.time = _obs.time;
		result.outsidetemp = { true, _obs.temperature };
		result.outsidehum = { true, int(_obs.humidity) };
	}

	return result;
}

}
