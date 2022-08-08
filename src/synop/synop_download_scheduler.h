/**
 * @file synop_download_scheduler.h
 * @brief Definition of the SynopDownloadScheduler class
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

#ifndef SYNOP_DOWNLOAD_SCHEDULER_H
#define SYNOP_DOWNLOAD_SCHEDULER_H

#include <iostream>
#include <vector>
#include <map>

#include <cassandra.h>
#include <date.h>
#include <tz.h>

#include "../abstract_download_scheduler.h"

namespace meteodata
{
namespace chrono = std::chrono;

/**
 */
class SynopDownloadScheduler : public AbstractDownloadScheduler
{
public:
	SynopDownloadScheduler(asio::io_context& ioContext, DbConnectionObservations& db);

private:
	std::map<std::string, CassUuid> _icaos;

	struct SynopGroup {
		std::string prefix;
		chrono::minutes period;
		chrono::hours backlog;
	};
	std::vector<SynopGroup> _groups;

	static constexpr char HOST[] = "www.ogimet.com";
	static constexpr int MINIMAL_PERIOD_MINUTES = 20;
	/**
	 * The SYNOP country prefix for France
	 */
	static constexpr char GROUP_FR[] = "07";
	/**
	 * The SYNOP country prefix for Luxemburg
	 */
	static constexpr char GROUP_LU[] = "06";

	void download() override;
	void downloadGroup(const std::string& group, const chrono::hours& backlog);
	static void buildDownloadRequest(std::ostream& out, const std::string& group, const chrono::hours& backlog);

	void add(const std::string& group, const chrono::minutes& period, const chrono::hours& backlog);
	void reloadStations() override;
};

}

#endif
