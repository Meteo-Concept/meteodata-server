/**
 * @file event_manager.cpp
 * @brief Definition of the EventManager class
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

#include <vector>
#include <string>
#include <map>
#include <utility>

#include <boost/asio/basic_waitable_timer.hpp>
#include <cassandra.h>
#include <cassobs/dbconnection_observations.h>

#include "event/event.h"
#include "event/event_manager.h"
#include "connector.h"
#include "cassandra_utils.h"

namespace meteodata
{

void EventManager::subscribe(Subscriber* subscriber, Event::EventType type)
{
	_subscriptions[type].push_back(subscriber);
}

void EventManager::subscribe(Subscriber* subscriber, Event::EventType type, const CassUuid& station)
{
	_subscriptionsForStation[std::make_pair(type, station)].push_back(subscriber);
}

void EventManager::publish(const Event& event)
{
	auto it = _subscriptions.find(event.getEventType());
	if (it != _subscriptions.end()) {
		for (Subscriber* s : it->second) {
			event.dispatch(*s);
		}
	}

	std::optional<CassUuid> station = event.getStation();
	if (station) {
		auto key = std::make_pair(event.getEventType(), *station);
		auto it2 = _subscriptionsForStation.find(key);
		for (Subscriber* s : it2->second) {
			event.dispatch(*s);
		}
	}
}

}
