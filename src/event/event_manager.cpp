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
#include <mutex>

#include <boost/asio/basic_waitable_timer.hpp>
#include <cassandra.h>
#include <cassobs/dbconnection_observations.h>
#include <systemd/sd-daemon.h>

#include "event/event.h"
#include "event/event_manager.h"
#include "event/subscriber.h"
#include "connector.h"
#include "cassandra_utils.h"

namespace
{
	std::function<bool(const std::weak_ptr<meteodata::Subscriber>&)> matchingOrAbsentSubscriber(const std::shared_ptr<meteodata::Subscriber>& subscriber)
	{
		return [&subscriber](const std::weak_ptr<meteodata::Subscriber>& sub) -> bool {
			std::shared_ptr<meteodata::Subscriber> locked = sub.lock();
			if (locked) {
				return locked.get() == subscriber.get();
			} else {
				// remove all deleted subscribers as well
				return true;
			}
		};
	}
}

namespace meteodata
{

void EventManager::subscribe(const std::shared_ptr<Subscriber>& subscriber, Event::EventType type)
{
	std::lock_guard<std::mutex> guardOnSubs{_mutex};
	_subscriptions[type].push_back(subscriber);
}

void EventManager::subscribe(const std::shared_ptr<Subscriber>& subscriber, Event::EventType type, const CassUuid& station)
{
	std::lock_guard<std::mutex> guardOnSubs{_mutex};
	_subscriptionsForStation[std::make_pair(type, station)].push_back(subscriber);
}

void EventManager::unsubscribeFromAll(const std::shared_ptr<Subscriber>& subscriber)
{
	auto eraseMatchingOrAbsentSubscriber = ::matchingOrAbsentSubscriber(subscriber);

	std::lock_guard<std::mutex> guardOnSubs{_mutex};
	for (auto&& [event,subscribers] : _subscriptions) {
		subscribers.erase(
			std::remove_if(subscribers.begin(), subscribers.end(), eraseMatchingOrAbsentSubscriber),
			subscribers.end()
		);
	}

	for (auto&& [key,subscribers] : _subscriptionsForStation) {
		subscribers.erase(
			std::remove_if(subscribers.begin(), subscribers.end(), eraseMatchingOrAbsentSubscriber),
			subscribers.end()
		);
	}
}

void EventManager::unsubscribe(const std::shared_ptr<Subscriber>& subscriber, Event::EventType type)
{
	std::lock_guard<std::mutex> guardOnSubs{_mutex};
	auto it = _subscriptions.find(type);
	if (it != _subscriptions.end()) {
		it->second.erase(
			std::remove_if(it->second.begin(), it->second.end(),
				matchingOrAbsentSubscriber(subscriber)
			),
			it->second.end()
		);
	}
}

void EventManager::unsubscribe(const std::shared_ptr<Subscriber>& subscriber, Event::EventType type, const CassUuid& station)
{
	std::lock_guard<std::mutex> guardOnSubs{_mutex};
	auto it = _subscriptionsForStation.find(std::make_pair(type, station));
	if (it != _subscriptionsForStation.end()) {
		it->second.erase(
			std::remove_if(it->second.begin(), it->second.end(),
				matchingOrAbsentSubscriber(subscriber)
			),
			it->second.end()
		);
	}
}

void EventManager::publish(const Event& event)
{
	std::lock_guard<std::mutex> guardOnSubs{_mutex};
	auto it = _subscriptions.find(event.getEventType());
	if (it != _subscriptions.end()) {
		for (auto it2 = it->second.begin() ; it2 != it->second.end() ; ++it2) {
			const std::weak_ptr<Subscriber>& s = *it2;
			auto sub = s.lock();
			if (sub) {
				std::cerr << SD_DEBUG << "Dispatching event " << event.getEventName() << std::endl;
				event.dispatch(*sub);
				++it2;
			} else {
				it2 = it->second.erase(it2);
			}
		}
	}
}

void EventManager::publish(const Event& event, const CassUuid& station)
{
	std::lock_guard<std::mutex> guardOnSubs{_mutex};
	auto key = std::make_pair(event.getEventType(), station);
	auto it3 = _subscriptionsForStation.find(key);
	if (it3 != _subscriptionsForStation.end()) {
		for (auto it4 = it3->second.begin() ; it4 != it3->second.end() ; ++it4) {
			const std::weak_ptr<Subscriber>& s = *it4;
			auto sub = s.lock();
			if (sub) {
				std::cerr << SD_DEBUG << "Dispatching event " << event.getEventName() << " for station " << station << std::endl;
				event.dispatch(*sub);
				++it4;
			} else {
				it4 = it3->second.erase(it4);
			}
		}
	}
}

}
