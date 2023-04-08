/**
 * @file connector_group.cpp
 * @brief Implementation of the ConnectorGroup class
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

#include <algorithm>
#include <iostream>
#include <memory>
#include <string>

#include <boost/asio.hpp>
#include <dbconnection_observations.h>

#include "connector.h"
#include "connector_group.h"

namespace meteodata
{

ConnectorGroup::ConnectorGroup(boost::asio::io_context& ioContext, DbConnectionObservations& db):
		Connector(ioContext, db)
{}

ConnectorGroup::~ConnectorGroup()
{
	ConnectorGroup::stop();
}

std::string ConnectorGroup::getStatus()
{
	cleanupExpiredConnectors();

	std::ostringstream os;
	for (auto&& c : _connectors) {
		if (std::shared_ptr<Connector> cc = c.lock()) {
			os << cc->getStatus() << "\n";
		}
	}
	return os.str();
}

void ConnectorGroup::start()
{
	cleanupExpiredConnectors();
	for (auto&& c : _connectors) {
		if (std::shared_ptr<Connector> cc = c.lock()) {
			cc->start();
		}
	}
}

void ConnectorGroup::stop()
{
	cleanupExpiredConnectors();
	for (auto&& c : _connectors) {
		if (std::shared_ptr<Connector> cc = c.lock()) {
			cc->stop();
		}
	}
}

void ConnectorGroup::reload()
{
	cleanupExpiredConnectors();
	for (auto&& c : _connectors) {
		if (std::shared_ptr<Connector> cc = c.lock()) {
			cc->reload();
		}
	}
}

void ConnectorGroup::addConnector(const std::weak_ptr<Connector>& connector)
{
	_connectors.push_back(connector);
}

void ConnectorGroup::cleanupExpiredConnectors()
{
	_connectors.erase(
		std::remove_if(_connectors.begin(), _connectors.end(), std::mem_fn(&std::weak_ptr<Connector>::expired)),
		_connectors.end()
	);
}

}
