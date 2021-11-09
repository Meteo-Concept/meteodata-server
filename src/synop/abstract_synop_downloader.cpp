/**
 * @file abstractsynopdownloader.cpp
 * @brief Implementation of the AbstractSynopDownloader class
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

#include <iostream>
#include <memory>
#include <functional>
#include <iterator>
#include <chrono>
#include <map>
#include <sstream>
#include <iomanip>
#include <exception>

#include <cstring>
#include <cctype>
#include <unistd.h>
#include <systemd/sd-daemon.h>

#include <boost/system/error_code.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/basic_waitable_timer.hpp>
#include <dbconnection_observations.h>

#include "synop_downloader.h"
#include "ogimet_synop.h"
#include "synop_decoder/parser.h"
#include "../http_utils.h"
#include "../curl_wrapper.h"
#include "../cassandra_utils.h"

namespace asio = boost::asio;
namespace sys = boost::system; //system() is a function, it cannot be redefined
//as a namespace
namespace chrono = std::chrono;
namespace args = std::placeholders;

namespace meteodata {

using namespace date;

constexpr char AbstractSynopDownloader::HOST[];

AbstractSynopDownloader::AbstractSynopDownloader(asio::io_service& ioService, DbConnectionObservations& db) :
	_ioService(ioService),
	_db(db),
	_timer(_ioService)
{
}

void AbstractSynopDownloader::waitUntilNextDownload()
{
	auto self(shared_from_this());
	_timer.expires_from_now(computeWaitDuration());
	_timer.async_wait(std::bind(&AbstractSynopDownloader::checkDeadline, self, args::_1));
}

void AbstractSynopDownloader::stop()
{
    std::cout << SD_NOTICE << "[SYNOP] connection: "
        << "Stopping activity" << std::endl;

    /* signal that the downloader should not go to sleep after its
     * current download (if any) but rather die */
    _mustStop = true;
    /* cancel the timer, this will let the downloader be destroyed since
     * it will run out of activity in the checkDeadline() method below */
    _timer.cancel();
}

void AbstractSynopDownloader::checkDeadline(const sys::error_code& e)
{
	/* if the timer has been cancelled, then bail out ; we probably have been
	 * asked to die */
	if (e == sys::errc::operation_canceled)
		return;

	// verify that the timeout is not spurious
	if (_timer.expires_at() <= chrono::steady_clock::now()) {
		try {
			download();
		} catch (std::exception& e) {
			std::cerr << SD_ERR << "[SYNOP] protocol: "
			    << "Getting the SYNOP messages failed (" << e.what() << "), will retry"
				<< std::endl;
			// nothing more, just go back to sleep and retry next time
		}
		// Going back to sleep unless we're not supposed too
		if (!_mustStop)
            waitUntilNextDownload();
	} else {
		/* spurious handler call, restart the timer without changing the
		 * deadline */
		auto self(shared_from_this());
		_timer.async_wait(std::bind(&AbstractSynopDownloader::checkDeadline, self, args::_1));
	}
}

void AbstractSynopDownloader::download()
{
	std::cout << SD_INFO << "[SYNOP] measurement: "
	    << "Now downloading SYNOP messages " << std::endl;

	std::ostringstream requestStream;
	buildDownloadRequest(requestStream);

	CurlWrapper client;

	CURLcode ret = client.download(std::string{"http://"} + HOST + requestStream.str(), [&](const std::string& body) {
		std::istringstream bodyIterator(body);

		std::string line;
		while (std::getline(bodyIterator, line)) {
			std::istringstream lineIterator{line};

			Parser parser;
			if (parser.parse(lineIterator)) {
				const SynopMessage& m = parser.getDecodedMessage();
				auto uuidIt = _icaos.find(m._stationIcao);
				if (uuidIt != _icaos.end()) {
					char uuidStr[CASS_UUID_STRING_LENGTH];
					const CassUuid& station = uuidIt->second;
					cass_uuid_string(station, uuidStr);

					std::string stationName;
					int pollingPeriod;
					time_t lastArchiveDownloadTime;
					_db.getStationDetails(station, stationName, pollingPeriod, lastArchiveDownloadTime);
					float latitude, longitude;
					int elevation;
					_db.getStationLocation(station, latitude, longitude, elevation);
					TimeOffseter timeOffseter = TimeOffseter::getTimeOffseterFor(TimeOffseter::PredefinedTimezone::UTC);
					timeOffseter.setLatitude(latitude);
					timeOffseter.setLongitude(longitude);
					timeOffseter.setElevation(elevation);
					timeOffseter.setMeasureStep(pollingPeriod);

					OgimetSynop synop{m, &timeOffseter};
					_db.insertV2DataPoint(station, synop);
					std::cout << SD_DEBUG << "[SYNOP] measurement: "
					    << "Inserted into database" << std::endl;

					std::pair<bool, float> rainfall24 = std::make_pair(false, 0.f);
					std::pair<bool, int> insolationTime24 = std::make_pair(false, 0);
					auto it = std::find_if(m._precipitation.begin(), m._precipitation.end(),
							[](const auto& p) { return p._duration == 24; });
					if (it != m._precipitation.end())
						rainfall24 = std::make_pair(true, it->_amount);
					if (m._minutesOfSunshineLastDay)
						insolationTime24 = std::make_pair(true, *m._minutesOfSunshineLastDay);
					auto day = date::floor<date::days>(m._observationTime) - date::days(1);
					_db.insertV2EntireDayValues(station, date::sys_seconds(day).time_since_epoch().count(), rainfall24, insolationTime24);
					if (m._minTemperature)
						_db.insertV2Tn(station, chrono::system_clock::to_time_t(m._observationTime), *m._minTemperature / 10.f);
					if (m._maxTemperature)
						_db.insertV2Tx(station, chrono::system_clock::to_time_t(m._observationTime), *m._maxTemperature / 10.f);
				}
			} else {
				std::cerr << SD_WARNING << "[SYNOP] measurement: "
				    << "Record looks invalid, discarding..." << std::endl;
			}
		}
	});

	if (ret != CURLE_OK) {
		std::string_view error = client.getLastError();
		std::cerr << SD_ERR << "[SYNOP] protocol: "
		    << "Failed to download SYNOPs: " << error << std::endl;
	}
}

}
