/**
 * @file event_manager.h
 * @brief Definition of the EventManager class
 * @author Laurent Georget
 * @date 2026-03-26
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

#ifndef EVENT_MANAGER_H
#define EVENT_MANAGER_H

#include <vector>
#include <string>
#include <map>
#include <utility>

#include <boost/asio/io_context.hpp>
#include <cassandra.h>
#include <cassobs/dbconnection_observations.h>

#include "event.h"
#include "subscriber.h"

namespace meteodata
{

namespace asio = boost::asio;

/**
 * @brief The central class organizing the subscribing and notifying of all
 * events
 */
class EventManager
{
public:
	/**
	 * @brief Subscribe to an event type
	 */
	void subscribe(const std::shared_ptr<Subscriber>& subscriber, Event::EventType type);

	/**
	 * @brief Subscribe to an event type for a specific station
	 */
	void subscribe(const std::shared_ptr<Subscriber>& subscriber, Event::EventType type, const CassUuid& station);

	void unsubscribeFromAll(const std::shared_ptr<Subscriber>& subscriber);
	void unsubscribe(const std::shared_ptr<Subscriber>& subscriber, Event::EventType type);
	void unsubscribe(const std::shared_ptr<Subscriber>& subscriber, Event::EventType type, const CassUuid& station);

	void publish(const Event& event);
	void publish(const Event& event, const CassUuid& station);



private:
	std::map<std::pair<Event::EventType, CassUuid>, std::vector<std::weak_ptr<Subscriber>>> _subscriptionsForStation;
	std::map<Event::EventType, std::vector<std::weak_ptr<Subscriber>>> _subscriptions;
};

}

#endif
