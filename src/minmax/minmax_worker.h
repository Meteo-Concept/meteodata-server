/**
 * @file minmax_worker.h
 * @brief Declaration of the MinmaxWorker class
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
#ifndef METEODATA_SERVER_MINMAX_WORKER_H
#define METEODATA_SERVER_MINMAX_WORKER_H

#include <chrono>

#include <boost/asio/basic_waitable_timer.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/system/error_code.hpp>

#include <cassobs/dbconnection_jobs.h>
#include <cassobs/dbconnection_minmax.h>

namespace meteodata
{

class MinmaxWorker
{
public:
	struct Configuration
	{
		std::string user;
		std::string password;
		std::string address;
		std::string jobsDbUsername;
		std::string jobsDbPassword;
		std::string jobsDbAddress;
		std::string jobsDbDatabase;
		unsigned long threads = 1;
	};

	MinmaxWorker(const Configuration& config, boost::asio::io_context& ioContext);

	void start();

	void stop();

private:
	boost::asio::io_context& _ioContext;

	boost::asio::basic_waitable_timer<std::chrono::steady_clock> _timer;

	DbConnectionMinmax _dbMinmax;

	DbConnectionJobs _dbJobs;

	void processJobs();

	void checkDeadline(const boost::system::error_code& ec);

	bool _stopped = true;

	constexpr static std::chrono::seconds WAITING_DELAY{30};
};

}

#endif //METEODATA_SERVER_MINMAX_WORKER_H
