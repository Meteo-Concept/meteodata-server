/**
 * @file mf_radome_message.h
 * @brief Definition of the MfRadomeMessage class
 * @author Laurent Georget
 * @date 2024-01-15
 */
/*
 * Copyright (C) 2024  SAS Météo Concept <contact@meteo-concept.fr>
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

#ifndef MF_RADOME_MESSAGE_H
#define MF_RADOME_MESSAGE_H

#include <boost/property_tree/ptree.hpp>
#include <cassandra.h>
#include <observation.h>
#include <date/date.h>

namespace meteodata
{

namespace pt = boost::property_tree;

/**
 * @brief A Message able to receive and store one data point from the
 * MeteoFrance hourly observation API
 */
class MfRadomeMessage
{
public:
	void parse(pt::ptree&& payload, date::sys_seconds& timestamp);
	Observation getObservation(const CassUuid& station) const;
	inline bool looksValid() const { return _valid; };
	inline std::string getMfId() const { return _mfId; };

private:
	std::string _mfId;
	bool _valid = false;
	date::sys_seconds _timestamp;
	boost::optional<float> _rr1;
	boost::optional<float> _ff;
	boost::optional<int> _dd;
	boost::optional<float> _fxy;
	boost::optional<int> _dxy;
	boost::optional<float> _fxi;
	boost::optional<int> _dxi;
	boost::optional<float> _t;
	boost::optional<float> _td;
	boost::optional<float> _tn;
	boost::optional<float> _tx;
	boost::optional<float> _u;
	boost::optional<float> _un;
	boost::optional<float> _ux;
	boost::optional<float> _pmer;
	boost::optional<float> _pres;
	boost::optional<float> _glo;
	boost::optional<int> _insolh;
};

}

#endif /* MF_RADOME_MESSAGE_H */
