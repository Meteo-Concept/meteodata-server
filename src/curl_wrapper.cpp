/**
 * @file curl_wrapper.cpp
 * @brief Implementation of a C++ wrapper class for CURL handles
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

#include <string>
#include <iostream>
#include <functional>
#include <string_view>

#include <curl/curl.h>
#include <curl/easy.h>

#include "curl_wrapper.h"

namespace meteodata {

CurlWrapper::CurlWrapper() :
	_handle(curl_easy_init(), &curl_easy_cleanup),
	_headers(nullptr, &curl_slist_free_all)
{
	curl_easy_setopt(_handle.get(), CURLOPT_ERRORBUFFER, _errorBuffer);
	curl_easy_setopt(_handle.get(), CURLOPT_WRITEFUNCTION, &CurlWrapper::receiveData);
	curl_easy_setopt(_handle.get(), CURLOPT_WRITEDATA, &_buffer);
	curl_easy_setopt(_handle.get(), CURLOPT_FAILONERROR, 1L);
}

void CurlWrapper::setHeader(const std::string& header, const std::string& value)
{
	std::string h = header + ":";
	if (!value.empty())
		h += " " + value;

	curl_slist* newHeaders = curl_slist_append(_headers.get(), h.data());
	if (newHeaders) {
		// The list currently owned by _headers is a sublist of the list
		// stored in newHeaders, it must not be freed. Instead, we
		// release ownership on the sublist and claim ownership on the
		// whole new list.
		// release() is noexcept, so this is memory-safe. (?)
		_headers.release();
		_headers.reset(newHeaders);
	} else {
		if (value.empty())
			throw std::runtime_error("Couldn't reset header " + header);
		else
			throw std::runtime_error("Couldn't set header " + header + " with value " + value);
	}
}

CURLcode CurlWrapper::download(const std::string& url, std::function<void(const std::string&)> parser)
{
	if (_headers)
		curl_easy_setopt(_handle.get(), CURLOPT_HTTPHEADER, _headers.get());
	curl_easy_setopt(_handle.get(), CURLOPT_URL, url.data());

	_buffer.clear();
	CURLcode res = curl_easy_perform(_handle.get());
	_headers.release();
	_headers.reset();
	if (res == CURLE_OK)
		parser(_buffer);
	_buffer.clear();
	return res;
}

std::string_view CurlWrapper::getLastError()
{
	return std::string_view(_errorBuffer);
}

std::size_t CurlWrapper::receiveData(void* buffer, std::size_t size, std::size_t nbemb, void* userp)
{
	std::string* destination = reinterpret_cast<std::string*>(userp);
	std::size_t realsize = size * nbemb;
	destination->append(reinterpret_cast<char*>(buffer), realsize);
	return realsize;
}

}
