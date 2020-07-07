/**
 * @file http_utils.h
 * @brief Definition of some handy functions to do common HTTP-related tasks
 * @author Laurent Georget
 * @date 2019-09-16
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

#include <string>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <memory>
#include <stdexcept>

#include <boost/system/error_code.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/streambuf.hpp>
#include <openssl/hmac.h>
#include <openssl/evp.h>

#include "blocking_tcp_client.h"

using std::size_t;

namespace asio = boost::asio;
namespace sys = boost::system;

namespace {

/**
 * Test whether two ASCII strings are equal, disregarding the case
 *
 * This is useful to compare HTML headers.
 * @param str1 The first string, assumed to be a ASCII string
 * @param str2 The second string, assumed to be a ASCII string
 * @return true if, and only if, lowered-case str1 has the same content as
 * lowered-case str2
 */
inline bool compareAsciiCaseInsensitive(const std::string& str1, const std::string& str2)
{
	if (str1.size() != str2.size()) {
		return false;
	}
	for (std::string::const_iterator c1 = str1.begin(), c2 = str2.begin(); c1 != str1.end(); ++c1, ++c2) {
		if (::tolower(*c1) != ::tolower(*c2)) {
			return false;
		}
	}
	return true;
}

/**
 * Compute the SHA256-based HMAC of a string and return the result as a
 * hexadecimal string
 *
 * @param str The string to compute the HMAC for
 * @param key The secret needed to compute the HMAC
 * @return The SHA256-based HMAC of the string, as an hexadecimal string
 */
inline std::string computeHMACWithSHA256(const std::string& str, const std::string& key)
{
	const EVP_MD* sha256 = EVP_sha256();

	unsigned int finalSize = EVP_MAX_MD_SIZE;
	unsigned char rawOutput[EVP_MAX_MD_SIZE + 1];

	unsigned char* result = HMAC(sha256, reinterpret_cast<const unsigned char*>(key.data()), key.length(), reinterpret_cast<const unsigned char*>(str.data()), str.length(), rawOutput, &finalSize);
	if (!result)
		throw std::runtime_error("OpenSSL failed to compute the HMAC for the string: " + str);
	rawOutput[finalSize] = 0;

	std::ostringstream output;
	output << std::hex;
	for (unsigned int i=0 ; i<finalSize ; i++)
		output << std::setw(2) << std::setfill('0') << (int)rawOutput[i];
	return output.str();
}

}

/**
 * Get the response for a HTTP 1.0 query made over a ASIO socket (ip::tcp::socket or ssl::stream)
 *
 * The idea is to receive the response into the buffer in parameter, to validate the status of the
 * query (which should be 200/OK) and the value of the headers. After this function terminates, the
 * response is in the response buffer and the response stream iterator is at the beginning of the
 * body content.
 *
 * @template Socket The type of the ASIO socket
 * @param socket The socket on which the request was made, and from which the response is expected
 * @param response The buffer into which the response will be received
 * @param responseStream The input iterator on the response
 * @param maxSize The maximum size of the response buffer
 * @param expectedMimeType The type of the response, to validate against the Content-Type: header
 *
 * @return True if, and only if, the "Connection: close" header was sent by the other header
 */
template<typename Socket>
bool getReponseFromHTTP10Query(Socket& socket, boost::asio::streambuf& response, std::istream& responseStream, std::size_t maxSize, const std::string& expectedMimeType)
{
	asio::read_until(socket, response, "\r\n");

	std::string httpVersion;
	responseStream >> httpVersion;
	unsigned int statusCode;
	responseStream >> statusCode;
	std::string statusMessage;
	std::getline(responseStream, statusMessage);

	if (!responseStream || httpVersion.substr(0, 5) != "HTTP/")
		throw std::runtime_error("Not a HTTP answer (was there anything left in the buffer?)");

	if (statusCode != 200)
		throw std::runtime_error("Status code is " + std::to_string(statusCode));

	// Read the response headers, which are terminated by a blank line.
	// TODO validate the size?
	[[ maybe_unused ]] std::size_t headersSize = asio::read_until(socket, response, "\r\n\r\n");

	// Process the response headers.
	std::string header;
	std::istringstream fromheader;
	std::string field;
	std::size_t size = 0;
	std::string connectionStatus;
	std::string type;
	while (std::getline(responseStream, header) && header != "\r") {
		fromheader.str(header);
		fromheader >> field;
		std::cerr << "Header: " << field << std::endl;
		if (compareAsciiCaseInsensitive(field, "content-length:")) {
			fromheader >> size;
			if (size == 0 || size >= maxSize)
				throw std::runtime_error("No content in response or too long");
		} else if (compareAsciiCaseInsensitive(field, "connection:")) {
			fromheader >> connectionStatus;
		} else if (compareAsciiCaseInsensitive(field, "content-type:") && !expectedMimeType.empty()) {
			fromheader >> type;
			if (!compareAsciiCaseInsensitive(type, expectedMimeType))
				throw std::runtime_error("Not the expected type in answer");
		}
	}

	// Read the response body
	sys::error_code ec;
	std::cerr << "We are expecting " << size << " bytes, the buffer contains " << response.size() << " bytes." << std::endl;
	if (size == 0) {
		if (compareAsciiCaseInsensitive(connectionStatus, "close")) {
			// The server has closed the connection, read until EOF
			asio::read(socket, response, asio::transfer_all(), ec);
		} else {
			throw std::runtime_error("No content in response or too long");
		}
	} else if (response.size() < size) {
		asio::read(socket, response, asio::transfer_at_least(size - response.size()), ec);
	}

	if (ec && ec != asio::error::eof)
		throw std::runtime_error("Not enough content in response");
	if (response.size() < size)
		throw std::runtime_error("Not enough content in response");

	return connectionStatus != "close";
}

template<typename Socket>
bool getReponseFromHTTP10QueryFromClient(meteodata::BlockingTcpClient<Socket>& client, boost::asio::streambuf& response, std::istream& responseStream, std::size_t maxSize, const std::string& expectedMimeType)
{
	std::size_t bytesReadInFirstLine;
	client.read_until(response, "\r\n", bytesReadInFirstLine);

	std::string httpVersion;
	responseStream >> httpVersion;
	unsigned int statusCode;
	responseStream >> statusCode;
	std::string statusMessage;
	std::getline(responseStream, statusMessage);

	if (bytesReadInFirstLine == 0)
		throw sys::system_error(asio::error::eof);

	if (!responseStream || httpVersion.substr(0, 5) != "HTTP/")
		throw std::runtime_error("Not a HTTP answer (was there anything left in the buffer?)");

	if (statusCode != 200)
		throw std::runtime_error("Status code is " + std::to_string(statusCode));

	// Read the response headers, which are terminated by a blank line.
	// TODO validate the size?
	std::size_t bytesReadInHeader;
	client.read_until(response, "\r\n\r\n", bytesReadInHeader);

	// Process the response headers.
	std::string header;
	std::istringstream fromheader;
	std::string field;
	std::size_t size = 0;
	std::string connectionStatus;
	std::string type;
	while (std::getline(responseStream, header) && header != "\r") {
		fromheader.str(header);
		fromheader >> field;
		std::cerr << "Header: " << field << std::endl;
		if (compareAsciiCaseInsensitive(field, "content-length:")) {
			fromheader >> size;
			if (size == 0 || size >= maxSize)
				throw std::runtime_error("No content in response or too long");
		} else if (compareAsciiCaseInsensitive(field, "connection:")) {
			fromheader >> connectionStatus;
		} else if (compareAsciiCaseInsensitive(field, "content-type:") && !expectedMimeType.empty()) {
			fromheader >> type;
			if (!compareAsciiCaseInsensitive(type, expectedMimeType))
				throw std::runtime_error("Not the expected type in answer");
		}
	}

	// Read the response body
	sys::error_code ec;
	std::cerr << "We are expecting " << size << " bytes, the buffer contains " << response.size() << " bytes." << std::endl;
	std::size_t bytesReadInBody = 0; // not very relevant since we can check the response size directly
	if (size == 0) {
		if (compareAsciiCaseInsensitive(connectionStatus, "close")) {
			// The server has closed the connection, read until EOF
			client.read_all(response, bytesReadInBody, ec);
		} else {
			throw std::runtime_error("No content in response or too long");
		}
	} else if (response.size() < size) {
		client.read_at_least(response, size - response.size(), bytesReadInHeader, ec);
	}

	if (ec && ec != asio::error::eof)
		throw std::runtime_error("Not enough content in response");
	if (response.size() < size)
		throw std::runtime_error("Not enough content in response");

	return connectionStatus != "close";
}
