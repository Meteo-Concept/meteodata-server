/**
 * @file exporter.cpp
 * @brief Implementation of the Exporter class
 * @author Laurent Georget
 * @date 2026-04-15
 */
/*
 * Copyright (C) 2026  SAS Météo Concept <contact@meteo-concept.fr>
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

#include <boost/asio.hpp>
#include <cassobs/dbconnection_observations.h>

#include "exporter.h"

namespace meteodata
{

Exporter::Exporter(boost::asio::io_context& ioContext, DbConnectionObservations& db) :
		_ioContext{ioContext},
		_db{db}
{}

}
