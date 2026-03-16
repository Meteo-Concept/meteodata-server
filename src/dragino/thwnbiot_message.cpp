/**
 * @file thwnbiot_message.cpp
 * @brief Implementation of the ThwnbiotMessage class
 * @author Laurent Georget
 * @date 2026-06-16
 */
/*
 * Copyright (C) 2026  SAS JD Environnement <contact@meteo-concept.fr>
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

#include <algorithm>
#include <string>
#include <cmath>
#include <vector>
#include <chrono>

#include <systemd/sd-daemon.h>
#include <cassandra.h>
#include <cassobs/observation.h>

#include "dragino/thwnbiot_message.h"
#include "hex_parser.h"
#include "davis/vantagepro2_message.h"
#include "cassandra_utils.h"

namespace meteodata
{

namespace chrono = std::chrono;

ThwnbiotMessage::ThwnbiotMessage(DbConnectionObservations& db):
	_db{db}
{}

bool ThwnbiotMessage::validateInput(const std::string& payload)
{
	if ((payload.length() - HEADER_LENGTH - FOOTER_LENGTH) % DATA_POINT_LENGTH != 0) {
		std::cerr << SD_ERR << "[UDP NB-IoT] protocol: " << "Invalid size " << payload.length() << " for payload "
				  << payload << std::endl;
		return false;
	}

	if (!std::all_of(payload.cbegin(), payload.cend(), [](char c) {
		return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
	})) {
		std::cerr << SD_ERR << "[UDP NB-IoT] protocol: " << "Payload " << payload
				  << " contains invalid characters" << std::endl;
		return false;
	}

	return true;
}

void ThwnbiotMessage::ingest(const CassUuid& station, const std::string& payload)
{
	using namespace hex_parser;

	if (!validateInput(payload)) {
		_valid = false;
		std::cerr << SD_ERR << "[UDP NB-IoT] protocol: Invalid payload" << std::endl;
		return;
	}

	// We skip the first data point (taken in-between two scheduled
	// collection times)
	int nbMessagesExpected = (payload.length() - HEADER_LENGTH - FOOTER_LENGTH - DATA_POINT_LENGTH) / DATA_POINT_LENGTH;
	std::istringstream is{payload};

	// The battery information is only in the header
	uint16_t version;
	uint16_t battery;
	uint16_t signal;
	uint16_t mode;
	is >> ignore(16)
	   >> parse(version, 4, 16)
	   >> parse(battery, 4, 16)
	   >> parse(signal, 2, 16)
	   >> parse(mode, 2, 16)
	   >> ignore(DATA_POINT_LENGTH);

	_obs.resize(nbMessagesExpected);

	std::cerr << SD_DEBUG << "[UDP NB-IoT] protocol: " << "Payload " << payload
		  << " contains " << nbMessagesExpected << " messages" << std::endl;

	for (int i=nbMessagesExpected-1 ; i>=0 ; i--) {
		DataPoint obs;

		uint16_t temp;
		uint16_t hum;
		uint16_t windPulses = 0U;
		uint16_t gustPulses = 0U;
		uint16_t minPulses = 0U;
		uint16_t windDir = 0xFFFFU;
		uint32_t timestamp;
		is >> parse(temp, 4, 16)
		   >> parse(hum, 4, 16)
		   >> parse(windPulses, 4, 16)
		   >> parse(gustPulses, 2, 16)
		   >> parse(minPulses, 2, 16)
		   >> parse(windDir, 4, 16)
		   >> parse(timestamp, 8, 16);

		obs.humidity = float(hum) / 10.f;
		if (temp == 0xFFFF && hum == 0xFFFF) {
			obs.temperature = NAN;
			obs.humidity = NAN;
		} else if ((temp & 0x8000) == 0) {
			obs.temperature = float(temp) / 10.f;
		} else {
			obs.temperature = (float(temp) - 65536) / 10.f;
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
		obs.windSpeed = from_mph_to_kph(windPulses * 2.25 / (pollingPeriod * 60));
		obs.gustSpeed = from_mph_to_kph(gustPulses);
		obs.minWindSpeed = from_mph_to_kph(minPulses);

		if (windDir != 0xFFFF) {
			obs.windDir = windDir;
		}


		obs.time = date::floor<chrono::seconds>(chrono::system_clock::from_time_t(timestamp));

		obs.valid = true;

		_obs[i] = std::move(obs);
	}

	// The battery information is only present in the realtime data, inject
	// it in the last archive entry
	if (!_obs.empty()) {
		_obs.back().battery = battery;
	}
}

std::vector<Observation> ThwnbiotMessage::getObservations(const CassUuid& station) const
{
	std::vector<Observation> exported;

	for (const DataPoint& dp : _obs) {
		if (!dp.valid)
			continue;

		Observation obs;
		obs.station = station;
		obs.day = date::floor<date::days>(dp.time);
		obs.time = dp.time;
		obs.outsidetemp = {!std::isnan(dp.temperature), dp.temperature};
		obs.outsidehum = {!std::isnan(dp.humidity), int(std::round(dp.humidity))};
		if (!std::isnan(dp.temperature) && !std::isnan(dp.humidity)) {
			obs.dewpoint = {true, dew_point(dp.temperature, dp.humidity)};
			obs.heatindex = {true, heat_index(from_Celsius_to_Farenheit(dp.temperature), dp.humidity)};
		}
		obs.windspeed = {!std::isnan(dp.windSpeed), dp.windSpeed};
		obs.windgust = {!std::isnan(dp.gustSpeed), dp.gustSpeed};
		obs.min_windspeed = {!std::isnan(dp.minWindSpeed), dp.minWindSpeed};
		obs.winddir = {!std::isnan(dp.windDir), int(std::round(dp.windDir))};
		obs.voltage_battery = {!std::isnan(dp.battery), dp.battery};

		exported.push_back(std::move(obs));
	}
	return exported;
}


}
