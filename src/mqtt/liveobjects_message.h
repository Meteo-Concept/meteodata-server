/**
 * @file liveobjects_message.h
 * @brief Definition of the LiveobjectsMessage class
 * @author Laurent Georget
 * @date 2022-04-28
 */
/*
 * Copyright (C) 2022  SAS JD Environnement <contact@meteo-concept.fr>
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

#ifndef LIVEOBJECTS_MESSAGE_H
#define LIVEOBJECTS_MESSAGE_H

#include <cassandra.h>
#include <observation.h>

namespace meteodata {

class LiveobjectsMessage
{
public:
	virtual ~LiveobjectsMessage() = default;

	virtual bool validateInput(const std::string& payload, int expectedSize);

	virtual Observation getObservation(const CassUuid& station) const = 0;

	virtual inline bool looksValid() const = 0;

	virtual void ingest(const CassUuid& station, const std::string& payload, const date::sys_seconds& timestamp) = 0;

	virtual void cacheValues(const CassUuid& station)
	{
		// no-op
	};
};

}


#endif //LIVEOBJECTS_MESSAGE_H
