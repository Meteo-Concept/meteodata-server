/**
 * @file cpl01_pluviometer_message.cpp
 * @brief Implementation of the Cpl01PluviometerMessage class
 * @author Laurent Georget
 * @date 2023-04-07
 */
/*
 * Copyright (C) 2023  SAS JD Environnement <contact@meteo-concept.fr>
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
#include <observation.h>
#include <boost/property_tree/ptree.hpp>

#include "cpl01_pluviometer_message.h"
#include "hex_parser.h"
#include "cassandra_utils.h"

namespace meteodata
{

namespace chrono = std::chrono;
namespace pt = boost::property_tree;

Cpl01PluviometerMessage::Cpl01PluviometerMessage(DbConnectionObservations& db):
	_db{db}
{}

void Cpl01PluviometerMessage::ingest(const CassUuid& station, const std::string& payload, const date::sys_seconds& datetime)
{
	using namespace hex_parser;

	if (!validateInput(payload, 22)) {
		_obs.valid = false;
		return;
	}

	_obs.time = datetime;

	std::istringstream is{payload};
	uint8_t statusAndAlarm;
	time_t timestamp;
	is >> parse(statusAndAlarm, 2, 16) >> parse(_obs.totalPulses, 6, 16) >> ignore(6) >> parse(timestamp, 8, 16);

	_obs.flag = statusAndAlarm & 0b1111'1100;
	_obs.alarm = statusAndAlarm & 0b0000'0010;
	_obs.currentlyOpen = statusAndAlarm & 0b0000'0001;

	time_t lastUpdate;
	int previousClicks;
	bool result = _db.getCachedInt(station, CPL01_RAINFALL_CACHE_KEY, lastUpdate, previousClicks);
	if (result && chrono::_V2::system_clock::from_time_t(lastUpdate) > chrono::_V2::system_clock::now() - chrono::hours{24}) {
		// the last rainfall datapoint is not too old, we can use
		// it as a reference for the current number of clicks recorded
		// by the pluviometer
		if (_obs.totalPulses > previousClicks) {
			_obs.rainfall = (_obs.totalPulses - previousClicks) * CPL01_RAIN_GAUGE_RESOLUTION;
		} else {
			_obs.rainfall = ((0xFFFFFFu - previousClicks) + _obs.totalPulses) * CPL01_RAIN_GAUGE_RESOLUTION;
		}
	}

	// if the datetime in the message is more recent than the lastest archive,
	// trust it to be correct, otherwise ignore it, the station might not be
	// synced with the LoRa clock yet
	if (timestamp > lastUpdate) {
		_obs.time = date::floor<chrono::seconds>(chrono::system_clock::from_time_t(timestamp));
	}

	_obs.valid = true;
}

void Cpl01PluviometerMessage::cacheValues(const CassUuid& station)
{
	if (_obs.valid) {
		int ret = _db.cacheInt(station, CPL01_RAINFALL_CACHE_KEY, chrono::system_clock::to_time_t(_obs.time), _obs.totalPulses);
		if (!ret)
			std::cerr << SD_ERR << "[MQTT " << station << "] management: "
					  << "Couldn't update the rainfall number of clicks, accumulation error possible"
					  << std::endl;
	}
}

Observation Cpl01PluviometerMessage::getObservation(const CassUuid& station) const
{
	Observation obs;
	obs.station = station;
	obs.day = date::floor<date::days>(_obs.time);
	obs.time = _obs.time;
	obs.rainfall = {!std::isnan(_obs.rainfall), _obs.rainfall};
	return obs;
}

pt::ptree Cpl01PluviometerMessage::getDecodedMessage() const
{
	pt::ptree decoded;
	decoded.put("model", "CPL01_pluviometer_20230410");
	auto& value = decoded.put_child("value", pt::ptree{});
	value.put("flag", _obs.flag);
	value.put("alarm", _obs.alarm);
	value.put("currently_open", _obs.currentlyOpen ? "true" : "false");
	value.put("total_pulses", _obs.totalPulses);
	value.put("rainfall", _obs.rainfall);

	return decoded;
}

}
