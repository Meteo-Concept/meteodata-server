/**
 * @file oy1110_thermohygrometer_message.h
 * @brief Definition of the Oy1110ThermohygrometerMessage class
 * @author Laurent Georget
 * @date 2022-10-06
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

#ifndef OY1110_THERMOHYGROMETER_MESSAGE_H
#define OY1110_THERMOHYGROMETER_MESSAGE_H

#include <string>
#include <vector>
#include <chrono>
#include <iterator>
#include <cmath>

#include <boost/json.hpp>
#include <date/date.h>
#include <cassobs/observation.h>
#include <cassandra.h>

#include "liveobjects/liveobjects_message.h"

namespace meteodata
{
/**
 * @brief A Message able to receive and store a Talkpool OY1110 thermohygrometer
 * IoT payload from a low-power connection (LoRa, NB-IoT, etc.)
 */
class Oy1110ThermohygrometerMessage : public LiveobjectsMessage
{
public:
	explicit Oy1110ThermohygrometerMessage(const CassUuid& station);

	/**
	 * @brief Parse the payload to build a specific datapoint for a given
	 * timestamp (not part of the payload itself)
	 *
	 * @param data The payload received by some mean, it's a ASCII-encoded
	 * hexadecimal string
	 * @param datetime The timestamp of the data message
	 */
	void ingest(const CassUuid& station, const std::string& payload, const date::sys_seconds& datetime) override;

	inline bool looksValid() const override
	{
		return _obs.valid && _obs.temperatures.size() == _obs.humidities.size();
	}

	Observation getObservation(const CassUuid& station) const override;

	class const_iterator : public std::iterator_traits<Observation> {
	private:
		const Oy1110ThermohygrometerMessage* _msg;
		std::vector<float>::const_iterator _tempIt;
		std::vector<float>::const_iterator _humIt;
		date::sys_seconds _time;

	public:
		using iterator_category = std::forward_iterator_tag;
		explicit const_iterator(const Oy1110ThermohygrometerMessage& m) :
			_msg{&m},
			_tempIt{m._obs.temperatures.begin()},
			_humIt{m._obs.humidities.begin()},
			_time{m._obs.basetime}
		{}
		const_iterator() :
			_msg{nullptr}
		{}

		inline void operator++() {
			++_tempIt;
			++_humIt;
			_time -= _msg->_obs.offset;
		}

		inline void operator++(int) {
			_tempIt++;
			_humIt++;
			_time -= _msg->_obs.offset;
		}

		inline Observation operator*() {
			Observation o;
			o.station = _msg->_station;
			o.day = date::floor<date::days>(_time);
			o.time = _time;
			o.outsidetemp = { true, *_tempIt };
			o.outsidehum = { true, int(std::round(*_humIt)) };

			return o;
		}

		inline bool operator==(const const_iterator& it)
		{
			if (it._msg == nullptr && _msg == nullptr) {
				return true;
			} else if (it._msg == nullptr && _msg != nullptr) {
				return false;
			} else if (it._msg != nullptr && _msg == nullptr) {
				return false;
			} else if (it._msg != nullptr && _msg != nullptr && it._msg != _msg) {
				return false;
			} else {
				return it._tempIt == _tempIt && it._humIt == _humIt;
			}
		}

		inline bool operator!=(const const_iterator& it)
		{
			return !operator==(it);
		}

		friend class Oy1110ThermohygrometerMessage;
	};

	inline const_iterator begin() const { return const_iterator{*this}; }
	inline const_iterator end() const { return const_iterator{}; }

	boost::json::object getDecodedMessage() const override;

private:
	CassUuid _station;

	/**
	 * @brief A struct used to store observation values to then populate the
	 * DB insertion query
	 */
	struct DataPoint
	{
		bool valid = false;
		date::sys_seconds basetime;
		std::chrono::seconds offset;
		std::vector<float> temperatures;
		std::vector<float> humidities;
	};

	static bool validateInput(const std::string& payload);

	/**
	 * @brief An observation object to store values as the API return value
	 * is getting parsed
	 */
	DataPoint _obs;

	friend class const_iterator;
};

}

#endif /* OY1110_THERMOHYGROMETER_MESSAGE_H */
