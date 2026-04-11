/**
 * @file ffvl_exporter.h
 * @brief Definition of the FfvlExporter class
 * @author Laurent Georget
 * @date 2026-03-28
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

#ifndef FFVL_EXPORTER_H
#define FFVL_EXPORTER_H

#include <string>
#include <chrono>
#include <map>
#include <string>
#include <memory>
#include <mutex>

#include <boost/asio.hpp>
#include <cassandra.h>
#include <cassobs/dbconnection_observations.h>
#include <cassobs/dto/exported_station.h>

#include "event/subscriber.h"
#include "event/new_datapoint_event.h"
#include "cassandra_utils.h"
#include "curl_wrapper.h"

namespace meteodata
{

namespace asio = boost::asio;
namespace sys = boost::system;
namespace chrono = std::chrono;

/**
 * @brief The component responsible for sending data to the FFVL platform
 * for selected stations as they arrive
 *
 * Only one exporter is necessary for all stations
 */
class FfvlExporter : public std::enable_shared_from_this<FfvlExporter>, public Subscriber
{
public:
	/**
	 * @brief Construct the exporter
	 *
	 * @param ioContext the Boost object used to process asynchronous
	 * events, timers, and callbacks
	 * @param db the Météodata observations database connector
	 */
	FfvlExporter(asio::io_context& ioContext, DbConnectionObservations& db,
		const std::string& ffvlPartnerKey);

	/**
	 * @brief Start the exports
	 */
	void start();

	/**
	 * @brief Stop the exports
	 */
	void stop();

	/**
	 * @brief Reload the configuration
	 */
	void reload();

	void handle(const Event* event);
	void handle(const NewDatapointEvent* event);

private:
	asio::io_context& _ioContext;
	DbConnectionObservations& _db;

	std::string _partnerKey;

	/**
	 * @brief The CURL handler used to make HTTP requests
	 */
	CurlWrapper _client;

	/**
	 * @brief Whether to stop exporting data
	 */
	bool _mustStop = false;

	std::map<CassUuid, std::string> _stations;

	asio::steady_timer _timer;

	std::mutex _stationsMutex;

	/**
	 * @brief Reload the list of FFVL stations from the database
	 */
	void reloadStations();

	void waitForEvent();

	void checkDeadline(const sys::error_code& e);

	void postStationExportJob(const CassUuid& station);

	void exportLastDatapoint(const CassUuid& station);

	constexpr static char BASE_URL[] = "https://balisemeteo.com/ws2/push_data.php";
};

}

#endif
