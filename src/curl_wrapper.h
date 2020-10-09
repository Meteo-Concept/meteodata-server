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

class CurlWrapper {
public:
	CurlWrapper();

	void setHeader(const std::string& header, const std::string& value);
	CURLcode download(const std::string& url, std::function<void(const std::string&)> parser);
	std::string_view getLastError();

	using CurlHandle = std::unique_ptr<CURL, decltype(&curl_easy_cleanup)>;
	using CurlHeadersList = std::unique_ptr<curl_slist, decltype(&curl_slist_free_all)>;

private:
	CurlHandle _handle;
	CurlHeadersList _headers;
	char _errorBuffer[CURL_ERROR_SIZE];
	std::string _buffer;

	static std::size_t receiveData(void* buffer, std::size_t sie, std::size_t nbemb, void* userp);
};

}

#endif
