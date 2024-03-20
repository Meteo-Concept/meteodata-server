/**
 * @file barani_meteoag_2022_message.cpp
 * @brief Implementation of the BaraniMeteoAg2022Message class
 * @author Laurent Georget
 * @date 2024-03-11
 */
/*
 * Copyright (C) 2024  SAS JD Environnement <contact@meteo-concept.fr>
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
#include <dbconnection_observations.h>
#include <observation.h>
#include <systemd/sd-daemon.h>

#include "barani_meteoag_2022_message.h"
#include "cassandra_utils.h"
#include "hex_parser.h"

namespace meteodata
{

namespace chrono = std::chrono;
namespace json = boost::json;

BaraniMeteoAg2022Message::BaraniMeteoAg2022Message(DbConnectionObservations& db):
	_db{db}
{}

void BaraniMeteoAg2022Message::ingest(const CassUuid& station, const std::string& payload, const date::sys_seconds& datetime)
{
	using namespace std::chrono;
	using namespace hex_parser;

	if (!validateInput(payload, 26)) {
		_obs.valid = false;
		return;
	}

	_obs.time = datetime;

	std::istringstream is{payload};

	// store the numbers on 16-bit integers to ensure the bit manipulations below never cause overflow
	std::vector<uint16_t> raw(13);
	for (int byte=0 ; byte<13 ; byte++) {
		is >> parse(raw[byte], 2, 16);
	}

	// bytes 0-7: index
	_obs.index = raw[0];
	// bytes 8-10: battery, resolution 0.15V, offset 3.2V
	uint16_t battery = (raw[1] & 0b1110'0000) >> 5;
	_obs.batteryVoltage = battery == 0b0111 ? NAN : (3.2 + battery * 0.15f);
	// bytes 11-13: selector E position
	_obs.selectorE = (raw[1] & 0b0001'1100) >> 2;
	// bytes 14-16: selector F position
	_obs.selectorF = ((raw[1] & 0b0000'0011) << 1) + ((raw[2] & 0b1000'0000) >> 7);
	// bytes 17-19: selector G position
	_obs.selectorG = (raw[2] & 0b0111'0000) >> 4;
	// bytes 20-31: sensor E1 voltage, resolution 0.80566, offset 0mV
	uint16_t sensorE1 = ((raw[2] & 0b0000'1111) << 8) + raw[3];
	_obs.sensorE1 = sensorE1 * 0.80566f;
	// bytes 32-43: sensor E2 voltage, resolution 0.80566, offset 0mV
	uint16_t sensorE2 = (raw[4] << 4) + ((raw[5] & 0b1111'0000) >> 4);
	_obs.sensorE2 = sensorE2 * 0.80566f;
	// bytes 44-55: sensor E3 voltage, resolution 0.80566, offset 0mV
	uint16_t sensorE3 = ((raw[5] & 0b0000'1111) << 8) + raw[6];
	_obs.sensorE3 = sensorE3 * 0.80566f;
	// bytes 56-67: sensor F1 voltage, resolution 0.80566, offset 0mV
	uint16_t sensorF1 = (raw[7] << 4) + ((raw[8] & 0b1111'0000) >> 4);
	_obs.sensorF1 = sensorF1 * 0.80566f;
	// bytes 68-79: sensor F2 voltage, resolution 0.80566, offset 0mV
	uint16_t sensorF2 = ((raw[8] & 0b0000'1111) << 8) + raw[9];
	_obs.sensorF2 = sensorF2 * 0.80566f;
	// bytes 80-91: sensor F3 voltage, resolution 0.80566, offset 0mV
	uint16_t sensorF3 = (raw[10] << 4) + ((raw[11] & 0b1111'0000) >> 4);
	_obs.sensorF3 = sensorF3 * 0.80566f;
	// bytes 92-103: sensor G voltage, resolution 0.80566, offset 0mV
	uint16_t sensorG1 = ((raw[11] & 0b0000'1111) << 8) + raw[12];
	_obs.sensorG1 = sensorG1 * 0.80566f;


	_obs.valid = true;
	if ((_obs.selectorE >= 3 && _obs.selectorE <= 5) || _obs.selectorE > 7) {
		_obs.valid = false;
	}
	if ((_obs.selectorF >= 3 && _obs.selectorF <= 6) || _obs.selectorF > 7) {
		_obs.valid = false;
	}
	if ((_obs.selectorG >= 4 && _obs.selectorG <= 5) || _obs.selectorG > 7) {
		_obs.valid = false;
	}
	if (_obs.selectorE == 7 && _obs.selectorF == 7) {
		_obs.valid = false;
	}
}

Observation BaraniMeteoAg2022Message::getObservation(const CassUuid& station) const
{
	Observation result;

	if (_obs.valid) {
		result.station = station;
		result.day = date::floor<date::days>(_obs.time);
		result.time = _obs.time;

		if (_obs.selectorF == 2) {
			result.soiltemp10cm = result.extratemp[0] = parse6470(_obs.sensorF1);
			result.soiltemp20cm = result.extratemp[1] = parse6470(_obs.sensorF2);
			result.soiltemp30cm = result.extratemp[2] = parse6470(_obs.sensorF3);
			if (_obs.selectorE == 7) {
				result.soiltemp40cm = parse6470(_obs.sensorE1);
				result.soiltemp50cm = parse6470(_obs.sensorE2);
				result.soiltemp60cm = parse6470(_obs.sensorE3);
			}
			// TODO: other probes are not supported yet
		}

		if (_obs.selectorE == 0) {
			float temp = result.soiltemp10cm.first ? result.soiltemp10cm.first : 24.f;
			result.soilmoistures10cm = result.soilmoistures[0] = parseSS200(_obs.sensorE1, temp);
			temp = result.soiltemp20cm.first ? result.soiltemp20cm.first : 24.f;
			result.soilmoistures20cm = result.soilmoistures[1] = parseSS200(_obs.sensorE2, temp);
			temp = result.soiltemp30cm.first ? result.soiltemp30cm.first : 24.f;
			result.soilmoistures30cm = result.soilmoistures[2] = parseSS200(_obs.sensorE3, temp);
			if (_obs.selectorF == 7) {
				temp = result.soiltemp10cm.first ? result.soiltemp10cm.first : 24.f;
				result.soilmoistures40cm = parseSS200(_obs.sensorF1, temp);
				result.soilmoistures50cm = parseSS200(_obs.sensorF2, temp);
				result.soilmoistures60cm = parseSS200(_obs.sensorF3, temp);
			}
			// TODO: other probes are not supported yet
		}
	}

	return result;
}

std::pair<bool, float> BaraniMeteoAg2022Message::parseSS200(float v, float temp)
{
	float r0  =  (15345000. / v) - 5120;
	if (r0 < 550)
	      return { true, 0 };
	else if (r0 < 1000)
		return { true, ((r0/1000.00) * 23.156 - 12.736) * -(1 + 0.018 * (temp - 24)) };
	else if (r0 < 8000)
		return { true, (3.213 * (r0/1000.00) + 4.093) / (1-0.009433 * (r0/1000.00) - 0.01205*temp) };
	else
		return { true, 2.246 + 5.239 * (r0/1000.00) * (1+0.018*(temp-24.00))+0.06756*(r0/1000.00)*(r0/1000.00)*((1.00+0.018*(temp-24.00))*(1.00+0.08*(temp-24.00))) };
}

std::pair<bool, float> BaraniMeteoAg2022Message::parse6470(float v)
{
	if (v != 0) {
		float lr0  =  std::log(15345000. / v - 5120);
		return { true, -273.15 + 1 / (1.140e-3 + 2.320e-4 * lr0 + 9.860e-8*std::pow(lr0, 3)) };
	} else {
		return { false, 0.f };
	}
}

json::object BaraniMeteoAg2022Message::getDecodedMessage() const
{
	return json::object{
		{ "model", "barani_meteoag_20240311" },
		{ "value", {
			{ "index", _obs.index },
			{ "battery_voltage", _obs.batteryVoltage },
			{ "selectorE", _obs.selectorE },
			{ "selectorF", _obs.selectorF },
			{ "selectorG", _obs.selectorG },
			{ "sensorE1", _obs.sensorE1 },
			{ "sensorE2", _obs.sensorE2 },
			{ "sensorE3", _obs.sensorE3 },
			{ "sensorF1", _obs.sensorF1 },
			{ "sensorF2", _obs.sensorF2 },
			{ "sensorF3", _obs.sensorF3 },
			{ "sensorG1", _obs.sensorG1 },
		} }
	};
}

}
