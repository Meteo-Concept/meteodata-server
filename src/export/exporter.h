/**
 * @file exporter.h
 * @brief Definition of the Exporter class
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
#ifndef EXPORTER_H
#define EXPORTER_H

#include <iostream>
#include <memory>

#include <boost/asio.hpp>
#include <cassobs/dbconnection_observations.h>


namespace meteodata
{
class Event;

/**
 * @brief Parent class of all meteo stations exporters
 */
class Exporter : public std::enable_shared_from_this<Exporter>
{
public:
	/**
	 * @brief Destroy the exporter
	 */
	virtual ~Exporter() = default;

	virtual void start() = 0;

	virtual void stop() = 0;

	virtual void reload() = 0;

protected:
	/**
	 * @brief Construct an exporter
	 *
	 * This method is only callable by actual child classes
	 * exporters.
	 * @param ioContext The Boost::Asio service to use for all
	 * Boost::Asio asynchronous operations
	 * @param db The handle to the database
	 */
	Exporter(boost::asio::io_context& ioContext, DbConnectionObservations& db);

	/**
	 * @brief The Boost::Asio service to use for asynchronous
	 * operations
	 */
	boost::asio::io_context& _ioContext;
	/**
	 * @brief The connection to the observations/climatology database
	 */
	DbConnectionObservations& _db;
};
}

#endif // EXPORTER_H
