/**
 * @file oy1110_thermohygrometer_message.cpp
 * @brief Implementation of the Oy1110ThermohygrometerMessage class
 * @author Laurent Georget
 * @date 2022-10-06
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

#include <systemd/sd-daemon.h>
#include <cassandra.h>
#include <observation.h>

#include "oy1110_thermohygrometer_message.h"
#include "hex_parser.h"

namespace meteodata
{

namespace chrono = std::chrono;

Oy1110ThermohygrometerMessage::Oy1110ThermohygrometerMessage(const CassUuid& station) :
	_station{station}
{}

bool Oy1110ThermohygrometerMessage::validateInput(const std::string& payload)
{
	if (payload.length() != 6 && (payload.length() - 1) % 6 == 0) {
		std::cerr << SD_ERR << "[MQTT Liveobjects] protocol: " << "Invalid size " << payload.length() << " for payload "
				  << payload << ", should be either a 3-byte packet or a 1-byte header followed by 3-byte packets" << std::endl;
		return false;
	}

	if (!std::all_of(payload.cbegin(), payload.cend(), [](char c) {
		return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
	})) {
		std::cerr << SD_ERR << "[MQTT Liveobjects] protocol: " << "Payload " << payload
				  << " contains invalid characters" << std::endl;
		return false;
	}

	return true;
}

void Oy1110ThermohygrometerMessage::ingest(const CassUuid&, const std::string& payload, const date::sys_seconds& datetime)
{
	using namespace hex_parser;

	if (!validateInput(payload)) {
		_obs.valid = false;
		return;
	}

	_obs.basetime = datetime;

	std::istringstream is{payload};

	std::size_t length = payload.length();
	if (length > 6) {
		uint8_t header;
		is >> parse(header, 2, 16);
		uint8_t minOrHour = header & 0b1000'0000;
		uint8_t time = header & 0b0111'1111;
		_obs.offset = minOrHour == 0 ? chrono::minutes{time} : chrono::hours{time};
		length--;
	}
	while (length > 0) {
		uint8_t temp1, temp2;
		uint8_t hum1, hum2;
		is >> parse(temp1, 2, 16)
		   >> parse(hum1, 2, 16)
		   >> parse(temp2, 1, 16)
		   >> parse(hum2, 1, 16);

		uint16_t temp = (temp1 << 4) + temp2;
		uint16_t hum = (hum1 << 4) + hum2;
		_obs.temperatures.push_back(float(static_cast<int16_t>(temp - 800u)) / 10.f);
		_obs.humidities.push_back(float(hum - 250) / 10.f);
		length -= 6;
	}

	_obs.valid = true;
}

Observation Oy1110ThermohygrometerMessage::getObservation(const CassUuid& station) const
{
	// return only the first (= most recent) observation in the message if it's a group of measurements
	const_iterator it = begin();

	Observation obs = *it;
	obs.station = station;
	return obs;
}

}
