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
#include <iomanip>
#include <algorithm>
#include <memory>
#include <stdexcept>

#include <openssl/hmac.h>
#include <openssl/evp.h>

using std::size_t;

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

