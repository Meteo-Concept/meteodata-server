/**
 * @file abstractsynopdownloader.cpp
 * @brief Implementation of the AbstractSynopDownloader class
 * @author Laurent Georget
 * @date 2019-02-20
 */
/*
 * Copyright (C) 2019  SAS Météo Concept <contact@meteo-concept.fr>
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
#include <chrono>
#include <map>
#include <vector>
#include <iomanip>

#include <systemd/sd-daemon.h>
#include <cassobs/dbconnection_observations.h>

#include "cassandra_utils.h"
#include "synop/ogimet_synop.h"
#include "synop/synop_download_scheduler.h"
#include "synop/synop_decoder/parser.h"

namespace chrono = std::chrono;

namespace meteodata
{
using namespace date;

SynopDownloadScheduler::SynopDownloadScheduler(asio::io_context& ioContext, DbConnectionObservations& db) :
	AbstractDownloadScheduler{chrono::minutes{MINIMAL_PERIOD_MINUTES}, ioContext, db}
{
}

void SynopDownloadScheduler::reloadStations()
{
	_groups.clear();

	// FR stations are downloaded via the MeteoFrance API since v2.13
	// add(GROUP_FR, chrono::minutes(20), chrono::hours(3));
	add(GROUP_LU, chrono::minutes(20), chrono::hours(3));

	// Add the deferred SYNOPs
	std::vector<std::tuple<CassUuid, std::string>> deferredSynops;
	_db.getDeferredSynops(deferredSynops);
	for (auto&& synop : deferredSynops) {
		add(std::get<1>(synop), chrono::minutes(6 * 60), chrono::hours(24));
	}

	std::vector<std::tuple<CassUuid, std::string>> icaos;
	_db.getAllIcaos(icaos);
	for (auto&& icao : icaos)
		_icaos.emplace(std::get<1>(icao), std::get<0>(icao));
}

void SynopDownloadScheduler::download()
{
	for (const SynopGroup& group : _groups) {
		if (_mustStop)
			break;
		auto now = chrono::system_clock::now();
		auto time = date::floor<chrono::minutes>(now - date::floor<date::days>(now));
		if (time.count() % group.period.count() < MINIMAL_PERIOD_MINUTES) {
			downloadGroup(group.prefix, group.backlog);
		}
	}
}

void SynopDownloadScheduler::downloadGroup(const std::string& group, const chrono::hours& backlog)
{
	std::cout << SD_INFO << "[SYNOP] measurement: " << "Now downloading SYNOP messages " << std::endl;

	std::ostringstream requestStream;
	buildDownloadRequest(requestStream, group, backlog);

	CURLcode ret = _client.download(std::string{"http://"} + HOST + requestStream.str(), [&](const std::string& body) {
		std::istringstream bodyIterator(body);

		std::string line;

		std::vector<Observation> allObs;

		while (std::getline(bodyIterator, line)) {
			std::istringstream lineIterator{line};

			Parser parser;
			if (parser.parse(lineIterator)) {
				const SynopMessage& m = parser.getDecodedMessage();
				auto uuidIt = _icaos.find(m._stationIcao);
				if (uuidIt != _icaos.end()) {
					char uuidStr[CASS_UUID_STRING_LENGTH];
					const CassUuid& station = uuidIt->second;
					cass_uuid_string(station, uuidStr);

					std::string stationName;
					int pollingPeriod;
					time_t lastArchiveDownloadTime;
					_db.getStationDetails(station, stationName, pollingPeriod, lastArchiveDownloadTime);
					float latitude, longitude;
					int elevation;
					_db.getStationLocation(station, latitude, longitude, elevation);
					TimeOffseter timeOffseter = TimeOffseter::getTimeOffseterFor(TimeOffseter::PredefinedTimezone::UTC);
					timeOffseter.setLatitude(latitude);
					timeOffseter.setLongitude(longitude);
					timeOffseter.setElevation(elevation);
					timeOffseter.setMeasureStep(pollingPeriod);

					OgimetSynop synop{m, &timeOffseter};
					auto o = synop.getObservations(station);
					_db.insertV2DataPoint(o);
					allObs.push_back(o);
					std::cout << SD_DEBUG << "[SYNOP] measurement: " << "Inserted into database" << std::endl;

					std::pair<bool, float> rainfall24 = std::make_pair(false, 0.f);
					std::pair<bool, int> insolationTime24 = std::make_pair(false, 0);
					auto it = std::find_if(m._precipitation.begin(), m._precipitation.end(),
										   [](const auto& p) { return p._duration == 24; });
					if (it != m._precipitation.end())
						rainfall24 = std::make_pair(true, it->_amount);
					if (m._minutesOfSunshineLastDay)
						insolationTime24 = std::make_pair(true, *m._minutesOfSunshineLastDay);
					auto day = date::floor<date::days>(m._observationTime) - date::days(1);
					_db.insertV2EntireDayValues(station, date::sys_seconds(day).time_since_epoch().count(), rainfall24,
												insolationTime24);
					if (m._minTemperature)
						_db.insertV2Tn(station, chrono::system_clock::to_time_t(m._observationTime),
									   *m._minTemperature / 10.f);
					if (m._maxTemperature)
						_db.insertV2Tx(station, chrono::system_clock::to_time_t(m._observationTime),
									   *m._maxTemperature / 10.f);
				}
			} else {
				std::cerr << SD_WARNING << "[SYNOP] measurement: " << "Record looks invalid, discarding..."
					  << std::endl;
			}
		}

		bool ret = _db.insertV2DataPointsInTimescaleDB(allObs.begin(), allObs.end());
		if (!ret) {
			std::cerr << SD_ERR << "[SYNOP] measurement: Failed to insert observations in TimescaleDB" << std::endl;
		}
	});

	if (ret != CURLE_OK) {
		std::string_view error = _client.getLastError();
		std::cerr << SD_ERR << "[SYNOP] protocol: " << "Failed to download SYNOPs: " << error << std::endl;
	}
}

void SynopDownloadScheduler::add(const std::string& group, const chrono::minutes& period, const chrono::hours& backlog)
{
	_groups.push_back({ group, period, backlog });
}


void SynopDownloadScheduler::buildDownloadRequest(std::ostream& out, const std::string& group, const chrono::hours& backlog)
{
	auto time = chrono::system_clock::now() - backlog;
	auto daypoint = date::floor<date::days>(time);
	auto ymd = date::year_month_day(daypoint);   // calendar date
	auto tod = date::make_time(time - daypoint); // Yields time_of_day type

	// Obtain individual components as integers
	auto y = int(ymd.year());
	auto m = unsigned(ymd.month());
	auto d = unsigned(ymd.day());
	auto h = tod.hours().count();
	auto min = 30;

	out << "/cgi-bin/getsynop?begin=" << std::setfill('0') << std::setw(4) << y << std::setw(2) << m << std::setw(2)
		<< d << std::setw(2) << h << std::setw(2) << min << "&block=" << group;
}

}
