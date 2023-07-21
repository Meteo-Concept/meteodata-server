/**
 * @file async_job_publisher.cpp
 * @brief Definition of the AsyncJobPublisher class
 * @author Laurent Georget
 * @date 2023-07-20
 */
/*
 * Copyright (C) 2023  SAS Météo Concept <contact@meteo-concept.fr>
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

#include <map>
#include <mutex>
#include <iostream>

#include <systemd/sd-daemon.h>

#include "async_job_publisher.h"
#include "cassandra_utils.h"

namespace meteodata
{

namespace asio = boost::asio;
namespace chrono = std::chrono;
namespace sys = boost::system;

using namespace std::chrono_literals;

AsyncJobPublisher::AsyncJobPublisher(boost::asio::io_context& ioContext, meteodata::DbConnectionJobs& dbJobs) :
	_io{ioContext},
	_dbJobs{dbJobs}
{}

void AsyncJobPublisher::publishJobsForPastDataInsertion(const CassUuid& station, const date::sys_seconds& begin, const date::sys_seconds& end)
{
	auto it = _debouncing.find(station);
	if (it == _debouncing.end()) {
		Timer timer{_io};
		resetTimer(timer, station);
		_debouncing.emplace(station, std::make_tuple(begin, end, std::move(timer)));
	} else {
		date::sys_seconds& b = std::get<0>(it->second);
		date::sys_seconds& e = std::get<1>(it->second);
		asio::basic_waitable_timer<chrono::system_clock>& timer = std::get<2>(it->second);
		timer.cancel();
		if (begin < b) {
			b = begin;
		}
		if (end > e) {
			e = end;
		}
		resetTimer(timer, station);
	}
}

void AsyncJobPublisher::doPublish(const CassUuid& station)
{
	auto it = _debouncing.find(station);
	if (it != _debouncing.end()) {
		std::get<2>(it->second).cancel();
		time_t b = std::get<0>(it->second).time_since_epoch().count();
		time_t e = std::get<1>(it->second).time_since_epoch().count();
		_dbJobs.publishMinmax(station, b, e);
		_dbJobs.publishAnomalyMonitoring(station, b, e);
	}
}

void AsyncJobPublisher::resetTimer(Timer& timer, const CassUuid& station)
{
	timer.expires_after(1min);
	timer.async_wait([this, &timer, &station](const sys::error_code& e) {
		if (e != sys::errc::operation_canceled) {
			try {
				doPublish(station);
			} catch (std::exception& e) {
				std::cerr << SD_ERR << "Failed publishing a job for station " << station
					<< ": " << e.what() << std::endl;
				// better luck next time?
				resetTimer(timer, station);
			}
		}
	});
}

} // meteodata