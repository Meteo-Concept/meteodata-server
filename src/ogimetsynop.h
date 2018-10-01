/**
 * @file ogimetsynop.h
 * @brief Definition of the OgimetSynop class
 * @author Laurent Georget
 * @date 2017-10-11
 */
/*
 * Copyright (C) 2017  SAS Météo Concept <contact@meteo-concept.fr>
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

#ifndef OGIMETSYNOP_H
#define OGIMETSYNOP_H

#include <cstdint>
#include <array>
#include <chrono>
#include <experimental/optional>

#include <boost/asio.hpp>
#include <cassandra.h>
#include <message.h>

#include "synopdecoder/synop_message.h"

namespace meteodata {

namespace asio = boost::asio;
namespace chrono = std::chrono;

/**
 * @brief A Message able to receive and store one raw data point from the
 * archive of a VantagePro2 (R) station, by Davis Instruments (R)
 */
class OgimetSynop : public Message
{
public:
	/**
	 * @brief Construct a \a OgimetSynop from a decoded SYNOP message
	 *
	 * @param data A SYNOP messgae obtained from Ogimet and decoded
	 */
	OgimetSynop(const SynopMessage& data);
	virtual void populateDataPoint(const CassUuid station, CassStatement* const statement) const override;
	virtual void populateV2DataPoint(const CassUuid station, CassStatement* const statement) const override;

private:
	/**
	 * @brief The data point, an individual SYNOP message
	 */
	SynopMessage _data;

	std::experimental::optional<int> _humidity;
	std::experimental::optional<float> _rainfall;
	std::experimental::optional<float> _wind_mps;
	std::experimental::optional<float> _gust;
};

}

#endif /* OGIMETSYNOP_H */
