/**
 * @file curl_wrapper.h
 * @brief Definition of a C++ wrapper class for CURL handles
 * @author Laurent Georget
 * @date 2020-09-28
 */
/*
 * Copyright (C) 2020  SAS Météo Concept <contact@meteo-concept.fr>
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
#ifndef CURL_WRAPPER_H
#define CURL_WRAPPER_H

#include <string>
#include <string_view>
#include <iostream>
#include <functional>
#include <memory>

#include <curl/curl.h>
#include <curl/easy.h>

namespace meteodata {

/**
 * @brief Ultra-simple curl wrapper for simple HTTP queries
 */
class CurlWrapper {
public:
    /**
     * @brief Construct the wrapper
     */
	CurlWrapper();

	/**
	 * @brief Set or reset a header for the next query
	 *
	 * Headers are not stored from one query to the next so the caller must set all appropriate headers for each query.
	 * Curl sets by default some headers like Host and Date if they are not set explicitly. It's possible to remove
	 * them with this method by passing an empty value.
	 * @param header The name of the HTTP header
	 * @param value The value for this header, can be empty to reset the header (i.e. remove it from the query)
	 */
	void setHeader(const std::string& header, const std::string& value);

	/**
	 * @brief Start the download and call a callback when it's done
	 *
	 * @param url The URL to query
	 * @param parser The callback to call with the data when the query is done. The parser is called only if the query
	 * is successful (no curl errors and HTTP status code in the 2xx range)
	 * @return The curl result/error code for the query (https://curl.se/libcurl/c/libcurl-errors.html)
	 */
	CURLcode download(const std::string& url, std::function<void(const std::string&)> parser);

	/**
	 * @brief Get the last error message returned by curl
	 * @return The last error message written by curl in its error buffer (https://curl.se/libcurl/c/CURLOPT_ERRORBUFFER.html)
	 */
	std::string_view getLastError();

	/**
	 * @brief Get the HTTP code of the last request made by curl
	 * @return The last HTTP code received, or 0 if no request has already been made
	 */
	long getLastRequestCode();

private:
    /**
     * @brief An alias for the curl library bare handle
     */
    using CurlHandle = std::unique_ptr<CURL, decltype(&curl_easy_cleanup)>;
    /**
     * @brief An alias for the curl library headers list type
     */
    using CurlHeadersList = std::unique_ptr<curl_slist, decltype(&curl_slist_free_all)>;

    /**
     * @brief A curl object to call curl functions upon
     */
    CurlHandle _handle;
    /**
     * @brief The list of HTTP headers to pass to the next query
     */
	CurlHeadersList _headers;
	/**
	 * @brief A buffer big enough to store curl error messages
	 */
	char _errorBuffer[CURL_ERROR_SIZE];
	/**
	 * @brief The buffer where the output from curl can be appended, cleared before and after each query
	 */
	std::string _buffer;

	/**
	 * @brief A callback passed to curl to output the downloaded data to _buffer
	 *
	 * @see https://curl.se/libcurl/c/CURLOPT_WRITEFUNCTION.html
	 * @param buffer The buffer where curl has put new data for us to consume
	 * @param size The size of each chunk of data available in curl's buffer (it's always 1 actually)
	 * @param nbemb The number of chunks of data in curl's buffer
	 * @param userp A pointer to the destination (we set it to the _buffer member of the instance doing the query)
	 * @return The number of bytes transferred from curl's buffer to our own _buffer
	 */
	static std::size_t receiveData(void* buffer, std::size_t size, std::size_t nbemb, void* userp);
};

}

#endif
