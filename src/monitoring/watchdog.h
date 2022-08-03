/**
 * @file watchdog.h
 * @brief Definition of the Watchdog class
 * @author Laurent Georget
 * @date 2021-01-21
 */
/*
 * Copyright (C) 2021  SAS JD Environnement <contact@meteo-concept.fr>
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

#ifndef WATCHDOG_H
#define WATCHDOG_H

#include <systemd/sd-daemon.h>

#include <chrono>

#include <boost/asio/basic_waitable_timer.hpp>
#include <boost/asio/io_context.hpp>

namespace meteodata
{

namespace asio = boost::asio;
namespace sys = boost::system;
namespace chrono = std::chrono;

using namespace std::placeholders;

/**
 * @brief The watchdog that monitors Meteodata (in)activity and notifies systemd
 *
 * Under some rare circumstances, Meteodata can freeze. I guess it's due do some
 * race conditions around timeouts in HTTP requests that end up locking all
 * the download workers eventually...
 *
 * Anyway, for safety purposes, it's best to have a watchdog that can trigger if
 * Meteodata is not active.
 */
class Watchdog : public std::enable_shared_from_this<Watchdog>
{
public:
	/**
	 * @brief Construct the watchdog
	 *
	 * @param ioContext the Boost object used to process asynchronous
	 * events, timers, and callbacks
	 */
	Watchdog(asio::io_context& ioContext);

	/**
	 * @brief Start the periodic watchdog notification
	 */
	void start();

private:
	/**
	 * @brief The Boost service that processes asynchronous events, timers,
	 * etc.
	 */
	asio::io_context& _ioContext;

	/**
	 * @brief The timer used to periodically trigger the data downloads
	 */
	asio::basic_waitable_timer<chrono::steady_clock> _timer;

	chrono::microseconds _period;

private:
	/**
	 * @brief Wait for the periodic notification timer to tick again
	 */
	void waitUntilNextNotification();

	/**
	 * @brief Notify systemd that we are still alive
	 */
	void sendNotification();

	/**
	 * @brief The callback registered to react to the periodic notification
	 * timer ticking
	 *
	 * This method makes sure the deadline set for the timer is actually
	 * reached (the timer could go off in case of an error, or anything).
	 *
	 * @param e The error/return code of the timer event
	 */
	void checkDeadline(const sys::error_code& e);
};

}

#endif
