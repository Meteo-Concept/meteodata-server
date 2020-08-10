/**
 * @file blocking_tcp_client.h
 * @brief Definition of the BlockingTcpClient class
 * @author Laurent Georget
 * @date 2020-07-02
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
#ifndef BLOCKING_TCP_CLIENT_H
#define BLOCKING_TCP_CLIENT_H

#include <iostream>
#include <memory>
#include <functional>

#include <boost/system/error_code.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/basic_waitable_timer.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/system/system_error.hpp>
#include <boost/asio/write.hpp>

namespace asio = boost::asio;
namespace ip = boost::asio::ip;
namespace sys = boost::system;
namespace chrono = std::chrono;

namespace meteodata
{
	/**
	 * @brief A Proxy class around Boost ASIO socket to handle timeouts
	 *
	 * This class is parametric in order to support different socket types
	 * as long as the basic timeout functionnality remains the same. It works
	 * with SSL sockets for instance.
	 */
	template<typename Socket>
	class BlockingTcpClient
	{
	private:
		/**
		 * @brief The Boost::Asio service to use for asynchronous
		 * operations
		 */
		asio::io_service _ioService;
		/**
		 * @brief The TCP socket used to communicate to the meteo
		 * station
		 */
		Socket _socket;

		asio::basic_waitable_timer<chrono::steady_clock> _timer;
		/**
		 * @brief The default timeout delay to use when no delay is
		 * provided by the caller
		 */
		chrono::milliseconds _default;

		void checkDeadline(const sys::error_code& e, sys::error_code& returnCode)
		{
			/* if the timer has been cancelled, then bail out, we have nothing more
			 * to do here. It's our original caller's responsability to restart us
			 * if needs be */
			std::cerr << "Deadline hit! " << e << std::endl;
			if (e == sys::errc::operation_canceled)
				return;

			// verify that the timeout is not spurious
			if (_timer.expires_at() <= chrono::steady_clock::now()) {
				/* We have timed out, all operations on the socket are cancelled
				 * Their handlers will be called and will need to handle the
				 * specific "operation_cancelled" error code. */
				_socket.lowest_layer().cancel();
				returnCode = asio::error::timed_out;
			} else {
				/* spurious handler call, restart the timer without changing the
				 * deadline */
				_timer.async_wait([this, &returnCode](const sys::error_code& e) {
					checkDeadline(e, returnCode);
				});
			}
		}

	public:
		/**
		 * @brief Construct a client
		 *
		 * @param socket The Boost::Asio socket to use for all I/O
		 */
		template<class ... SocketConstructionArgs>
		BlockingTcpClient(chrono::milliseconds defaultDelay, SocketConstructionArgs ... args) :
			_socket{_ioService, args...},
			_timer{_ioService},
			_default{defaultDelay}
		{
		}

		/**
		 * @brief Give back the TCP socket passed to this client
		 *
		 * @return The socket used to make I/O
		 */
		Socket& socket() { return _socket; }

		//template<typename ... Args>
		//using AsyncCallable = std::function<void(sys::error_code&, Args...)>;

		template<typename AsyncCallable>
		void asyncOperation(AsyncCallable&& operation, const chrono::milliseconds& delay, sys::error_code& opReturnCode)
		{
			// This code is guaranteed never to be returned by an async call
			// so we use it as a sentinel to know when the async handler has finished
			opReturnCode = asio::error::would_block;

			// It's the responsability of the handler of the async operation
			// to update the error code when said operation is finished.
			// The caller must make sure it has access to it, we won't pass
			// it back to the operation.
			operation();

			_timer.expires_from_now(delay);
			_timer.async_wait([this, &opReturnCode](const sys::error_code& e) {
				checkDeadline(e, opReturnCode);
			});

			do {
				// Loop until we make sure we have run the
				// operation handler (in the worst of case, we
				// wait until the timeout handler cancels the
				// async operation).
				_ioService.run_one();
			} while (opReturnCode == sys::errc::operation_would_block);
		}

		template<class ... SocketConstructionArgs>
		void reset(SocketConstructionArgs... args)
		{
			auto& s = _socket.lowest_layer();
			if (s.is_open()) {
				_socket.lowest_layer().shutdown(asio::socket_base::shutdown_both);
				_socket.lowest_layer().close();
			}
			_socket.~Socket();
			new (&_socket) Socket(_ioService, args...);
		}



		void connect(const std::string& name, const std::string& scheme,
			const chrono::milliseconds& delay, sys::error_code& error)
		{
			auto connectHandler = [this, &error](const sys::error_code& ec, const auto& /* endpoint */){
				_timer.cancel();
				std::cerr << "Connected? " << ec << std::endl;
				error = ec;
			};
				std::cerr << "Delay? " << delay.count() << std::endl;

			auto operation = [&]() {
				ip::tcp::resolver resolver(_ioService);
				ip::tcp::resolver::query query(name, scheme);
				ip::tcp::resolver::iterator endpointIterator = resolver.resolve(query);

				asio::async_connect(_socket.lowest_layer(), endpointIterator, connectHandler);
			};

			asyncOperation(operation, delay, error);
		}

		void connect(const std::string& name, const std::string& scheme, sys::error_code& ec)
		{
			connect(name, scheme, _default, ec);
		}

		sys::error_code connect(const std::string& name, const std::string& scheme)
		{
			sys::error_code ec;
			connect(name, scheme, _default, ec);
			if (ec)
				throw sys::system_error{ec};
			return ec;
		}

		sys::error_code connect(const std::string& name, const std::string& scheme, const chrono::milliseconds& delay)
		{
			return connect(name, scheme, delay);
		}



		void write(asio::streambuf& request, std::size_t& written,
			const chrono::milliseconds& delay, sys::error_code& error)
		{
			auto writeHandler = [&](const sys::error_code& ec, std::size_t bytes_transferred) {
				_timer.cancel();
				written = bytes_transferred;
				error = ec;
			};

			auto operation = [this, &request, &writeHandler]() {
				asio::async_write(_socket, request, writeHandler);
			};

			asyncOperation(operation, delay, error);
		}

		void write(asio::streambuf& request, std::size_t& written, sys::error_code& ec)
		{
			write(request, written, _default, ec);
		}

		sys::error_code write(asio::streambuf& request, std::size_t& written, const chrono::milliseconds& delay)
		{
			sys::error_code ec;
			write(request, written, delay, ec);
			if (ec)
				throw sys::system_error{ec};
			return ec;
		}

		sys::error_code write(asio::streambuf& request, std::size_t& written)
		{
			return write(request, written, _default);
		}



		class ReadHandler {
		private:
			BlockingTcpClient<Socket>& _self;
			std::size_t& _read;
			sys::error_code& _error;

		public:
			ReadHandler(BlockingTcpClient<Socket>& self, std::size_t& read, sys::error_code& e):
				_self(self),
				_read(read),
				_error(e)
			{}

			void operator()(const sys::error_code& ec, std::size_t bytes_transferred) {
				_self._timer.cancel();
				_read = bytes_transferred;
				_error = ec;
			}
		};

		void read_until(asio::streambuf& response, const std::string& delimiter, std::size_t& read,
			const chrono::milliseconds& delay, sys::error_code& error)
		{
			auto operation = [&]() {
				asio::async_read_until(_socket, response, delimiter, ReadHandler(*this, read, error));
			};

			asyncOperation(operation, delay, error);
		}

		void read_until(asio::streambuf& response, const std::string& delimiter, std::size_t& read,
			sys::error_code& error)
		{
			read_until(response, delimiter, read, _default, error);
		}

		sys::error_code read_until(asio::streambuf& response, const std::string& delimiter,
			std::size_t& read, const chrono::milliseconds& delay, bool throwOnEof = false)
		{
			sys::error_code ec;
			read_until(response, delimiter, read, delay, ec);

			// Do not throw in case of EOF, that's usually not an error
			if (ec && (throwOnEof || ec != asio::error::eof))
				throw sys::system_error{ec};
			return ec;
		}

		sys::error_code read_until(asio::streambuf& response, const std::string& delimiter,
			std::size_t& read, bool throwOnEof = false)
		{
			return read_until(response, delimiter, read, _default, throwOnEof);
		}



		void read_all(asio::streambuf& response, std::size_t& read,
			const chrono::milliseconds& delay, sys::error_code& error)
		{
			auto operation = [&]() {
				asio::async_read(_socket, response, asio::transfer_all(), ReadHandler(*this, read, error));
			};

			asyncOperation(operation, delay, error);
		}

		void read_all(asio::streambuf& response, std::size_t& read, sys::error_code& error)
		{
			read_all(response, read, _default, error);
		}

		sys::error_code read_all(asio::streambuf& response, const chrono::milliseconds& delay, std::size_t& read, bool throwOnEof = false)
		{
			sys::error_code ec;
			read_all(response, read, delay, ec);
			if (ec && (throwOnEof || ec != asio::error::eof))
				throw sys::system_error{ec};
			return ec;
		}

		sys::error_code read_all(asio::streambuf& response, std::size_t& read, bool throwOnEof = false)
		{
			return read_all(response, read, _default, throwOnEof);
		}



		void read_at_least(asio::streambuf& response, const unsigned& length,
			std::size_t& read, const chrono::milliseconds& delay, sys::error_code& error)
		{
			auto operation = [&]() {
				asio::async_read(_socket, response, asio::transfer_at_least(length), ReadHandler(*this, read, error));
			};

			asyncOperation(operation, delay, error);
		}

		void read_at_least(asio::streambuf& response, const unsigned& length,
			std::size_t& read, sys::error_code& error)
		{
			read_at_least(response, length, read, _default, error);
		}

		sys::error_code read_at_least(asio::streambuf& response, const unsigned& length,
			const chrono::milliseconds& delay, std::size_t& read, bool throwOnEof = false)
		{
			sys::error_code ec;
			read_at_least(response, length, read, delay, ec);
			if (ec && (throwOnEof || ec != asio::error::eof))
				throw sys::system_error{ec};
			return ec;
		}

		sys::error_code read_at_least(asio::streambuf& response, const unsigned& length,
			std::size_t& read, bool throwOnEof = false)
		{
			return read_at_least(response, length, read, _default, throwOnEof);
		}
	};

}

#endif // CONNECTOR_H
