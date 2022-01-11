/**
 * @file cimel_http_connection.cpp
 * @brief Implementation of the CimelHttpConnection class
 * @author Laurent Georget
 * @date 2022-01-10
 */
/*
 * Copyright (C) 2022 SAS Météo Concept <contact@meteo-concept.fr>
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
#include "cimel4A_importer.h"
#include "cimel_http_request_handler.h"

namespace meteodata {
	CimelHttpRequestHandler::CimelHttpRequestHandler(DbConnectionObservations& db) :
			_db{db}
	{
		std::vector<std::tuple<CassUuid, std::string, int>> cimelStations;
		_db.getAllCimelStations(cimelStations);
		for (auto&& s : cimelStations) {
			_stations[std::get<0>(s)] = StationInformation{ std::get<1>(s), TimeOffseter::PredefinedTimezone{std::get<2>(s)} };
		}
	}

	void CimelHttpRequestHandler::processRequest(const Request& request, Response& response)
	{
		bool targetFound = false;

		for (auto&& [verb, url, handler] : routes)  {
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

		response.result(targetFound ? boost::beast::http::status::method_not_allowed : boost::beast::http::status::not_found);
	}


	bool CimelHttpRequestHandler::getUuidAndCheckAccess(const Request& request, Response& response, CassUuid& uuid, const std::cmatch& url)
	{
		cass_uuid_from_string(url[1].str().c_str(), &uuid);
		if (_stations.count(uuid) == 0) {
			response.result(boost::beast::http::status::forbidden);
			return false;
		}
		return true;
	}

	void CimelHttpRequestHandler::postArchiveFile(const Request& request, Response& response, std::cmatch&& url) {
		CassUuid uuid;

		if (getUuidAndCheckAccess(request, response, uuid, url)) {
			const std::string& content = request.body();
			std::istringstream stream(content);

			std::string name;
			int pollingPeriod;
			time_t lastDownload;
			_db.getStationDetails(uuid, name, pollingPeriod, lastDownload);

			float latitude;
			float longitude;
			int elevation;
			_db.getStationCoordinates(uuid, latitude, longitude, elevation, name, pollingPeriod);

			StationInformation info = _stations[uuid];
			TimeOffseter timeOffseter = TimeOffseter::getTimeOffseterFor(info.timezone);
			timeOffseter.setMeasureStep(pollingPeriod);
			timeOffseter.setLatitude(latitude);
			timeOffseter.setLongitude(longitude);
			timeOffseter.setElevation(elevation);

			date::sys_seconds start, end;
			Cimel4AImporter cimel4AImporter(uuid, info.cimelId, std::move(timeOffseter), _db);

			response.body() = "";
			if (cimel4AImporter.import(stream, start, end, true)) {
				std::cerr << SD_INFO << "[CIMEL HTTP " << uuid << "] measurement: "
						  << "stored archive for station " << name
						  << std::endl;

				using namespace date;
				std::ostringstream os;
				os << "Data imported\n" << start << "\n" << end << "\n";
				response.body() = os.str();
				response.result(boost::beast::http::status::ok);
			} else {
				std::cerr << SD_ERR << "[CIMEL HTTP " << uuid << "] measurement: "
						  << "failed to store archive for station " << name << "! Aborting"
						  << std::endl;
				response.result(boost::beast::http::status::internal_server_error);
			}
		}
	}
}