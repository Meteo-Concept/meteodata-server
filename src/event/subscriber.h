/**
 * @file subscriber.h
 * @brief Definition of the Subscriber class
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

#ifndef SUBSCRIBER_H
#define SUBSCRIBER_H

namespace meteodata
{
class Event;

/**
 * @brief The base class for all event
 */
class Subscriber
{
public:
	virtual void handle(const Event& event);
};

}

#endif
