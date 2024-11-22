/**
 * @file async_job_publisher.h
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

#ifndef ASYNC_JOB_PUBLISHER_H
#define ASYNC_JOB_PUBLISHER_H

#include <map>
#include <tuple>
#include <chrono>
#include <mutex>

#include <boost/asio/io_context.hpp>
#include <boost/asio/basic_waitable_timer.hpp>
#include <cassobs/dbconnection_jobs.h>
#include <date.h>
#include <cassandra.h>

namespace meteodata
{

class AsyncJobPublisher
{
public:
	AsyncJobPublisher(boost::asio::io_context& ioContext,
					 const std::string& dbAddr, const std::string& dbUsername,
					 const std::string& dbPassword, const std::string& dbName);

	void publishJobsForPastDataInsertion(const CassUuid& station,
		const date::sys_seconds& begin, const date::sys_seconds& end);

private:
	using Timer = boost::asio::basic_waitable_timer<std::chrono::system_clock>;

	boost::asio::io_context& _io;

	DbConnectionJobs _dbJobs;

	std::map<CassUuid, std::tuple<date::sys_seconds, date::sys_seconds, Timer>> _debouncing;

	std::mutex _mutex;

	void doPublish(const CassUuid& station);

	void resetTimer(Timer& timer, const CassUuid& station);
};

} // meteodata

#endif //ASYNC_JOB_PUBLISHER_H
