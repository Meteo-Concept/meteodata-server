/**
 * @file abstractmbdatadecodingstrategy.h
 * @brief Definition of the MBDataMessageFactory class
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

#include <memory>
#include <stdexcept>

#include "../timeoffseter.h"
#include "abstractmbdatamessage.h"
#include "mbdataweatherlinkmessage.h"
#include "mbdatameteohubmessage.h"
#include "mbdataweathercatmessage.h"
#include "mbdatawswinmessage.h"
#include "mbdataweatherdisplaymessage.h"

namespace meteodata {

class MBDataMessageFactory
{
public:
	static inline AbstractMBDataMessage::ptr chose(const std::string& type, std::istream& entry, const TimeOffseter& timeOffseter)
	{
		std::experimental::optional<float> rainfall = 0.f; // XXX get the appropriate rainfall here, depending on the message type
		if (type == "weatherlink")
			return AbstractMBDataMessage::create<MBDataWeatherlinkMessage>(entry, rainfall, timeOffseter);
		else if (type == "meteohub")
			return AbstractMBDataMessage::create<MBDataMeteohubMessage>(entry, rainfall, timeOffseter);
		else if (type == "weathercat")
			return AbstractMBDataMessage::create<MBDataWeathercatMessage>(entry, rainfall, timeOffseter);
		else if (type == "wswin")
			return AbstractMBDataMessage::create<MBDataWsWinMessage>(entry, rainfall, timeOffseter);
		else if (type == "weatherdisplay")
			return AbstractMBDataMessage::create<MBDataWeatherDisplayMessage>(entry, rainfall, timeOffseter);
		else if (type == "cumulus")
			return AbstractMBDataMessage::create<MBDataWeatherDisplayMessage>(entry, rainfall, timeOffseter);
		else if (type == "weewx")
			return AbstractMBDataMessage::create<MBDataWeatherDisplayMessage>(entry, rainfall, timeOffseter);
		else
			throw new std::invalid_argument("Unknown message type");
	}
};

}
