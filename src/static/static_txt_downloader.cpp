/**
 * @file statictxtdownloader.cpp
 * @brief Implementation of the StatICTxtDownloader class
 * @author Laurent Georget
 * @date 2019-02-06
 */
/*
 * Copyright (C) 2019  JD Environnement <contact@meteo-concept.fr>
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
#include <optional>

#include <systemd/sd-daemon.h>

#include <date.h>
#include <cassobs/dbconnection_observations.h>

#include "time_offseter.h"
#include "curl_wrapper.h"
#include "cassandra_utils.h"
#include "static/static_txt_downloader.h"
#include "static/static_message.h"

namespace asio = boost::asio;
namespace sys = boost::system;
namespace chrono = std::chrono;
namespace args = std::placeholders;

namespace meteodata
{

using namespace date;

StatICTxtDownloader::StatICTxtDownloader(DbConnectionObservations& db,
	CassUuid station, const std::string& host,
	const std::string& url, bool https, int timezone,
	std::map<std::string, std::string> sensors) :
		_db{db},
		_station{station},
		// any impossible date will do before the first download,
		// if it's old enough, it cannot correspond to any date sent
		// by the station
		_lastDownloadTime{chrono::seconds(0)},
		_sensors{std::move(sensors)}
{
	float latitude;
	float longitude;
	int elevation;
	int pollingPeriod;
	db.getStationCoordinates(station, latitude, longitude, elevation, _stationName, pollingPeriod);

	// Timezone is supposed to always be UTC for StatIC files, but it's better not to rely
	// on station owners to never misconfigure their station
	_timeOffseter = TimeOffseter::getTimeOffseterFor(TimeOffseter::PredefinedTimezone(timezone));
	_timeOffseter.setLatitude(latitude);
	_timeOffseter.setLongitude(longitude);
	_timeOffseter.setElevation(elevation);
	_timeOffseter.setMeasureStep(pollingPeriod);

	std::ostringstream query;
	query << (https ? "https://" : "http://") << host << url;
	_query = query.str();
}

void StatICTxtDownloader::download(CurlWrapper& client)
{
	std::cout << SD_INFO << "[StatIC " << _station << "] measurement: " << "Now downloading a StatIC file for station "
			  << _stationName << " (" << _query << ")" << std::endl;

	CURLcode ret = client.download(_query, [&](const std::string& body) {
		std::istringstream responseStream{body};

		StatICMessage m{responseStream, _timeOffseter, _sensors};
		if (!m) {
			std::cerr << SD_ERR << "[StatIC " << _station << "] protocol: "
					  << "StatIC file: Cannot parse response from: " << _query << std::endl;
			return;
		}

		if (m.getDateTime() == _lastDownloadTime) {
			// We are still reading the last file, discard it in order
			// not to pollute the cumulative rainfall value
			std::cout << SD_NOTICE << "[StatIC " << _station << "] protocol: " << "previous message from " << _query
					  << " has the same date: " << m.getDateTime() << "!" << std::endl;
			return;
		} else {
			// The rain is given over the last hour but the file may be
			// fetched more frequently so it's necessary to compute the
			// difference with the rainfall over an hour ago
			auto downloadTime = m.getDateTime();
			auto end = chrono::system_clock::to_time_t(downloadTime);
			auto begin1h = chrono::system_clock::to_time_t(downloadTime - chrono::hours(1));
			auto dayRainfall = getDayRainfall(downloadTime);

			float f1h;
			if (_db.getRainfall(_station, begin1h, end, f1h) && dayRainfall)
				m.computeRainfall(f1h, *dayRainfall);
		}

		auto o = m.getObservation(_station);
		bool ret = _db.insertV2DataPoint(o) &&
			   _db.insertV2DataPointInTimescaleDB(o);
		if (ret) {
			std::cout << SD_DEBUG << "[StatIC " << _station << "] measurement: " << "Data from StatIC file from "
					  << _query << " inserted into database" << std::endl;
		} else {
			std::cerr << SD_ERR << "[StatIC " << _station << "] measurement: "
					  << "Failed to insert data from StatIC file from " << _query << " into database" << std::endl;
		}

		ret = _db.updateLastArchiveDownloadTime(_station, chrono::system_clock::to_time_t(m.getDateTime()));
		if (!ret) {
			std::cerr << SD_ERR << "[StatIC " << _station << "] measurement: "
					  << "Failed to update the last insertion time of station " << _station << std::endl;
		}

		std::optional<float> newDayRain = m.getDayRainfall();
		if (newDayRain) {
			ret = _db.cacheFloat(_station, RAINFALL_SINCE_MIDNIGHT, chrono::system_clock::to_time_t(_lastDownloadTime),
								 *newDayRain);
			if (!ret) {
				std::cerr << SD_ERR << "[StatIC  " << _station << "] protocol: "
						  << "Failed to cache the rainfall for station " << _station << std::endl;
			}
		}
	});

	if (ret != CURLE_OK) {
		std::string_view error = client.getLastError();
		std::cerr << SD_ERR << "[StatIC " << _station << "] protocol: " << "Download failed for " << _stationName
				  << " Bad response from " << _query << ": " << error << std::endl;
	}
}

std::optional<float> StatICTxtDownloader::getDayRainfall(const date::sys_seconds& datetime)
{
	time_t lastUpdateTimestamp;
	float rainfall;

	date::local_seconds localMidnight = date::floor<date::days>(_timeOffseter.convertToLocalTime(datetime));
	date::sys_seconds localMidnightInUTC = _timeOffseter.convertFromLocalTime(localMidnight);
	std::time_t beginDay = chrono::system_clock::to_time_t(localMidnightInUTC);
	std::time_t currentTime = chrono::system_clock::to_time_t(datetime);

	if (_db.getCachedFloat(_station, RAINFALL_SINCE_MIDNIGHT, lastUpdateTimestamp, rainfall)) {
		auto lastUpdate = chrono::system_clock::from_time_t(lastUpdateTimestamp);
		if (!std::isnan(rainfall) && lastUpdate > localMidnightInUTC)
			return rainfall;
	}

	if (_db.getRainfall(_station, beginDay, currentTime, rainfall))
		return rainfall;
	else
		return std::nullopt;
}

}
