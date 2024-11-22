/**
 * @file mbdata_download_scheduler.cpp
 * @brief Implementation of the MBDataDownloadScheduler class
 * @author Laurent Georget
 * @date 2022-08-01
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

#include <iostream>
#include <chrono>
#include <thread>

#include <systemd/sd-daemon.h>
#include <boost/asio/basic_waitable_timer.hpp>
#include <cassandra.h>
#include <cassobs/dbconnection_observations.h>

#include "../time_offseter.h"
#include "mbdata_download_scheduler.h"
#include "mbdata_txt_downloader.h"
#include "../abstract_download_scheduler.h"

namespace chrono = std::chrono;

namespace meteodata
{

using namespace date;

MBDataDownloadScheduler::MBDataDownloadScheduler(asio::io_context& ioContext, DbConnectionObservations& db) :
	AbstractDownloadScheduler{chrono::minutes{POLLING_PERIOD}, ioContext, db}
{
}

void MBDataDownloadScheduler::add(const std::tuple<CassUuid, std::string, std::string, bool, int, std::string>& downloadDetails)
{
	_downloaders.emplace_back(std::make_shared<MBDataTxtDownloader>(_ioContext, _db, downloadDetails));
}

void MBDataDownloadScheduler::download()
{
	for (const auto& _downloader : _downloaders) {
		try {
			_downloader->download(_client);
		} catch (const std::runtime_error& e) {
			std::cerr << SD_ERR << "[MBData] protocol: " << "Runtime error, impossible to download " << e.what()
					  << ", moving on..." << std::endl;
		}
	}
}

void MBDataDownloadScheduler::reloadStations()
{
	_downloaders.clear();

	std::vector<std::tuple<CassUuid, std::string, std::string, bool, int, std::string>> mbDataTxtStations;
	_db.getMBDataTxtStations(mbDataTxtStations);
	for (const auto& station : mbDataTxtStations) {
		add(station);
	}
}

}
