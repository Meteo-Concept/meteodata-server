/**
 * @file mf_radome_message.cpp
 * @brief Implementation of the MfRadomeMessage class
 * @author Laurent Georget
 * @date 2024-01-15
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

#include <iostream>
#include <string>

#include <boost/property_tree/ptree.hpp>
#include <observation.h>

#include "meteo_france/mf_radome_message.h"
#include "cassandra_utils.h"
#include "davis/vantagepro2_message.h"

namespace meteodata
{

void MfRadomeMessage::parse(boost::property_tree::ptree&& json, date::sys_seconds& timestamp)
{
	auto t = json.get<std::string>("validity_time");
	std::istringstream is{t};
	is >> date::parse("%Y-%m-%dT%H:%M:%S", _timestamp);

	_valid = bool(is);

	if (_valid) {
		_mfId = json.get<std::string>("geo_id_insee");
		_rr1 = json.get_optional<float>("rr1");
		_ff = json.get_optional<float>("ff");
		_dd = json.get_optional<float>("dd");
		_fxy = json.get_optional<float>("fxy");
		_dxy = json.get_optional<int>("dxy");
		_fxi = json.get_optional<float>("fxi");
		_dxi = json.get_optional<int>("dxi");
		_t = json.get_optional<float>("t");
		_td = json.get_optional<float>("td");
		_tn = json.get_optional<float>("tn");
		_tx = json.get_optional<float>("tx");
		_u = json.get_optional<float>("u");
		_un = json.get_optional<float>("un");
		_ux = json.get_optional<float>("ux");
		_pmer = json.get_optional<float>("pmer");
		_pres = json.get_optional<float>("pres");
		_glo = json.get_optional<float>("ray_glo01");
		_insolh = json.get_optional<int>("insolh");
	}
}

Observation MfRadomeMessage::getObservation(const CassUuid& station) const
{
	Observation result;

	if (_valid) {
		result.station = station;
		result.day = date::floor<date::days>(_timestamp);
		result.time = _timestamp;
		result.rainfall = { bool(_rr1), _rr1.value_or(0.f) };
		result.windspeed = { bool(_ff), from_mps_to_kph(_ff.value_or(0.f))  };
		result.winddir = { bool(_dd), _dd.value_or(0) };
		result.windgust = { bool(_fxi), from_mps_to_kph(_fxi.value_or(0.f)) };
		result.outsidetemp = { bool(_t), from_Kelvin_to_Celsius(_t.value_or(0.f)) };
		result.dewpoint = { bool(_td), from_Kelvin_to_Celsius(_td.value_or(0.f)) };
		result.min_outside_temperature = { bool(_tn), from_Kelvin_to_Celsius(_tn.value_or(0.f)) };
		result.max_outside_temperature = { bool(_tx), from_Kelvin_to_Celsius(_tx.value_or(0.f)) };
		result.outsidehum = { bool(_u), ::roundf(_u.value_or(0)) };
		if (bool(_pmer)) {
			result.barometer = { true, *_pmer / 100.f };
		} else if (bool(_pres) && bool(_t) && bool(_u)) {
			result.barometer = { true, seaLevelPressure(*_pres, from_Kelvin_to_Celsius(*_t), ::roundf(*_u)) };
		}
		result.solarrad = { bool(_glo), from_Jpsqcm_to_Wpsqm(_glo.value_or(0.f)) };
		result.insolation_time = { bool(_insolh), _insolh.value_or(0) };
	}

	return result;
}

}
