/**
 * @file nbiot_udp_connection.cpp
 * @brief Implementation of the NbiotUdpConnection class
 * @author Laurent Georget
 * @date 2024-06-21
 */
/*
 * Copyright (C) 2024 SAS Météo Concept <contact@meteo-concept.fr>
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

#include <boost/asio.hpp>
#include <systemd/sd-daemon.h>
#include <date.h>
#include <dbconnection_observations.h>

#include <sstream>
#include <vector>
#include <tuple>
#include <regex>
#include <string>

#include "cassandra_utils.h"
#include "udp_connection.h"
#include "async_job_publisher.h"
#include "dragino/thplnbiot_message.h"
#include "nbiot/nbiot_udp_request_handler.h"
#include "hex_parser.h"
#include "http_utils.h"

namespace meteodata
{
NbiotUdpRequestHandler::NbiotUdpRequestHandler(DbConnectionObservations& db, AsyncJobPublisher* jobPublisher) :
		_db{db},
		_jobPublisher{jobPublisher}
{
}

void NbiotUdpRequestHandler::reloadStations()
{
	std::vector<NbiotStation> nbiotStations;
	_db.getAllNbiotStations(nbiotStations);
	for (auto&& s : nbiotStations) {
		_infosByStation[s.imei] = s;
	}
}

void NbiotUdpRequestHandler::processRequest(const std::string& rawBody)
{
	using namespace hex_parser;

	if (rawBody.size() < 16) {
		std::cerr << SD_ERR << "[UDP] protocol: UDP message too short" << std::endl;
		return;
	}

	// We convert the string to a hexadecimal string to process its content
	// even if the internal converters will probably have to un-hexify part
	// of it to parse integers or floats
	std::string body{hexify(rawBody)};
	std::cerr << SD_DEBUG << "[UDP] protocol: Parsing UDP message (" << rawBody.size() << " bytes)\n"
		  << body << std::endl;

	std::istringstream is{body};

	// IMEI is 15 hexadecimal characters
	char imeiRaw[15];
	is >> ignore(1);
	is.read(imeiRaw, 15);
	std::string imei{imeiRaw, 15};

	auto it = _infosByStation.find(imei);
	if (it != _infosByStation.end()) {
		const NbiotStation& st = it->second;
		const CassUuid& uuid = st.station;

		std::string message = body.substr(0, body.size() - 64);
		std::string key;
		std::istringstream keyIs{st.hmacKey};
		key.resize(st.hmacKey.size() / 2);
		for (char& i : key) {
			keyIs >> parse(i, 2, 16);
		}
		std::string expectedHmac = computeHMACWithSHA256(message, key);
		std::string receivedHmac = body.substr(body.size() - 64);
		if (expectedHmac != receivedHmac) {
			std::cerr << "HMAC " << receivedHmac << " does not validate "
				  << "for message " << message << ", "
				  << "expected " << expectedHmac << std::endl;
			// TODO fail with an error here
		}

		std::string name;
		int pollingPeriod;
		time_t lastDownload;
		bool storeInsideMeasurements;
		_db.getStationDetails(uuid, name, pollingPeriod, lastDownload, &storeInsideMeasurements);

		float latitude;
		float longitude;
		int elevation;
		_db.getStationCoordinates(uuid, latitude, longitude, elevation, name, pollingPeriod);

		// TODO: When we have more message type, we'll need a factory
		// to instantiate the correct message.
		ThplnbiotMessage msg{_db};
		msg.ingest(uuid, body);

		date::sys_seconds oldest = date::floor<chrono::seconds>(chrono::system_clock::now());
		date::sys_seconds newest = date::sys_seconds{};

		for (const Observation& obs : msg.getObservations(uuid)) {
			bool ret = _db.insertV2DataPoint(obs);
			if (ret) {
				std::cout << SD_DEBUG << "[THPLNBIOT UDP " << uuid << "] measurement: " << "archive data stored for station "
						  << name << std::endl;
				time_t lastArchiveDownloadTime = chrono::system_clock::to_time_t(obs.time);
				oldest = std::min(obs.time, oldest);
				newest = std::max(obs.time, newest);
				ret = _db.updateLastArchiveDownloadTime(uuid, lastArchiveDownloadTime);
				if (!ret)
					std::cerr << SD_ERR << "[THPLNBIOT UDP " << uuid << "] management: "
							  << "couldn't update last archive download time for station " << name << std::endl;
				msg.cacheValues(uuid);
			} else {
				std::cerr << SD_ERR << "[THPLNBIOT UDP " << uuid << "] measurement: " << "failed to store an observation for station "
						  << name << "! Trying the other ones..." << std::endl;
			}
		}

		if (oldest < newest) {
			_jobPublisher->publishJobsForPastDataInsertion(uuid, oldest, newest);
		}
	}
}
}
