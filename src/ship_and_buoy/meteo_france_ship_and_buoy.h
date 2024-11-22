/**
 * @file meteofranceshipandbuoy.h
 * @brief Definition of the MeteoFranceShipAndBuoy class
 * @author Laurent Georget
 * @date 2019-01-16
 */
/*
 * Copyright (C) 2019  JD Environnement <contact@meteo-concept.fr>
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

#ifndef METEOFRANCESHIPANDBUOY_H
#define METEOFRANCESHIPANDBUOY_H

#include <cstdint>
#include <array>
#include <chrono>
#include <optional>

#include <cassandra.h>
#include <boost/asio.hpp>
#include <cassobs/observation.h>
#include <date/date.h>

namespace meteodata
{

namespace asio = boost::asio;
namespace chrono = std::chrono;

/**
 * @brief A Message able to receive and store one raw data point from the
 * CSV downloadable from https://donneespubliques.meteofrance.fr/donnees_libres/Txt/Marine/...
 */
class MeteoFranceShipAndBuoy
{
public:
	MeteoFranceShipAndBuoy(std::istream& entry, const std::vector<std::string>& fields);
	Observation getObservation(const CassUuid station) const;

	inline const std::string& getIdentifier()
	{
		return _identifier;
	}

	inline operator bool()
	{
		return _valid;
	}

private:
	std::string _identifier; //numer_sta
	date::sys_seconds _datetime; //time, yyyymmddHHMMss
	float _latitude; //lat
	float _longitude; //lon
	std::optional<float> _airTemp; //t, K
	std::optional<float> _dewPoint; //td, K
	std::optional<int> _humidity; //u, %
	std::optional<int> _windDir; //dd, m/s
	std::optional<float> _wind; //ff, m/s
	std::optional<int> _pressure; //pmer, Pa
	std::optional<float> _seaTemp; //tmer, K
	std::optional<float> _seaWindHeight; //HwaHwa
	std::optional<float> _seaWindPeriod; //PwaPwa
	std::optional<float> _seaWindDirection; //dwadwa
	std::optional<float> _swellHeight1; //Hw1Hw1
	std::optional<float> _swellPeriod1; //Pw1Pw1
	std::optional<float> _swellDirection1; //dw1dw1
	std::optional<float> _swellHeight2; //Hw2Hw2
	std::optional<float> _swellPeriod2; //Pw2Pw2
	std::optional<float> _swellDirection2; //dw2dw2
	std::optional<float> _gust; //rafper, m/s
	bool _valid;
};

}

#endif /* METEOFRANCESHIPANDBUOY_H */
