/**
 * @file event.h
 * @brief Definition of the Event class
 * @author Laurent Georget
 * @date 2026-03-28
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

#ifndef EVENT_H
#define EVENT_H

#include <optional>
#include <string>

#include <cassandra.h>
#include <cassobs/dbconnection_observations.h>

#include "connector.h"

namespace meteodata
{

class Subscriber;

/**
 * @brief The base class for all event
 */
class Event
{
public:
	enum class EventType {
		NewDatapoint,
	};

	virtual std::optional<CassUuid> getStation() const { return {}; }

	virtual Event::EventType getEventType() const = 0;
	virtual std::string getEventName() const = 0;
	virtual void dispatch(Subscriber& visitor) const = 0;

private:
};

}

#endif
