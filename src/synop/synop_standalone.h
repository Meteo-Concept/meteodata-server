/**
 * @file synop_standalone.h
 * @brief Definition of the SynopStandalone class
 * @author Laurent Georget
 * @date 2018-08-28
 */
/*
 * Copyright (C) 2018  SAS Météo Concept <contact@meteo-concept.fr>
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

#ifndef SYNOP_STANDALONE_H
#define SYNOP_STANDALONE_H

#include <iostream>
#include <string>
#include <map>

#include <cassandra.h>
#include <date.h>
#include <cassobs/dbconnection_observations.h>


namespace meteodata
{

using namespace meteodata;

/**
 */
class SynopStandalone
{
public:
	SynopStandalone(DbConnectionObservations& db);
	void start(const std::string& file);

private:
	DbConnectionObservations& _db;
	std::map<std::string, CassUuid> _icaos;

	static constexpr char HOST[] = "www.ogimet.com";
	static constexpr char GROUP_FR[] = "07";
};

}

#endif
