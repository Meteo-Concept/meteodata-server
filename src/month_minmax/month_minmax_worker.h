/**
 * @file month_minmax_worker.h
 * @brief Declaration of the MonthMinmaxWorker class
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
#ifndef METEODATA_SERVER_MONTH_MINMAX_WORKER_H
#define METEODATA_SERVER_MONTH_MINMAX_WORKER_H

#include <chrono>

#include <boost/asio/basic_waitable_timer.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/system/error_code.hpp>

#include <dbconnection_jobs.h>
#include <dbconnection_month_minmax.h>
#include <dbconnection_normals.h>

namespace meteodata
{

class MonthMinmaxWorker
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
		std::string stationsDbUsername;
		std::string stationsDbPassword;
		std::string stationsDbAddress;
		std::string stationsDbDatabase;
		unsigned long threads = 1;
	};

	MonthMinmaxWorker(const Configuration& config, boost::asio::io_context& ioContext);

	void start();

	void stop();

private:
	boost::asio::io_context& _ioContext;

	boost::asio::basic_waitable_timer<std::chrono::steady_clock> _timer;

	DbConnectionMonthMinmax _dbMonthMinmax;

	DbConnectionNormals _dbNormals;

	DbConnectionJobs _dbJobs;

	void processJobs();

	void checkDeadline(const boost::system::error_code& ec);

	bool _stopped = true;

	constexpr static std::chrono::seconds WAITING_DELAY{300};
};

}

#endif //METEODATA_SERVER_MONTH_MINMAX_WORKER_H
