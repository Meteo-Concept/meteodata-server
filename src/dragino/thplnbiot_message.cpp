/**
 * @file thplnbiot_message.cpp
 * @brief Implementation of the ThplnbiotMessage class
 * @author Laurent Georget
 * @date 2024-06-19
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
#include <cmath>
#include <vector>
#include <chrono>

#include <systemd/sd-daemon.h>
#include <cassandra.h>
#include <observation.h>

#include "dragino/thplnbiot_message.h"
#include "hex_parser.h"
#include "davis/vantagepro2_message.h"

namespace meteodata
{

namespace chrono = std::chrono;

ThplnbiotMessage::ThplnbiotMessage(DbConnectionObservations& db):
	_db{db}
{}

bool ThplnbiotMessage::validateInput(const std::string& payload)
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

void ThplnbiotMessage::ingest(const CassUuid& station, const std::string& payload)
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
	is >> ignore(HEADER_LENGTH)
	   >> ignore(DATA_POINT_LENGTH);

	_obs.resize(nbMessagesExpected);

	std::cerr << SD_DEBUG << "[UDP NB-IoT] protocol: " << "Payload " << payload
			  << " contains " << nbMessagesExpected << " messages" << std::endl;

	for (int i=nbMessagesExpected-1 ; i>=0 ; i--) {
		DataPoint obs;

		uint16_t temp;
		uint16_t hum;
		uint16_t intensity;
		uint32_t timestamp;
		is >> parse(temp, 4, 16)
		   >> parse(hum, 4, 16)
		   >> parse(obs.count, 8, 16)
		   >> parse(intensity, 4, 16)
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

		if (intensity == 0x7FFF) {
			obs.intensity = NAN;
		} else {
			obs.intensity = float(intensity)/10;
		}

		obs.time = date::floor<chrono::seconds>(chrono::system_clock::from_time_t(timestamp));

		obs.valid = true;

		_obs[i] = std::move(obs);
	}

	// Go over all messages again, in chronological order, to compute the
	// rainfall amount
	for (DataPoint& dp : _obs) {
		if (!dp.valid)
			continue;

		time_t lastUpdate;
		int previousClicks;
		bool result = _db.getCachedInt(station, THPLNBIOT_RAINFALL_CACHE_KEY, lastUpdate, previousClicks);
		if (result && chrono::_V2::system_clock::from_time_t(lastUpdate) > chrono::_V2::system_clock::now() - chrono::hours{24}) {
			// the last rainfall datapoint is not too old, we can use
			// it as a reference for the current number of clicks recorded
			// by the pluviometer
			if (dp.count >= previousClicks) {
				dp.rainfall = (dp.count - previousClicks) * THPLNBIOT_RAIN_GAUGE_RESOLUTION;
			} else {
				dp.rainfall = ((0xFFFFFFFFu - previousClicks) + dp.count) * THPLNBIOT_RAIN_GAUGE_RESOLUTION;
			}
		}
	}
}

std::vector<Observation> ThplnbiotMessage::getObservations(const CassUuid& station) const
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
		obs.rainfall = {!std::isnan(dp.rainfall), dp.rainfall};
		obs.rainrate = {!std::isnan(dp.intensity), dp.intensity};

		exported.push_back(std::move(obs));
	}
	return exported;
}


}
