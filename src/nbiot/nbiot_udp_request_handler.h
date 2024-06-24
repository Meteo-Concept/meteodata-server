/**
 * @file nbiot_udp_request_handler.h
 * @brief Definition of the NbiotUdpRequestHandler class
 * @author Laurent Georget
 * @date 2024-06-24
 */
/*
 * Copyright (C) 2024 SAS Météo Concept <contact@meteo-concept.fr>
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
#ifndef NBIOT_UDP_REQUEST_HANDLER_H
#define NBIOT_UDP_REQUEST_HANDLER_H

#include <boost/asio.hpp>
#include <systemd/sd-daemon.h>
#include <date.h>
#include <dbconnection_observations.h>

#include <map>
#include <vector>
#include <tuple>
#include <regex>

#include "async_job_publisher.h"

namespace meteodata
{

class NbiotUdpRequestHandler
{
public:
	explicit NbiotUdpRequestHandler(DbConnectionObservations& db, AsyncJobPublisher* jobPublisher = nullptr);
	void processRequest(const std::string& body);

private:
	DbConnectionObservations& _db;

	AsyncJobPublisher* _jobPublisher;

	std::map<std::string, NbiotStation> _infosByStation;
};

}


#endif //NBIOT_UDP_REQUEST_HANDLER_H
