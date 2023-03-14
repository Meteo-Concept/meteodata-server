/**
 * @file mbdatawswinmessage.h
 * @brief Definition of the MBDataWsWinMessage class
 * @author Laurent Georget
 * @date 2019-02-21
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

#ifndef MBDATA_WSWIN_MESSAGE
#define MBDATA_WSWIN_MESSAGE

#include <cstdint>
#include <array>
#include <chrono>
#include <optional>

#include <boost/asio.hpp>
#include <observation.h>
#include <date.h>

#include "../../time_offseter.h"
#include "abstract_mbdata_message.h"

namespace meteodata
{

namespace asio = boost::asio;
namespace chrono = std::chrono;

/**
 * @brief A Message able to receive and store one raw data point from a
 * WsWin-formatted MBData text file
 */
class MBDataWsWinMessage : public AbstractMBDataMessage
{
public:
	MBDataWsWinMessage(date::sys_seconds datetime, const std::string& content,
		const TimeOffseter& timeOffseter);
};

}

#endif /* MBDATA_WSWIN_MESSAGE */

