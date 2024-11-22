/**
 * @file connector_group.h
 * @brief Definition of the ConnectorGroup class
 * @author Laurent Georget
 * @date 2023-04-08
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
#ifndef CONNECTOR_GROUP_H
#define CONNECTOR_GROUP_H

#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <boost/asio.hpp>
#include <cassobs/dbconnection_observations.h>

namespace meteodata
{
/**
 * @brief A group of connectors, acting as a proxy for all contained connectors
 */
class ConnectorGroup : public Connector
{
public:
	ConnectorGroup(boost::asio::io_context& ioContext, DbConnectionObservations& db);

	~ConnectorGroup() override;

	/**
	 * @brief Start all inner connectors
	 */
	void start() override;

	/**
	 * @brief Stop all inner connectors
	 */
	void stop() override;

	/**
	 * @brief Reload all inner connectors
	 */
	void reload() override;

	std::string getStatus() const override;

	void addConnector(const std::weak_ptr<Connector>& connector);

private:
	std::vector<std::weak_ptr<Connector>> _connectors;

	void cleanupExpiredConnectors();
};
}

#endif // CONNECTOR_GROUP_H
