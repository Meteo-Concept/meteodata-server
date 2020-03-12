/**
 * @file weatherlinkdownloadscheduler.cpp
 * @brief Implementation of the WeatherlinkDownloadScheduler class
 * @author Laurent Georget
 * @date 2019-03-08
 */
/*
 * Copyright (C) 2019  SAS JD Environnement <contact@meteo-concept.fr>
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
#include "weatherlink_download_scheduler.h"
#include "weatherlink_downloader.h"

namespace asio = boost::asio;
namespace ip = boost::asio::ip;
namespace sys = boost::system;
namespace chrono = std::chrono;
namespace args = std::placeholders;

namespace meteodata {

constexpr char WeatherlinkDownloadScheduler::HOST[];
constexpr char WeatherlinkDownloadScheduler::APIHOST[];
constexpr int WeatherlinkDownloadScheduler::POLLING_PERIOD;
constexpr int WeatherlinkDownloadScheduler::UNPRIVILEGED_POLLING_PERIOD;


using namespace date;

WeatherlinkDownloadScheduler::WeatherlinkDownloadScheduler(asio::io_service& ioService, DbConnectionObservations& db,
		const std::string& apiId, const std::string& apiSecret) :
	_ioService{ioService},
	_db{db},
	_apiId{apiId},
	_apiSecret{apiSecret},
	_timer{ioService},
	_sslContext{asio::ssl::context::sslv23}
{
	_sslContext.set_default_verify_paths();
}

void WeatherlinkDownloadScheduler::add(const CassUuid& station, const std::string& auth,
	const std::string& apiToken, TimeOffseter::PredefinedTimezone tz)
{
	_downloaders.emplace_back(std::make_shared<WeatherlinkDownloader>(station, auth, apiToken, _ioService, _db, tz));
}

void WeatherlinkDownloadScheduler::addAPIv2(const CassUuid& station, bool archived, const std::string& weatherlinkId,
		TimeOffseter::PredefinedTimezone tz)
{
	_downloadersAPIv2.emplace_back(archived, std::make_shared<WeatherlinkApiv2Downloader>(station, weatherlinkId,
		_apiId, _apiSecret, _ioService, _db, tz));
}

void WeatherlinkDownloadScheduler::start()
{
	waitUntilNextDownload();
}

void WeatherlinkDownloadScheduler::connectSocket(std::unique_ptr<ip::tcp::socket>& socket, const char host[])
{
	socket.reset(new ip::tcp::socket(_ioService));
	ip::tcp::resolver resolver(_ioService);
	ip::tcp::resolver::query query(host, "http");
	ip::tcp::resolver::iterator endpointIterator = resolver.resolve(query);

	boost::asio::connect(*socket, endpointIterator);
}

void WeatherlinkDownloadScheduler::connectSocket(std::unique_ptr<asio::ssl::stream<ip::tcp::socket>>& socket, const char host[])
{
	socket.reset(new asio::ssl::stream<ip::tcp::socket>(_ioService, _sslContext));
	ip::tcp::resolver resolver(_ioService);
	ip::tcp::resolver::query query(host, "https");
	ip::tcp::resolver::iterator endpointIterator = resolver.resolve(query);

        if(!SSL_set_tlsext_host_name(socket->native_handle(), host))
        {
            sys::error_code ec{static_cast<int>(::ERR_get_error()), boost::asio::error::get_ssl_category()};
            throw boost::system::system_error{ec};
        }
	boost::asio::connect(socket->lowest_layer(), endpointIterator);

	socket->set_verify_mode(asio::ssl::verify_peer);
	socket->set_verify_callback(asio::ssl::rfc2818_verification(host));
	socket->handshake(asio::ssl::stream<ip::tcp::socket>::client);
}

void WeatherlinkDownloadScheduler::downloadRealTime()
{
	std::unique_ptr<asio::ssl::stream<ip::tcp::socket>> socket;

	auto now = date::floor<chrono::minutes>(chrono::system_clock::now()).time_since_epoch().count();

	connectSocket(socket, APIHOST);
	int retry = 0;
	for (auto it = _downloaders.cbegin() ; it != _downloaders.cend() ; ) {
		if ((*it)->getPollingPeriod() <= POLLING_PERIOD || now % UNPRIVILEGED_POLLING_PERIOD < POLLING_PERIOD)
			genericDownload(socket, APIHOST, [it](asio::ssl::stream<ip::tcp::socket>& activeSocket) { (*it)->downloadRealTime(activeSocket); }, retry);
		if (!retry)
			++it;
	}

	retry = 0;
	for (auto it = _downloadersAPIv2.cbegin() ; it != _downloadersAPIv2.cend() ; ) {
		if (now % UNPRIVILEGED_POLLING_PERIOD < POLLING_PERIOD)
			genericDownload(socket, APIHOST, [it](asio::ssl::stream<ip::tcp::socket>& activeSocket) { (it->second)->downloadRealTime(activeSocket); }, retry);
		if (!retry)
			++it;
	}
}

void WeatherlinkDownloadScheduler::downloadArchives()
{
	{ // scope of the socket
		std::unique_ptr<ip::tcp::socket> socket;
		connectSocket(socket, HOST);
		int retry = 0;

		for (auto it = _downloaders.cbegin() ; it != _downloaders.cend() ; ) {
			genericDownload(socket, HOST, [it](ip::tcp::socket& activeSocket) { (*it)->download(activeSocket); }, retry);
			if (!retry)
				++it;
		}
	}

	std::unique_ptr<asio::ssl::stream<ip::tcp::socket>> sslSocket;

	connectSocket(sslSocket, APIHOST);
	int retry = 0;
	for (auto it = _downloadersAPIv2.cbegin() ; it != _downloadersAPIv2.cend() ; ) {
		if (it->first) { // only download archives from archived stations
			genericDownload(sslSocket, APIHOST, [it](asio::ssl::stream<ip::tcp::socket>& activeSocket) { (it->second)->download(activeSocket); }, retry);
			if (!retry)
				++it;
		} else {
			++it;
		}
	}
}

void WeatherlinkDownloadScheduler::waitUntilNextDownload()
{
	auto self(shared_from_this());
	constexpr auto realTimePollingPeriod = chrono::minutes(POLLING_PERIOD);
	auto tp = chrono::minutes(realTimePollingPeriod) -
	       (chrono::system_clock::now().time_since_epoch() % chrono::minutes(realTimePollingPeriod));
	_timer.expires_from_now(tp);
	_timer.async_wait(std::bind(&WeatherlinkDownloadScheduler::checkDeadline, self, args::_1));
}

void WeatherlinkDownloadScheduler::checkDeadline(const sys::error_code& e)
{
	/* if the timer has been cancelled, then bail out ; we probably have been
	 * asked to die */
	std::cerr << "Deadline handler hit: " << e.value() << ": " << e.message() << std::endl;
	if (e == sys::errc::operation_canceled)
		return;

	// verify that the timeout is not spurious
	if (_timer.expires_at() <= chrono::steady_clock::now()) {
		std::cerr << "Timed out!" << std::endl;
		auto now = chrono::system_clock::now();
		auto daypoint = date::floor<date::days>(now);
		auto tod = date::make_time(now - daypoint); // Yields time_of_day type

		downloadRealTime();
		if (tod.minutes().count() < POLLING_PERIOD) //will trigger once per hour
			downloadArchives();
		waitUntilNextDownload();
	} else {
		/* spurious handler call, restart the timer without changing the
		 * deadline */
		auto self(shared_from_this());
		_timer.async_wait(std::bind(&WeatherlinkDownloadScheduler::checkDeadline, self, args::_1));
	}
}

}
