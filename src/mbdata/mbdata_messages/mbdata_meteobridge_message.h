/**
 * @file mbdata_meteobridge_message.h
 * @brief Definition of the MBDataMeteobridgeMessage class
 * @author Laurent Georget
 * @date 2023-12-14
 */
/*
 * Copyright (C) 2023  JD Environnement <contact@meteo-concept.fr>
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

#ifndef MBDATA_METEOBRIDGE_MESSAGE_H
#define MBDATA_METEOBRIDGE_MESSAGE_H

#include <cstdint>
#include <array>
#include <chrono>
#include <optional>
#include <map>

#include <boost/asio.hpp>
#include <cassobs/observation.h>
#include <date.h>

#include "time_offseter.h"
#include "abstract_mbdata_message.h"

namespace meteodata
{

namespace chrono = std::chrono;

/**
 * @brief A Message able to receive and store one raw data point from a
 * Meteobridge default text file
 */
class MBDataMeteobridgeMessage : public AbstractMBDataMessage
{
public:
	MBDataMeteobridgeMessage(std::istream& entry, std::optional<float> dayRainfall, const TimeOffseter& timeOffseter);
	std::optional<float> getRainfallSince0h() const;

private:
	std::optional<float> _rainfallSince0h;
};

}

#endif /* MBDATA_METEOBRIDGE_MESSAGE_H */
