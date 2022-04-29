/**
 * @file liveobjects_message.cpp
 * @brief Implementation of the LiveobjectsMessage class
 * @author Laurent Georget
 * @date 2022-04-29
 */
/*
 * Copyright (C) 2022  SAS JD Environnement <contact@meteo-concept.fr>
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

#include <algorithm>
#include <iostream>
#include <string>
#include <systemd/sd-daemon.h>

#include "liveobjects_message.h"

namespace meteodata
{

bool LiveobjectsMessage::validateInput(const std::string& payload, int expectedSize)
{
	if (payload.length() != expectedSize) {
		std::cerr << SD_ERR << "[MQTT Liveobjects] protocol: " << "Invalid size " << payload.length() << " for payload "
				  << payload << ", should be " << expectedSize << std::endl;
		return false;
	}

	if (!std::all_of(payload.cbegin(), payload.cend(), [](char c) {
		return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
	})) {
		std::cerr << SD_ERR << "[MQTT Liveobjects] protocol: " << "Payload " << payload
				  << " contains invalid characters" << std::endl;
		return false;
	}

	return true;
}

}