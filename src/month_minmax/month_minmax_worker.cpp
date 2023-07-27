/**
 * @file minmax_worker.cpp
 * @brief Definition of the MonthMinmaxWorker class
 * @author Laurent Georget
 * @date 2023-07-26
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

#include <chrono>
#include <iostream>

#include <date.h>
#include <dbconnection_minmax.h>
#include <dbconnection_jobs.h>
#include <systemd/sd-daemon.h>
#include <date/date.h>

#include "month_minmax_worker.h"
#include "month_minmax/month_minmax_computer.h"
#include "cassandra_utils.h"

namespace meteodata {

namespace sys = boost::system;
namespace chrono = std::chrono;

MonthMinmaxWorker::MonthMinmaxWorker(const Configuration& config, boost::asio::io_context& ioContext):
	_ioContext{ioContext},
	_timer{ioContext},
	_dbMonthMinmax{config.address, config.user, config.password},
	_dbNormals{config.stationsDbAddress, config.stationsDbUsername, config.stationsDbPassword, config.stationsDbDatabase},
	_dbJobs{config.jobsDbAddress, config.jobsDbUsername, config.jobsDbPassword, config.jobsDbDatabase}
{
}


void MonthMinmaxWorker::checkDeadline(const sys::error_code& e)
{
	if (e == sys::errc::operation_canceled)
		return;

	if (_timer.expires_at() <= chrono::steady_clock::now()) {
		if (!_stopped)
			processJobs();
	} else {
		/* spurious handler call, restart the timer without changing the
		 * deadline */
		_timer.async_wait([this](const sys::error_code& e) { checkDeadline(e); });
	}
}

void MonthMinmaxWorker::start()
{
	_stopped = false;
	processJobs();
}

void MonthMinmaxWorker::stop()
{
	_stopped = true;
}

void MonthMinmaxWorker::processJobs()
{
	using namespace date;

	if (_stopped)
		return;

	std::optional<DbConnectionJobs::StationJob> nextMonthMinmaxJob = _dbJobs.retrieveMonthMinmax();
	if (nextMonthMinmaxJob && !_stopped) {
		MonthMinmaxComputer computer{_dbMonthMinmax, _dbNormals};
		do {
			bool result = computer.computeMonthMinmax(nextMonthMinmaxJob->station, nextMonthMinmaxJob->begin, nextMonthMinmaxJob->end);
			date::sys_days b{date::floor<date::days>(nextMonthMinmaxJob->begin)};
			date::sys_days e{date::floor<date::days>(nextMonthMinmaxJob->end)};
			if (result) {
				std::cerr << SD_INFO << "Month minmax computed for station "
					<< nextMonthMinmaxJob->station << " between times "
					<< b << " and " << e << std::endl;
				_dbJobs.markJobAsFinished(nextMonthMinmaxJob->id, std::time(nullptr), 0);
			} else {
				std::cerr << SD_ERR << "Month minmax computation failed at least partially for station "
					<< nextMonthMinmaxJob->station << " between times "
					<< b << " and " << e << std::endl;
				_dbJobs.markJobAsFinished(nextMonthMinmaxJob->id, std::time(nullptr), 1);
			}
			nextMonthMinmaxJob = _dbJobs.retrieveMonthMinmax();
		} while (nextMonthMinmaxJob);
	}

	_timer.expires_after(WAITING_DELAY);
	_timer.async_wait([this](const sys::error_code& e) { checkDeadline(e); });
}

}
