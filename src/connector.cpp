/**
 * @file connector.cpp
 * @brief Implementation of the Connector class
 * @author Laurent Georget
 * @date 2016-10-05
 */
/*
 * Copyright (C) 2016  SAS Météo Concept <contact@meteo-concept.fr>
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
#include <iomanip>

#include <boost/asio.hpp>
#include <date.h>
#include <tz.h>

#include "connector.h"

namespace meteodata
{

Connector::Connector(boost::asio::io_context& ioContext, DbConnectionObservations& db) :
		_ioContext{ioContext},
		_db{db}
{}

std::string Connector::getStatus()
{
	using namespace date;

	auto h = date::floor<chrono::hours>(_status.timeToNextDownload);
	auto m = date::floor<chrono::minutes>(_status.timeToNextDownload - h);
	auto s = _status.timeToNextDownload - h - m;

	std::ostringstream os;
	auto z = date::current_zone();
	os << _status.shortStatus << "\n"
	   << "active since " << date::make_zoned(z, _status.activeSince) << "\n"
	   << _status.nbDownloads << " since last reload at " << date::make_zoned(z, _status.lastReloaded) << "\n"
	   << "next download scheduled ";

	if (h.count())
		os << h;
	os << std::setw(2) << std::setfill('0');
	if (m.count())
		os << m;
	os << s << " from now.\n";
	return os.str();
}

}
