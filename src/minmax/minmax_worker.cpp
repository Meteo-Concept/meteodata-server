/**
 * @file minmax_worker.cpp
 * @brief Definition of the MinmaxWorker class
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
#include <dbconnection_jobs.h>
#include <systemd/sd-daemon.h>

#include "minmax_worker.h"
#include "minmax_computer.h"
#include "cassandra_utils.h"
#include "date_utils.h"

namespace meteodata {

namespace sys = boost::system;
namespace chrono = std::chrono;

MinmaxWorker::MinmaxWorker(const Configuration& config, boost::asio::io_context& ioContext):
	_ioContext{ioContext},
	_timer{ioContext},
	_dbMinmax{config.address, config.user, config.password},
	_dbJobs{config.jobsDbAddress, config.jobsDbUsername, config.jobsDbPassword, config.jobsDbDatabase}
{
}


void MinmaxWorker::checkDeadline(const sys::error_code& e)
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

void MinmaxWorker::start()
{
	_stopped = false;
	processJobs();
}

void MinmaxWorker::stop()
{
	_stopped = true;
}

void MinmaxWorker::processJobs()
{
	using namespace date;

	if (_stopped)
		return;

	std::optional<DbConnectionJobs::StationJob> nextMinmaxJob = _dbJobs.retrieveMinmax();
	if (nextMinmaxJob && !_stopped) {
		MinmaxComputer computer{_dbMinmax};
		do {
			bool result = computer.computeMinmax(nextMinmaxJob->station, nextMinmaxJob->begin, nextMinmaxJob->end);
			date::sys_days b{date::floor<date::days>(nextMinmaxJob->begin)};
			date::sys_days e{date::floor<date::days>(nextMinmaxJob->end)};
			if (result) {
				std::cerr << SD_INFO << "Minmax computed for station "
					<< nextMinmaxJob->station << " between times "
					<< b << " and " << e << std::endl;
				_dbJobs.markJobAsFinished(nextMinmaxJob->id, std::time(nullptr), 0);
				if (to_year_month(b) < to_year_month(chrono::system_clock::now()))
					_dbJobs.publishMonthMinmax(nextMinmaxJob->station, chrono::system_clock::to_time_t(b), chrono::system_clock::to_time_t(e));
			} else {
				std::cerr << SD_ERR << "Minmax computation failed at least partially for station "
					<< nextMinmaxJob->station << " between times "
					<< b << " and " << e << std::endl;
				_dbJobs.markJobAsFinished(nextMinmaxJob->id, std::time(nullptr), 1);
			}
			nextMinmaxJob = _dbJobs.retrieveMinmax();
		} while (nextMinmaxJob);
	}

	_timer.expires_after(WAITING_DELAY);
	_timer.async_wait([this](const sys::error_code& e) { checkDeadline(e); });
}

}
