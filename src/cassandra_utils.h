#ifndef CASSANDRA_UTILS
#define CASSANDRA_UTILS
/**
 * @file cassandra_utils.h
 * @brief Definition of some handy functions to manipulate Cassandra types
 * @author Laurent Georget
 * @date 2019-09-16
 */
/*
 * Copyright (C) 2019  SAS Météo Concept <contact@meteo-concept.fr>
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

#include <ostream>

#include <cassandra.h>

inline bool operator==(const CassUuid& uuid1, const CassUuid& uuid2)
{
	return uuid1.time_and_version == uuid2.time_and_version;
}

inline bool operator<(const CassUuid& uuid1, const CassUuid& uuid2)
{
	return uuid1.time_and_version < uuid2.time_and_version ||
		   (uuid1.time_and_version == uuid2.time_and_version && uuid1.clock_seq_and_node < uuid2.clock_seq_and_node);
}

inline std::ostream& operator<<(std::ostream& os, const CassUuid& uuid)
{
	char str[CASS_UUID_STRING_LENGTH];
	cass_uuid_string(uuid, str);
	os << str;
	return os;
}

#endif
