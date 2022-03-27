/**
 * @file hex_parser.h
 * @brief Definition of the hex_parser classes
 * @author Laurent Georget
 * @date 2022-03-24
 */
/*
 * Copyright (C) 2022  JD Environnement <contact@meteo-concept.fr>
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

#ifndef HEX_PARSER_H
#define HEX_PARSER_H

#include <iostream>
#include <exception>
#include <stdexcept>

namespace meteodata
{

namespace hex_parser
{

template<typename T>
struct ParserBigEndian
{
	int length = 2;
	int base = 10;

	T& destination;

	void parse(std::istream& is)
	{
		destination = 0;
		for (int i = length ; i > 0 ; i--) {
			auto c = is.get();
			if (std::isspace(c)) {
				i++; // do as if the blank character was not there at all
				continue;
			}

			if (c >= '0' && c <= '9') {
				destination = destination * base + c - '0';
			} else if (c >= 'A' && c <= 'F') {
				destination = destination * base + c - 'A' + 10;
			} else if (c >= 'a' && c <= 'f') {
				destination = destination * base + c - 'a' + 10;
			}
		}
	}
};

template<typename T>
struct ParserLittleEndian
{
	int length = 2;
	int base = 10;

	T& destination;


	ParserLittleEndian(int length, int base, T& destination) :
		length{length},
		base{base},
		destination{destination}
	{
		if (length % 2 != 0) {
			throw std::invalid_argument("Length must be even");
		}
	}

	void parse(std::istream& is)
	{
		destination = 0;
		size_t nbByte = 0;
		for (int i = length ; i > 0 ; i-=2) {
			int byte = 0;

			for (int j = 0 ; j < 2 ; j++) {
				auto c = is.get();
				if (std::isspace(c)) {
					i++; // do as if the blank character was not there at all
					continue;
				}

				if (c >= '0' && c <= '9') {
					byte = byte * base + c - '0';
				} else if (c >= 'A' && c <= 'F') {
					byte = byte * base + c - 'A' + 10;
				} else if (c >= 'a' && c <= 'f') {
					byte = byte * base + c - 'a' + 10;
				}
			}
			destination += (byte << (8 * nbByte));
			nbByte++;
		}
	}
};

template<typename T>
static ParserBigEndian<T> parse(T& destination, int length, int base)
{
	return ParserBigEndian<T>{length, base, destination};
}

template<typename T>
static ParserLittleEndian<T> parseLE(T& destination, int length, int base)
{
	return ParserLittleEndian<T>{length, base, destination};
}

struct Ignorer
{
	std::streamsize length;
};

static Ignorer ignore(std::streamsize length)
{
	return Ignorer{length};
}

}

template<typename T>
std::istream& operator>>(std::istream& is, hex_parser::ParserBigEndian<T>&& p)
{
	p.parse(is);
	return is;
}

template<typename T>
std::istream& operator>>(std::istream& is, hex_parser::ParserLittleEndian<T>&& p)
{
	p.parse(is);
	return is;
}

inline std::istream& operator>>(std::istream& is, const hex_parser::Ignorer& ignorer)
{
	std::streamsize i = ignorer.length;
	while (i > 0) {
		auto c = is.get();
		if (!std::isspace(c)) {
			i--; // do not count blank characters
		}
	}
	return is;
}

}

#endif
