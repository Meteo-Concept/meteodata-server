/**
 * @file mbdatatxtdownloader.cpp
 * @brief Implementation of the MBDataTxtDownloader class
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
#include <memory>
#include <chrono>
#include <tuple>
#include <string>
#include <functional>

#include <systemd/sd-daemon.h>
#include <unistd.h>

#include <boost/asio/basic_waitable_timer.hpp>
#include <boost/asio/io_context.hpp>
#include <date/date.h>
#include <cassobs/dbconnection_observations.h>
#include <cassobs/dto/download.h>

#include "../cassandra_utils.h"
#include "mbdata_txt_downloader.h"
#include "mbdata_messages/mbdata_message_factory.h"
#include "../time_offseter.h"
#include "../curl_wrapper.h"

namespace asio = boost::asio;
namespace args = std::placeholders;

namespace meteodata
{

using namespace date;

const std::string MBDataTxtDownloader::DOWNLOAD_CONNECTOR_ID = "mbdatatxt";

MBDataTxtDownloader::MBDataTxtDownloader(DbConnectionObservations& db,
	const std::tuple<CassUuid, std::string, std::string, bool, int, std::string>& downloadDetails)
		:
		_db(db),
		_station(std::get<0>(downloadDetails)),
		_type(std::get<5>(downloadDetails)),
		_lastDownloadTime(chrono::seconds(0)) // any impossible date will do before the first download, if it's old enough, it cannot correspond to any date sent by the station
{
	float latitude;
	float longitude;
	int elevation;
	int pollingPeriod;
	db.getStationCoordinates(_station, latitude, longitude, elevation, _stationName, pollingPeriod);

	_timeOffseter = TimeOffseter::getTimeOffseterFor(TimeOffseter::PredefinedTimezone(std::get<4>(downloadDetails)));
	_timeOffseter.setLatitude(latitude);
	_timeOffseter.setLongitude(longitude);
	_timeOffseter.setElevation(elevation);
	_timeOffseter.setMeasureStep(pollingPeriod);

	std::ostringstream query;
	query << (std::get<3>(downloadDetails) ? "https://" : "http://") << std::get<1>(downloadDetails) << std::get<2>(downloadDetails);
	_query = query.str();
}

bool MBDataTxtDownloader::doProcess(const std::string& body)
{
	std::istringstream fileStream{body};

	auto m = MBDataMessageFactory::chose(_db, _station, _type, fileStream, _timeOffseter);
	if (!m || !(*m)) {
		std::cerr << SD_ERR << "[MBData " << _station << "] protocol: " << "Download failed for station "
			  << _stationName << std::endl;
		return false;
	}

	// We are still reading the last file, discard it
	if (m->getDateTime() <= _lastDownloadTime) {
		std::cerr << SD_NOTICE << "[MBData " << _station << "] measurement: " << "File for station " << _stationName
			  << " has not been updated" << std::endl;
		return false;
	}
	if (m->getDateTime() > chrono::system_clock::now() + chrono::minutes(1)) { // Allow for some clock deviation
		std::cerr << SD_ERR << "[MBData " << _station << "] management: " << "Station " << _stationName
			  << " has data in the future" << std::endl;
		return false;
	}

	char uuidStr[CASS_UUID_STRING_LENGTH];
	cass_uuid_string(_station, uuidStr);

	Observation o = m->getObservation(_station);
	bool ret = _db.insertV2DataPoint(o) && _db.insertV2DataPointInTimescaleDB(o);
	if (ret) {
		std::cout << SD_INFO << "[MBData " << _station << "] measurement: " << "Data from station " << _stationName
			  << " inserted into database" << std::endl;
	} else {
		std::cerr << SD_ERR << "[MBData " << _station << "] measurement: "
			  << "Insertion into database failed for station " << _stationName << std::endl;
		return false;
	}

	_lastDownloadTime = m->getDateTime();
	ret = _db.updateLastArchiveDownloadTime(_station, chrono::system_clock::to_time_t(_lastDownloadTime));
	if (!ret) {
		std::cerr << SD_ERR << "[MBData " << _station << "] management: "
			  << "Failed to update the last insertion time of station " << _stationName << std::endl;
		return false;
	}

	std::optional<float> rainfallSince0h = m->getRainfallSince0h();
	if (rainfallSince0h) {
		ret = _db.cacheFloat(_station, AbstractMBDataMessage::RAINFALL_SINCE_MIDNIGHT,
			chrono::system_clock::to_time_t(_lastDownloadTime), *rainfallSince0h);
		if (!ret) {
			std::cerr << SD_ERR << "[MBData  " << _station << "] protocol: "
				  << "Failed to cache the rainfall for station " << _station << std::endl;
		}
	}

	return ret;
}

void MBDataTxtDownloader::download(CurlWrapper& client)
{
	std::cout << SD_INFO << "[MBData " << _station << "] measurement: " << "Downloading a MBData file for station "
		  << _stationName << " (" << _query << ")" << std::endl;


	CURLcode ret = client.download(_query, [&](const std::string& body) {
		doProcess(body);
	});

	if (ret != CURLE_OK) {
		std::string_view error = client.getLastError();
		std::cerr << SD_ERR << "[MBData " << _station << "] protocol: " << "Download failed for " << _stationName
			  << ", bad response from " << _query << ": " << error;
	}
}

void MBDataTxtDownloader::downloadOnly(DbConnectionObservations& db, CurlWrapper& client,
	const std::tuple<CassUuid, std::string, std::string, bool, int, std::string>& downloadDetails)
{
	std::ostringstream queryStr;
	queryStr << (std::get<3>(downloadDetails) ? "https://" : "http://") << std::get<1>(downloadDetails) << std::get<2>(downloadDetails);
	std::string query = queryStr.str();
	CassUuid station = std::get<0>(downloadDetails);

	CURLcode ret = client.download(query, [&](const std::string& body) {
		// Ignore non-ASCII chars just in case, otherwise, the DB can
		// get annoyed
		std::string copy{body};
		for (char& c : copy) {
			if (c != '\r' && c != '\n' && (c < 20 || c >= 126))
				c = '?';
		}
		bool r = db.insertDownload(station, chrono::system_clock::to_time_t(chrono::system_clock::now()), DOWNLOAD_CONNECTOR_ID, copy, false, "new");
		if (!r) {
			std::cout << SD_ERR << "[MBDataTxt downloader] connection: "
				  << " inserting download failed for station " << station << std::endl;
			throw std::runtime_error("Download failed");
		}
	});

	if (ret != CURLE_OK) {
		std::string_view error = client.getLastError();
		std::cerr << SD_ERR << "[MBDataTxt " << station << "] protocol: " << "Download failed"
			  << " Bad response from " << query << ": " << error << std::endl;
		throw std::runtime_error("Download failed");
	}
}

void MBDataTxtDownloader::ingest()
{
	std::vector<Download> downloads;
	_db.selectDownloadsByStation(_station, DOWNLOAD_CONNECTOR_ID, downloads);

	if (downloads.empty()) {
		std::cout << SD_WARNING << "[MBDataTxt " << _station << "] measurement: "
			  << "no new data for station " << _stationName << std::endl;
	} else {
		std::cout << SD_INFO << "[MBDataTxt " << _station << "] measurement: "
			  << "ingesting downloaded data for station " << _stationName << std::endl;
	}

	for (const Download& d : downloads) {
		bool inserted = doProcess(d.content);
		if (!inserted) {
			std::cerr << SD_ERR << "[MBDataTxt " << _station << "] measurement: "
				  << "Failed to insert pre-downloaded observation in TimescaleDB for station " << _stationName << std::endl;
			_db.updateDownloadStatus(d.station, chrono::system_clock::to_time_t(d.datetime), false, "failed");
			throw std::runtime_error("Insertion failed");
		} else {
			_db.updateDownloadStatus(d.station, chrono::system_clock::to_time_t(d.datetime), true, "completed");
		}
	}
}

}
