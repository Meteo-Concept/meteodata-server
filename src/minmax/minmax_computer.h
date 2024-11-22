/**
 * @file minmax_computer.h
 * @brief Declaration of the MinmaxComputer class
 * @author Laurent Georget
 * @date 2023-07-21
 */
/*
 * Copyright (C) 2023  SAS Météo Concept <contact@meteo-concept.fr>
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

#ifndef METEODATA_SERVER_MINMAX_COMPUTER_H
#define METEODATA_SERVER_MINMAX_COMPUTER_H

#include <cassandra.h>
#include <date.h>

#include "cassobs/dbconnection_minmax.h"
#include "cassobs/dbconnection_month_minmax.h"
#include "cassobs/dbconnection_normals.h"

namespace meteodata
{

class MinmaxComputer
{
public:
	explicit MinmaxComputer(DbConnectionMinmax& dbMinmax);

	bool computeMinmax(const CassUuid& station, const date::sys_seconds& begin, const date::sys_seconds& end);

private:
	DbConnectionMinmax& _dbMinmax;
};

} // meteodata

#endif //METEODATA_SERVER_MINMAX_COMPUTER_H
