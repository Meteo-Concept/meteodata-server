/**
 * @file abstractsynopdownloader.h
 * @brief Definition of the AbstractSynopDownloader class
 * @author Laurent Georget
 * @date 2019-02-20
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

#ifndef ABSTRACTSYNOPDOWNLOADER_H
#define ABSTRACTSYNOPDOWNLOADER_H

#include <iostream>
#include <memory>
#include <array>
#include <vector>
#include <functional>
#include <map>
#include <unistd.h>

#include <boost/system/error_code.hpp>
#include <boost/asio.hpp>
#include <boost/asio/basic_waitable_timer.hpp>
#include <cassandra.h>
#include <date.h>
#include <tz.h>
#include <dbconnection_observations.h>

#include "../connector.h"

namespace meteodata
{

namespace ip = boost::asio::ip;
namespace asio = boost::asio;
namespace sys = boost::system;
namespace chrono = std::chrono;

using namespace std::placeholders;
using namespace meteodata;

/**
 */
class SynopDownloadScheduler : public Connector
{
public:
	SynopDownloadScheduler(asio::io_context& ioContext, DbConnectionObservations& db);
	void start() override;
	void stop() override;
	void reload() override;

	void add(const std::string& group, const chrono::minutes& period, const chrono::hours& backlog);

	/**
	 * The SYNOP country prefix for France
	 */
	static constexpr char GROUP_FR[] = "07";
	/**
	 * The SYNOP country prefix for Luxemburg
	 */
	static constexpr char GROUP_LU[] = "06";

private:
	asio::basic_waitable_timer<chrono::steady_clock> _timer;
	std::map<std::string, CassUuid> _icaos;

	struct SynopGroup {
		std::string prefix;
		chrono::minutes period;
		chrono::hours backlog;
	};
	std::vector<SynopGroup> _groups;
	bool _mustStop = false;

	static constexpr char HOST[] = "www.ogimet.com";
	static constexpr int MINIMAL_PERIOD_MINUTES = 20;
	static constexpr int DELAY_MINUTES = 4;

	void checkDeadline(const sys::error_code& e);
	void waitUntilNextDownload();
	void download(const std::string& group, const chrono::hours& backlog);
	static void buildDownloadRequest(std::ostream& out, const std::string& group, const chrono::hours& backlog);
	void reloadStations();
};

}

#endif
