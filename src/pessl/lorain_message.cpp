/**
 * @file lorain_message.cpp
 * @brief Implementation of the LorainMessage class
 * @author Laurent Georget
 * @date 2022-03-24
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

#include <iostream>
#include <string>
#include <sstream>
#include <optional>
#include <algorithm>
#include <systemd/sd-daemon.h>
#include <cmath>

#include <cassandra.h>
#include <observation.h>
#include <dbconnection_observations.h>

#include "lorain_message.h"
#include "../hex_parser.h"

namespace meteodata
{

namespace chrono = std::chrono;


void LorainMessage::ingest(const std::string& payload, const date::sys_seconds& datetime, std::optional<int> lastRainfallClicks)
{
	using namespace hex_parser;

	if (payload.length() != 94) {
		_obs.valid = false;
		std::cerr << SD_ERR << "[MQTT Lorain] protocol: "
			  << "Invalid size " << payload.length() << " for payload " << payload << ", should be 94"
			  << std::endl;
		return;
	}

	if (!std::all_of(payload.cbegin(), payload.cend(),
		[](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); })) {
		std::cerr << SD_ERR << "[MQTT Lorain] protocol: "
			  << "Payload " << payload << " contains invalid characters"
			  << std::endl;
		_obs.valid = false;
		return;
	}


	_obs.time = datetime;

	// parse and fill in the obs
	_obs.valid = true;

	std::istringstream is{payload};

	// TODO: WRONG! this device uses little-endian so the parsing is wrong

	int tm, tn, tx;
	int rhm, rhn, rhx;
	int deltaTm, deltaTn, deltaTx;
	int d, dn;
	int vp, vpn;
	int l;
	is >> ignore(28)
	   >> parseLE(_obs.batteryVoltage, 4, 16)
	   >> parseLE(_obs.solarPanelVoltage, 4, 16)
	   >> parseLE(_obs.rainfallClicks, 4, 16)
	   >> parseLE(tm, 4, 16)
	   >> parseLE(tn, 4, 16)
	   >> parseLE(tx, 4, 16)
	   >> parseLE(rhm, 4, 16)
	   >> parseLE(rhn, 4, 16)
	   >> parseLE(rhx, 4, 16)
	   >> parseLE(deltaTm, 4, 16)
	   >> parseLE(deltaTn, 4, 16)
	   >> parseLE(deltaTx, 4, 16)
	   >> parseLE(d, 4, 16)
	   >> parseLE(dn, 4, 16)
	   >> parseLE(vp, 4, 16)
	   >> parseLE(vpn, 4, 16)
	   >> parseLE(l, 2, 16);

	_obs.temperature = tm / 100.f;
	_obs.maxTemperature = tx / 100.f;
	_obs.minTemperature = tn / 100.f;

	_obs.humidity = rhm / 10.f;
	_obs.maxHumidity = rhx / 10.f;
	_obs.minHumidity = rhn / 10.f;

	_obs.deltaT = deltaTm / 100.f;
	_obs.maxDeltaT = deltaTx / 100.f;
	_obs.minDeltaT = deltaTn / 100.f;

	_obs.dewPoint = d / 100.f;
	_obs.minDewPoint = dn / 100.f;

	_obs.vaporPressureDeficit = vp / 100.f;
	_obs.minVaporPressureDeficit = vpn / 100.f;

	_obs.leafWetnessTimeRatio = l;

	if (lastRainfallClicks) {
		int prev = *lastRainfallClicks;
		if (_obs.rainfallClicks < prev) {
			// overflow
			_obs.rainfall = (_obs.rainfallClicks + 0xffff - prev) * 0.2;
		} else {
			_obs.rainfall = (_obs.rainfallClicks - prev) * 0.2;
		}
	} else {
		_obs.rainfall = NAN;
	}
}

Observation LorainMessage::getObservation(const CassUuid station) const
{
	Observation result;

	if (_obs.valid) {
		result.station = station;
		result.day = date::floor<date::days>(_obs.time);
		result.time = _obs.time;
		result.dewpoint = { !std::isnan(_obs.dewPoint), _obs.dewPoint };
		result.outsidehum = { true, std::round(_obs.humidity) };
		result.outsidetemp = { !std::isnan(_obs.temperature), _obs.temperature };
		result.min_outside_temperature = { !std::isnan(_obs.minTemperature), _obs.minTemperature };
		result.max_outside_temperature = { !std::isnan(_obs.maxTemperature), _obs.maxTemperature };
		result.rainfall = { !std::isnan(_obs.rainfall), _obs.rainfall };
		result.leafwetness_timeratio1 = { true, _obs.leafWetnessTimeRatio };
	}

	return result;
}

}
