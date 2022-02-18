/**
 * @file vantagepro2_http_connection.cpp
 * @brief Implementation of the VantagePro2HttpConnection class
 * @author Laurent Georget
 * @date 2021-12-24
 */
/*
 * Copyright (C) 2021 SAS Météo Concept <contact@meteo-concept.fr>
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

#include <boost/beast/http.hpp>
#include <systemd/sd-daemon.h>
#include <date.h>

#include <vector>
#include <tuple>
#include <regex>

#include "../http_connection.h"
#include "../cassandra.h"
#include "../cassandra_utils.h"
#include "../time_offseter.h"
#include "vantagepro2_archive_message.h"
#include "vantagepro2_http_request_handler.h"

namespace meteodata
{
VantagePro2HttpRequestHandler::VantagePro2HttpRequestHandler(DbConnectionObservations& db) :
		_db{db}
{
	std::vector<std::tuple<CassUuid, std::string, int, std::string, std::unique_ptr<char[]>, std::size_t, std::string, int>> mqttStations;
	_db.getMqttStations(mqttStations);
	std::regex userName{"^vp2/([^/]*)"};
	for (auto&& s : mqttStations) {
		const std::string& topic = std::get<6>(s);
		std::smatch match;
		if (std::regex_search(topic, match, userName)) {
			_userAndTimezoneByStation[std::get<0>(s)] = {match[1].str(),
														 TimeOffseter::PredefinedTimezone(std::get<7>(s))};
		}
	}
}

void VantagePro2HttpRequestHandler::processRequest(const Request& request, Response& response)
{
	bool targetFound = false;

	for (auto&&[verb, url, handler] : routes) {
		std::cmatch match;
		if (std::regex_match(request.target().begin(), request.target().end(), match, url)) {
			targetFound = true;
			if (verb == request.method()) {
				(this->*handler)(request, response, std::move(match));
				response.set(boost::beast::http::field::content_type, "text/plain");
				return;
			}
		}
	}

	response.result(
			targetFound ? boost::beast::http::status::method_not_allowed : boost::beast::http::status::not_found);
}


bool VantagePro2HttpRequestHandler::getUuidAndCheckAccess(const Request& request, Response& response, CassUuid& uuid,
														  const std::cmatch& url)
{
	cass_uuid_from_string(url[1].str().c_str(), &uuid);
	const boost::beast::string_view httpUser = request.base()["X-Authenticated-User"];
	if (httpUser.empty() || httpUser != _userAndTimezoneByStation[uuid].authorizedUser) {
		response.result(boost::beast::http::status::forbidden);
		return false;
	}
	return true;
}

void VantagePro2HttpRequestHandler::getLastArchive(const Request& request, Response& response, std::cmatch&& url)
{
	CassUuid uuid;
	if (getUuidAndCheckAccess(request, response, uuid, url)) {
		std::string name;
		int pollingPeriod;
		time_t lastDownload;
		_db.getStationDetails(uuid, name, pollingPeriod, lastDownload);
		response.body() = std::to_string(lastDownload);
	}
}

void VantagePro2HttpRequestHandler::postArchivePage(const Request& request, Response& response, std::cmatch&& url)
{
	CassUuid uuid;
	if (getUuidAndCheckAccess(request, response, uuid, url)) {
		const std::string& content = request.body();
		std::size_t size = content.size();
		if (size % sizeof(VantagePro2ArchiveMessage::ArchiveDataPoint) != 0) {
			response.result(boost::beast::http::status::not_acceptable);
			response.body() = "Incorrect response size when receiving archives";
			std::cerr << SD_ERR << "[VP2 HTTP " << uuid << "] protocol: " << "invalid size " << size << std::endl;
		}

		std::string name;
		int pollingPeriod;
		time_t lastDownload;
		_db.getStationDetails(uuid, name, pollingPeriod, lastDownload);

		float latitude;
		float longitude;
		int elevation;
		_db.getStationCoordinates(uuid, latitude, longitude, elevation, name, pollingPeriod);

		ClientInformation info = _userAndTimezoneByStation[uuid];
		TimeOffseter timeOffseter = TimeOffseter::getTimeOffseterFor(info.timezone);
		timeOffseter.setMeasureStep(pollingPeriod);
		timeOffseter.setLatitude(latitude);
		timeOffseter.setLongitude(longitude);
		timeOffseter.setElevation(elevation);

		bool ret = true;
		date::sys_seconds lastArchive = date::floor<std::chrono::seconds>(
				std::chrono::system_clock::from_time_t(lastDownload));
		auto start = lastArchive;

		const auto* dataPoint = reinterpret_cast<const VantagePro2ArchiveMessage::ArchiveDataPoint*>(content.data());
		const VantagePro2ArchiveMessage::ArchiveDataPoint* pastLastDataPoint =
				dataPoint + (size / sizeof(VantagePro2ArchiveMessage::ArchiveDataPoint));

		for (; dataPoint < pastLastDataPoint && ret ; ++dataPoint) {
			VantagePro2ArchiveMessage message{*dataPoint, &timeOffseter};

			if (message.looksValid()) {
				lastArchive = message.getTimestamp();
				auto end = lastArchive;
				auto day = date::floor<date::days>(start);
				auto lastDay = date::floor<date::days>(end);
				while (day <= lastDay) {
					ret = _db.deleteDataPoints(uuid, day, start, end);

					if (!ret)
						std::cerr << SD_ERR << "[VP2 HTTP " << uuid << "] management: "
								  << "couldn't delete temporary realtime observations for station " << name
								  << std::endl;
					day += date::days(1);
				}

				start = end;
				ret = _db.insertV2DataPoint(message.getObservation(uuid));
			} else {
				std::cerr << SD_WARNING << "[VP2 HTTP " << uuid << "] measurement: "
						  << "record looks invalid for station " << name << ", discarding..." << std::endl;
			}
			//Otherwise, just discard
		}

		if (ret) {
			std::cout << SD_DEBUG << "[VP2 HTTP " << uuid << "] measurement: " << "archive data stored for station "
					  << name << std::endl;
			time_t lastArchiveDownloadTime = lastArchive.time_since_epoch().count();
			ret = _db.updateLastArchiveDownloadTime(uuid, lastArchiveDownloadTime);
			if (!ret)
				std::cerr << SD_ERR << "[VP2 HTTP " << uuid << "] management: "
						  << "couldn't update last archive download time for station " << name << std::endl;
		} else {
			std::cerr << SD_ERR << "[VP2 HTTP " << uuid << "] measurement: " << "failed to store archive for station "
					  << name << "! Aborting" << std::endl;
			return;
		}

		response.body() = "";
		response.result(boost::beast::http::status::no_content);
	}
}
}