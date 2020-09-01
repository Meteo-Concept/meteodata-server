/**
 * @file fieldclimate_api_download_scheduler.cpp
 * @brief Implementation of the FieldClimateApiDownloadScheduler class
 * @author Laurent Georget
 * @date 2020-09-02
 */
/*
 * Copyright (C) 2020  SAS JD Environnement <contact@meteo-concept.fr>
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
#include <syslog.h>
#include <unistd.h>

#include <boost/system/error_code.hpp>
#include <boost/asio/basic_waitable_timer.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <date/date.h>
#include <cassandra.h>
#include <dbconnection_observations.h>

#include "../time_offseter.h"
#include "fieldclimate_api_download_scheduler.h"
#include "fieldclimate_api_downloader.h"
#include "../http_utils.h"
#include "../blocking_tcp_client.h"

namespace asio = boost::asio;
namespace ip = boost::asio::ip;
namespace sys = boost::system;
namespace chrono = std::chrono;
namespace args = std::placeholders;

namespace meteodata {

constexpr char FieldClimateApiDownloadScheduler::APIHOST[];
constexpr int FieldClimateApiDownloadScheduler::POLLING_PERIOD;


using namespace date;

FieldClimateApiDownloadScheduler::FieldClimateApiDownloadScheduler(
	asio::io_service& ioService, DbConnectionObservations& db,
	const std::string& apiId, const std::string& apiSecret
	) :
	_ioService{ioService},
	_db{db},
	_apiId{apiId},
	_apiSecret{apiSecret},
	_timer{ioService}
{
}

asio::ssl::context FieldClimateApiDownloadScheduler::createSslContext()
{
	asio::ssl::context sslContext{asio::ssl::context::sslv23};
	sslContext.set_default_verify_paths();
	return sslContext;
}

void FieldClimateApiDownloadScheduler::add(
	const CassUuid& station, const std::string& fieldClimateId,
	TimeOffseter::PredefinedTimezone tz,
	const std::map<std::string, std::string> sensors
) {
	_downloaders.emplace_back(std::make_shared<FieldClimateApiDownloader>(station, fieldClimateId, sensors, _db, tz, _apiId, _apiSecret));
}

void FieldClimateApiDownloadScheduler::start()
{
	reloadStations();
	waitUntilNextDownload();
}

void FieldClimateApiDownloadScheduler::connectClient(BlockingTcpClient<asio::ssl::stream<ip::tcp::socket>>& client)
{
	std::cerr << "Resetting" << std::endl;
	client.reset(createSslContext());
	std::cerr << "Reset" << std::endl;
	auto& socket = client.socket();
	if(!SSL_set_tlsext_host_name(socket.native_handle(), APIHOST))
	{
		sys::error_code ec{static_cast<int>(::ERR_get_error()), boost::asio::error::get_ssl_category()};
		throw boost::system::system_error{ec};
	}
	std::cerr << "Connecting" << std::endl;
	client.connect(APIHOST, "https");
	std::cerr << "Connected" << std::endl;

	socket.set_verify_mode(asio::ssl::verify_peer);
	socket.set_verify_callback(asio::ssl::rfc2818_verification(APIHOST));
	socket.handshake(asio::ssl::stream<ip::tcp::socket>::client);
	std::cerr << "Handshaked" << std::endl;
}

void FieldClimateApiDownloadScheduler::downloadArchives()
{
	BlockingTcpClient<asio::ssl::stream<ip::tcp::socket>> sslClient(chrono::seconds(5), createSslContext());
	connectClient(sslClient);
	int retry = 0;
	for (auto it = _downloaders.cbegin() ; it != _downloaders.cend() ; ) {
		genericDownload(sslClient, [it](auto& client) { (*it)->download(client); }, retry);
		if (!retry)
			++it;
	}
}

void FieldClimateApiDownloadScheduler::waitUntilNextDownload()
{
	auto self(shared_from_this());
	constexpr auto realTimePollingPeriod = chrono::minutes(POLLING_PERIOD);
	auto tp = chrono::minutes(realTimePollingPeriod) -
	       (chrono::system_clock::now().time_since_epoch() % chrono::minutes(realTimePollingPeriod));
	_timer.expires_from_now(tp);
	_timer.async_wait(std::bind(&FieldClimateApiDownloadScheduler::checkDeadline, self, args::_1));
}

void FieldClimateApiDownloadScheduler::checkDeadline(const sys::error_code& e)
{
	/* if the timer has been cancelled, then bail out ; we probably have been
	 * asked to die */
	std::cerr << "Deadline handler hit: " << e.value() << ": " << e.message() << std::endl;
	if (e == sys::errc::operation_canceled)
		return;

	// verify that the timeout is not spurious
	if (_timer.expires_at() <= chrono::steady_clock::now()) {
		std::cerr << "Timed out!" << std::endl;

		downloadArchives();
		waitUntilNextDownload();
	} else {
		/* spurious handler call, restart the timer without changing the
		 * deadline */
		auto self(shared_from_this());
		_timer.async_wait(std::bind(&FieldClimateApiDownloadScheduler::checkDeadline, self, args::_1));
	}
}

void FieldClimateApiDownloadScheduler::reloadStations()
{
	_downloaders.clear();

	std::vector<std::tuple<CassUuid, std::string, int, std::map<std::string, std::string>>> fieldClimateStations;
	_db.getAllFieldClimateApiStations(fieldClimateStations);
	for (const auto& station : fieldClimateStations) {
		add(
			std::get<0>(station), std::get<1>(station),
			TimeOffseter::PredefinedTimezone(std::get<2>(station)),
			std::get<3>(station)
		);
	}
}

}
