/**
 * @file abstract_weatherlink_downloader.h
 * @brief Definition of the AbstractWeatherlinkDownloader class
 * @author Laurent Georget
 * @date 2019-09-19
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

#ifndef ABSTRACT_WEATHERLINK_DOWNLOADER_H
#define ABSTRACT_WEATHERLINK_DOWNLOADER_H

#include <iostream>
#include <memory>
#include <string>

#include <boost/system/error_code.hpp>
#include <boost/asio.hpp>
#include <date.h>
#include <dbconnection_observations.h>
#include "async_job_publisher.h"

#include "time_offseter.h"
#include "async_job_publisher.h"


namespace meteodata
{

namespace ip = boost::asio::ip;
namespace asio = boost::asio;

using namespace std::placeholders;
using namespace meteodata;

/**
 */
class AbstractWeatherlinkDownloader : public std::enable_shared_from_this<AbstractWeatherlinkDownloader>
{
public:
	AbstractWeatherlinkDownloader(const CassUuid& station, DbConnectionObservations& db,
								  TimeOffseter&& to, AsyncJobPublisher* jobPublisher = nullptr) :
			_db{db},
			_jobPublisher{jobPublisher},
			_station{station},
			_timeOffseter{to},
			_pollingPeriod{0}
	{
		time_t lastArchiveDownloadTime;
		bool storeInsideMeasurement;
		db.getStationDetails(station, _stationName, _pollingPeriod, lastArchiveDownloadTime, &storeInsideMeasurement);
		float latitude, longitude;
		int elevation;
		db.getStationLocation(station, latitude, longitude, elevation);
		_lastArchive = date::sys_seconds(chrono::seconds(lastArchiveDownloadTime));
		_timeOffseter.setLatitude(latitude);
		_timeOffseter.setLongitude(longitude);
		_timeOffseter.setElevation(elevation);
		_timeOffseter.setMeasureStep(_pollingPeriod);
		_timeOffseter.setMayStoreInsideMeasurements(storeInsideMeasurement);
	}

	AbstractWeatherlinkDownloader(const CassUuid& station, DbConnectionObservations& db,
								  TimeOffseter::PredefinedTimezone tz, AsyncJobPublisher* jobPublisher = nullptr) :
			_db{db},
			_jobPublisher{jobPublisher},
			_station{station},
			_pollingPeriod{0}
	{
		time_t lastArchiveDownloadTime;
		bool storeInsideMeasurement;
		db.getStationDetails(station, _stationName, _pollingPeriod, lastArchiveDownloadTime, &storeInsideMeasurement);
		float latitude, longitude;
		int elevation;
		db.getStationLocation(station, latitude, longitude, elevation);
		_lastArchive = date::sys_seconds(chrono::seconds(lastArchiveDownloadTime));
		_timeOffseter = TimeOffseter::getTimeOffseterFor(tz);
		_timeOffseter.setLatitude(latitude);
		_timeOffseter.setLongitude(longitude);
		_timeOffseter.setElevation(elevation);
		_timeOffseter.setMeasureStep(_pollingPeriod);
		_timeOffseter.setMayStoreInsideMeasurements(storeInsideMeasurement);
	}

protected:
	/**
	 * @brief A connection to the observations database, to store the data
	 * that is downloaded
	 */
	DbConnectionObservations& _db;

	/**
	 * @brief An optional asynchronous job publisher, to schedule climatology
	 * computations after downloads
	 */
	AsyncJobPublisher* _jobPublisher;

	/**
	 * @brief The connected station's identifier in the database
	 */
	CassUuid _station;
	std::string _stationName;

	/**
	 * @brief The amount of time between two queries for data to the stations
	 */
	int _pollingPeriod;

	/**
	 * @brief The timestamp (in POSIX time) of the last archive entry
	 * recorded in the database
	 */
	date::sys_seconds _lastArchive;

	/**
	 * @brief The timestamp (in POSIX time) of the oldest archive entry
	 * retrieved from the station
	 */
	date::sys_seconds _oldestArchive{date::floor<chrono::seconds>(std::chrono::system_clock::now())};

	/**
	 * @brief The timestamp (in POSIX time) of the newest archive entry
	 * retrieved from the station
	 */
	date::sys_seconds _newestArchive{};

	/**
	 * @brief The \a TimeOffseter to use to convert timestamps between the
	 * station's time and POSIX time
	 */
	TimeOffseter _timeOffseter;

public:
	inline int getPollingPeriod() const
	{
		return _pollingPeriod;
	};
};

}

#endif
