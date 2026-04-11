/**
 * @file ffvl_exporter.cpp
 * @brief Implementation of the FfvlExporter class
 * @author Laurent Georget
 * @date 2026-04-02
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

#include <iostream>
#include <memory>

#include <boost/system/error_code.hpp>
#include <boost/asio/basic_waitable_timer.hpp>
#include <date/date.h>
#include <cassandra.h>
#include <cassobs/dbconnection_observations.h>
#include <cassobs/dto/exported_station.h>
#include <systemd/sd-daemon.h>

#include "ffvl_exporter.h"
#include "curl_wrapper.h"
#include "http_utils.h"
#include "meteo_server.h"

namespace asio = boost::asio;
namespace sys = boost::system;
namespace chrono = std::chrono;

namespace meteodata
{

using namespace date;

FfvlExporter::FfvlExporter(
		asio::io_context& ioContext,
		DbConnectionObservations& db,
		const std::string& ffvlPartnerKey
	) :
	_ioContext{ioContext},
	_db{db},
	_partnerKey{ffvlPartnerKey},
	_timer{ioContext}
{
}

void FfvlExporter::start()
{
	_mustStop = false;
	reloadStations();
	waitForEvent();
}

void FfvlExporter::stop()
{
	_mustStop = true;
	_timer.cancel();
}

void FfvlExporter::reload()
{
	auto self{std::static_pointer_cast<FfvlExporter>(shared_from_this())};
	EventManager& em = MeteoServer::getEventManager();
	em.unsubscribeFromAll(self);
	std::lock_guard<std::mutex> guardOnStations{_stationsMutex};
	reloadStations();
	for (auto&& [s,p] : _stations) {
		em.subscribe(self, Event::EventType::NewDatapoint, s);
	}
	waitForEvent();
}

void FfvlExporter::reloadStations()
{
	std::vector<ExportedStation> ffvl;
	bool ok = _db.selectExportedStations("ffvl", ffvl);
	if (ok) {
		_stations.clear();
		for (auto&& s : ffvl) {
			_stations[s.station] = s.param;
		}
	}
}

void FfvlExporter::waitForEvent()
{
	if (_mustStop)
		return;

	auto self{std::static_pointer_cast<FfvlExporter>(shared_from_this())};
	_timer.expires_at(decltype(_timer)::time_point::max());
	_timer.async_wait([this, self](const sys::error_code& e) { checkDeadline(e); });
}

void FfvlExporter::checkDeadline(const sys::error_code& e)
{
	// if the timer has been cancelled, we have work to do, stop waiting
	if (e == sys::errc::operation_canceled)
		return;

	// Keep waiting until the end of time
	auto self{std::static_pointer_cast<FfvlExporter>(shared_from_this())};
	_timer.async_wait([this, self](const sys::error_code& e) { checkDeadline(e); });
}

void FfvlExporter::handle(const Event* event)
{
	//no-op: event unknown
	//TODO: warning log message
}

void FfvlExporter::handle(const NewDatapointEvent* event)
{
	std::lock_guard<std::mutex> guardOnStations{_stationsMutex};
	CassUuid st = event->getStation();
	auto it = _stations.find(st);
	if (it != _stations.end()) {
		_timer.cancel();
		postStationExportJob(st);
	}
}

void FfvlExporter::postStationExportJob(const CassUuid& station)
{
	auto self = shared_from_this();
	asio::post(_ioContext.get_executor(),
		[this,self,station]() {
			exportLastDatapoint(station);
			waitForEvent();
		});
}

void FfvlExporter::exportLastDatapoint(const CassUuid& station)
{
	// Get the data
	Observation values;
	time_t now = chrono::system_clock::to_time_t(chrono::system_clock::now());
	_db.getLastDataBefore(station, now, values);

	// Format the URL
	std::ostringstream os;
	os << BASE_URL
	   << "?";
	if (values.winddir.first)
		os << "dir_avg=" << values.winddir.second << "&";
	if (values.windspeed.first)
		os << "speed_avg=" << values.windspeed.second << "&";
	if (values.windgust.first)
		os << "speed_max=" << values.windgust.second << "&";
	if (values.min_windspeed.first)
		os << "speed_min=" << values.min_windspeed.second << "&";
	if (values.outsidetemp.first)
		os << "temperature=" << values.outsidetemp.second << "&";
	os << "ffvl_partner_api_key=" << _partnerKey << "&"
	   << "manufacturer_device_id=" << station;

	auto ret = _client.download(os.str(), [&](const std::string& body) {
		std::cerr << SD_DEBUG << "FFVL server says: " << body << std::endl;
	});
	if (ret != CURLE_OK) {
		std::string_view error = _client.getLastError();
		std::cerr << SD_ERR << "FFVL export for " << station << " Bad response: " << error << std::endl;
	}
}

}
