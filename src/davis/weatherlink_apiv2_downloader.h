/**
 * @file weatherlink_apiv2_downloader.h
 * @brief Definition of the WeatherlinkApiv2Downloader class
 * @author Laurent Georget
 * @date 2019-09-18
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

#ifndef WEATHERLINK_APIV2_DOWNLOADER_H
#define WEATHERLINK_APIV2_DOWNLOADER_H

#include <iostream>
#include <memory>
#include <vector>

#include <boost/system/error_code.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <dbconnection_observations.h>

#include "../time_offseter.h"
#include "abstract_weatherlink_downloader.h"
#include "weatherlink_apiv2_archive_message.h"
#include "weatherlink_apiv2_realtime_message.h"


namespace meteodata {

namespace ip = boost::asio::ip;
namespace asio = boost::asio;

using namespace meteodata;

/**
 */
class WeatherlinkApiv2Downloader : public AbstractWeatherlinkDownloader
{
public:
	WeatherlinkApiv2Downloader(const CassUuid& station,
		const std::string& weatherlinkId,
		const std::string& apiKey, const std::string& apiSecret,
		asio::io_service& ioService, DbConnectionObservations& db,
		TimeOffseter::PredefinedTimezone tz);
	void download(asio::ssl::stream<ip::tcp::socket>& socket);
	void downloadRealTime(asio::ssl::stream<ip::tcp::socket>& socket);

private:
	std::string _apiKey;
	std::string _apiSecret;

	using Params = std::map<std::string, std::string>; // a map sorted by its keys in asciibetical order

	/**
	 * @brief The Weatherlink station id
	 */
	std::string _weatherlinkId;

	std::string computeApiSignature(const Params& params);
};

}

#endif
