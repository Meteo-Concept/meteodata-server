/**
 * @file watchdog.cpp
 * @brief Implementation of the Watchdog class
 * @author Laurent Georget
 * @date 2021-01-27
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

#include <iostream>
#include <chrono>
#include <systemd/sd-daemon.h>
#include <cstdlib>

#include <boost/system/error_code.hpp>
#include <boost/asio/basic_waitable_timer.hpp>
#include <boost/asio/io_service.hpp>

#include "watchdog.h"

namespace asio = boost::asio;
namespace chrono = std::chrono;
namespace args = std::placeholders;

namespace meteodata {

Watchdog::Watchdog(asio::io_service& ioService) :
	_ioService{ioService},
	_timer{ioService}
{
}
void Watchdog::start()
{
	const char* watchdogusec = getenv("WATCHDOG_USEC");
	if (watchdogusec) {
		_period = chrono::microseconds(std::strtoul(watchdogusec, nullptr, 10) / 2);
		waitUntilNextNotification();
	}
	// in case no watchdog notification period is passed to us by systemd
	// just bail off
}

void Watchdog::waitUntilNextNotification()
{
	auto self(shared_from_this());
	_timer.expires_from_now(_period);
	_timer.async_wait(std::bind(&Watchdog::checkDeadline, self, args::_1));
}

void Watchdog::checkDeadline(const sys::error_code& e)
{
	/* if the timer has been cancelled, then bail out ; we probably have been
	 * asked to die */
	if (e == sys::errc::operation_canceled)
		return;

	// verify that the timeout is not spurious
	if (_timer.expires_at() <= chrono::steady_clock::now()) {
		sendNotification();
		waitUntilNextNotification();
	} else {
		/* spurious handler call, restart the timer without changing the
		 * deadline */
		auto self(shared_from_this());
		_timer.async_wait(std::bind(&Watchdog::checkDeadline, self, args::_1));
	}
}

void Watchdog::sendNotification()
{
	sd_notify(0, "WATCHDOG=1");
}

}
