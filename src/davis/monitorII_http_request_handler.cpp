/**
 * @file monitorII_http_connection.cpp
 * @brief Implementation of the MonitorIIHttpConnection class
 * @author Laurent Georget
 * @date 2024-12-17
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

#include <boost/beast/http.hpp>
#include <systemd/sd-daemon.h>
#include <date/date.h>

#include <sstream>
#include <vector>
#include <tuple>
#include <regex>

#include "http_connection.h"
#include "cassandra.h"
#include "cassandra_utils.h"
#include "time_offseter.h"
#include "async_job_publisher.h"
#include "davis/monitorII_archive_entry.h"
#include "davis/monitorII_http_request_handler.h"

namespace meteodata
{
MonitorIIHttpRequestHandler::MonitorIIHttpRequestHandler(DbConnectionObservations& db, AsyncJobPublisher* jobPublisher) :
		_db{db},
		_jobPublisher{jobPublisher}
{
	std::vector<std::tuple<CassUuid, std::string, int, std::string, std::unique_ptr<char[]>, std::size_t, std::string, int>> mqttStations;
	_db.getMqttStations(mqttStations);
	std::regex userName{"^monitorII/([^/]*)"};
	for (auto&& s : mqttStations) {
		const std::string& topic = std::get<6>(s);
		std::smatch match;
		if (std::regex_search(topic, match, userName)) {
			_userAndTimezoneByStation[std::get<0>(s)] = {match[1].str(),
			TimeOffseter::PredefinedTimezone(std::get<7>(s))};
		}
	}
}

void MonitorIIHttpRequestHandler::processRequest(const Request& request, Response& response)
{
	bool targetFound = false;

	auto begin = request.target().begin();
	// Discard any query string for now
	auto end = std::find(begin, request.target().end(), '?');

	for (auto&&[verb, url, handler] : routes) {
		std::cmatch match;
		if (std::regex_match(begin, end, match, url)) {
			targetFound = true;
			if (verb == request.method()) {
				(this->*handler)(request, response, std::move(match));
				response.set(boost::beast::http::field::content_type, "text/plain");
				return;
			}
		}
	}

	response.result(targetFound ? boost::beast::http::status::method_not_allowed : boost::beast::http::status::not_found);
}


bool MonitorIIHttpRequestHandler::getUuidAndCheckAccess(const Request& request, Response& response, CassUuid& uuid,
		const std::cmatch& url)
{
	cass_uuid_from_string(url[1].str().c_str(), &uuid);
	const boost::beast::string_view httpUser = request.base()["X-Authenticated-User"];
	if (httpUser.empty()) {
		response.result(boost::beast::http::status::unauthorized);
		response.body() = "Authenticated user required";
		return false;
	}

	if (_userAndTimezoneByStation.count(uuid) == 0) {
		response.result(boost::beast::http::status::forbidden);
		response.body() = "Station " + url[1].str() + " unknown";
		return false;
	}

	if (httpUser != _userAndTimezoneByStation[uuid].authorizedUser) {
		response.result(boost::beast::http::status::forbidden);
		std::ostringstream os;
		os << "Access to station " << uuid << " by user " << httpUser << " forbidden";
		response.body() = os.str();
		return false;
	}
	return true;
}

void MonitorIIHttpRequestHandler::getLastArchive(const Request& request, Response& response, std::cmatch&& url)
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

void MonitorIIHttpRequestHandler::postArchivePage(const Request& request, Response& response, std::cmatch&& url)
{
	CassUuid uuid;
	if (getUuidAndCheckAccess(request, response, uuid, url)) {
		const std::string& content = request.body();
		std::size_t size = content.size();
		if (size % sizeof(MonitorIIArchiveEntry::DataPoint) != 0) {
			response.result(boost::beast::http::status::not_acceptable);
			response.body() = "Incorrect response size when receiving archives";
			std::cerr << SD_ERR << "[MonitorII HTTP " << uuid << "] protocol: " << "invalid size " << size << std::endl;
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

		ClientInformation info = _userAndTimezoneByStation[uuid];

		bool ret = true;
		date::sys_seconds lastArchive = date::floor<std::chrono::seconds>(
			std::chrono::system_clock::from_time_t(lastDownload));
		auto start = lastArchive;

		date::sys_seconds oldestArchive = date::floor<chrono::seconds>(chrono::system_clock::now());
		date::sys_seconds newestArchive{};

		const auto* dataPoint = reinterpret_cast<const MonitorIIArchiveEntry::DataPoint*>(content.data());
		const MonitorIIArchiveEntry::DataPoint* pastLastDataPoint = dataPoint + (size / sizeof(MonitorIIArchiveEntry::DataPoint));

		std::vector<Observation> allObs;

		for (; dataPoint < pastLastDataPoint && ret ; ++dataPoint) {
			MonitorIIArchiveEntry message{*dataPoint};

			if (message.looksValid()) {
				lastArchive = message.getTimestamp();

				Observation o = message.getObservation(uuid);
				allObs.push_back(o);
				ret = _db.insertV2DataPoint(o);
			} else {
				std::cerr << SD_WARNING << "[MonitorII HTTP " << uuid << "] measurement: "
					  << "record looks invalid for station " << name << ", discarding..." << std::endl;
			}
			//Otherwise, just discard
		}

		ret = ret && _db.insertV2DataPointsInTimescaleDB(allObs.begin(), allObs.end());

		if (ret) {
			std::cout << SD_DEBUG << "[MonitorII HTTP " << uuid << "] measurement: " << "archive data stored for station "
				  << name << std::endl;
			time_t lastArchiveDownloadTime = lastArchive.time_since_epoch().count();
			ret = _db.updateLastArchiveDownloadTime(uuid, lastArchiveDownloadTime);
			if (!ret)
				std::cerr << SD_ERR << "[MonitorII HTTP " << uuid << "] management: "
					  << "couldn't update last archive download time for station " << name << std::endl;

			if (_jobPublisher) {
				_jobPublisher->publishJobsForPastDataInsertion(uuid, oldestArchive, newestArchive);
			}
		} else {
			std::cerr << SD_ERR << "[MonitorII HTTP " << uuid << "] measurement: " << "failed to store archive for station "
				  << name << "! Aborting" << std::endl;
			return;
		}

		response.body() = "";
		response.result(boost::beast::http::status::no_content);
	}
}


}
