/**
 * @file new_datapoint_event.cpp
 * @brief Definition of the NewDatapointEvent methods
 * @author Laurent Georget
 * @date 2026-04-01
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


#include <memory>
#include <vector>
#include <string>
#include <chrono>
#include <map>

#include <boost/asio/basic_waitable_timer.hpp>
#include <cassandra.h>
#include <cassobs/dbconnection_observations.h>

#include "new_datapoint_event.h"
#include "event/subscriber.h"

namespace meteodata
{

namespace asio = boost::asio;
namespace sys = boost::system;
namespace chrono = std::chrono;

using namespace std::placeholders;

NewDatapointEvent::NewDatapointEvent(
	const CassUuid& station, const date::sys_seconds& receivedAt, const date::sys_seconds& measuredAt
) : m_station{station},
    m_receivedAt{receivedAt},
    m_measuredAt{measuredAt}
{}

void NewDatapointEvent::dispatch(Subscriber& visitor) const
{
	visitor.handle(*this);
}

}
