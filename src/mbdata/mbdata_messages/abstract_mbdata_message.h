/**
 * @file abstractmbdatamessage.h
 * @brief Definition of the AbstractMBDataMessage class
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

#ifndef ABSTRACT_MBDATA_MESSAGE_H
#define ABSTRACT_MBDATA_MESSAGE_H

#include <cstdint>
#include <array>
#include <chrono>
#include <memory>
#include <optional>

#include <boost/asio.hpp>
#include <date.h>
#include <cassandra.h>
#include <observation.h>

#include "../../time_offseter.h"

namespace meteodata
{

namespace asio = boost::asio;
namespace chrono = std::chrono;

class MBDataMessageFactory;

/**
 * @brief A Message able to receive and store one raw data point from a
 * MBData text file
 */
class AbstractMBDataMessage
{
public:
	static constexpr char RAINFALL_SINCE_MIDNIGHT[] = "rainfall_midnight";

	/**
	 * @brief AbstractMBDataMessage::ptr is the type to use to manipulate a
	 * generic MBData message
	 */
	typedef std::unique_ptr<AbstractMBDataMessage> ptr;

	/**
	 * @brief Instantiate a new MBdata message parser.
	 *
	 * @tparam T The actual MBdata message type
	 * @param datetime The datetime of the archive
	 * @param content The raw content of the message
	 * @param rainfall A previous rainfall to compute the difference
	 * @param timeOffseter A object able to perform time conversions from and
	 * since the timezone used in the message
	 *
	 * @return An auto-managed shared pointer to the message
	 */
	template<typename T>
	static typename std::enable_if<std::is_base_of<AbstractMBDataMessage, T>::value, AbstractMBDataMessage::ptr>::type
	create(date::sys_seconds datetime, const std::string& content, std::optional<float> rainfall,
		   const TimeOffseter& timeOffseter)
	{
		return AbstractMBDataMessage::ptr(new T(datetime, std::ref(content), rainfall, std::ref(timeOffseter)));
	}

	/**
	 * @brief Instantiate a new MBdata message parser.
	 *
	 * @tparam T The actual MBdata message type
	 * @param datetime The datetime of the archive
	 * @param content The raw content of the message
	 * @param timeOffseter A object able to perform time conversions from and
	 * since the timezone used in the message
	 *
	 * @return An auto-managed shared pointer to the message
	 */
	template<typename T>
	static typename std::enable_if<std::is_base_of<AbstractMBDataMessage, T>::value, AbstractMBDataMessage::ptr>::type
	create(date::sys_seconds datetime, const std::string& content, const TimeOffseter& timeOffseter)
	{
		return AbstractMBDataMessage::ptr(new T(datetime, std::ref(content), std::ref(timeOffseter)));
	}

	inline operator bool() const
	{
		return _valid;
	}

	inline date::sys_seconds getDateTime() const
	{
		return _datetime;
	}

	Observation getObservation(const CassUuid station) const;

	virtual std::optional<float> getRainfallSince0h() const
	{
		return {};
	}

protected:
	AbstractMBDataMessage(date::sys_seconds datetime, const std::string& content, const TimeOffseter& timeOffseter);

	date::sys_seconds _datetime;
	std::string _content;
	bool _valid;
	const TimeOffseter& _timeOffseter;
	static constexpr int POLLING_PERIOD = 10;

	std::optional<float> _airTemp;
	std::optional<float> _dewPoint;
	std::optional<int> _humidity;
	std::optional<int> _windDir;
	std::optional<float> _wind;
	std::optional<float> _pressure;
	std::optional<float> _gust;
	std::optional<float> _rainRate;
	std::optional<int> _solarRad;
	std::optional<float> _computedRainfall;

	friend MBDataMessageFactory;
};

}

#endif /* ABSTRACT_MBDATA_MESSAGE_H */
