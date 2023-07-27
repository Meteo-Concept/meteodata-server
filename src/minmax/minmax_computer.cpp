/**
 * @file minmax_computer.cpp
 * @brief Definition of the MinmaxComputer class
 * @author Laurent Georget
 * @date 2023-07-21
 */
/*
 * Copyright (C) 2023  SAS Météo Concept <contact@meteo-concept.fr>
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

#include <iostream>
#include <systemd/sd-daemon.h>

#include <dbconnection_minmax.h>
#include <dbconnection_month_minmax.h>
#include <dbconnection_normals.h>

#include "minmax_computer.h"
#include "cassandra_utils.h"

namespace meteodata
{

MinmaxComputer::MinmaxComputer(DbConnectionMinmax& dbMinmax) :
		_dbMinmax{dbMinmax}
{
}

bool MinmaxComputer::computeMinmax(const CassUuid& station, const date::sys_seconds& begin, const date::sys_seconds& end)
{
	using namespace date;
	DbConnectionMinmax::Values values;
	bool ret = true;

	date::sys_days selectedDate = date::floor<date::days>(begin);
	while (selectedDate <= end) {
		std::pair<bool, float> rainToday, etToday, rainYesterday, etYesterday, rainBeginMonth, etBeginMonth;
		auto ymd = date::year_month_day(selectedDate);
		date::sys_days beginningOfMonth = selectedDate - date::days(unsigned(ymd.day()));
		std::vector<std::pair<int, float>> winds;
		int count = 0;
		std::array<int, 16> dirs = {0};

		if (!_dbMinmax.getValues6hTo6h(station, selectedDate, values))
			goto skipped;
		if (!_dbMinmax.getValues18hTo18h(station, selectedDate, values))
			goto skipped;
		if (!_dbMinmax.getValues0hTo0h(station, selectedDate, values))
			goto skipped;

		if (unsigned(ymd.month()) == 1 && unsigned(ymd.day()) == 1) {
			rainToday = values.rainfall;
			etToday = values.et;
		} else {
			if (!_dbMinmax.getYearlyValues(station, selectedDate - date::days(1), rainYesterday, etYesterday))
				goto skipped;
			compute(rainToday, values.rainfall, rainYesterday, std::plus<>());
			compute(etToday, values.et, etYesterday, std::plus<>());
		}

		if (unsigned(ymd.month()) == 1) {
			values.monthRain = rainToday;
			values.monthEt = etToday;
		} else {
			if (!_dbMinmax.getYearlyValues(station, beginningOfMonth, rainBeginMonth, etBeginMonth))
				goto skipped;
			compute(values.monthRain, rainToday, rainBeginMonth, std::minus<>());
			compute(values.monthEt, etToday, etBeginMonth, std::minus<>());
		}

		values.dayRain = values.rainfall;
		values.yearRain = rainToday;
		values.dayEt = values.et;
		values.yearEt = etToday;

		computeMean(values.outsideTemp_avg, values.outsideTemp_max, values.outsideTemp_min);
		computeMean(values.insideTemp_avg, values.insideTemp_max, values.insideTemp_min);

		for (int i = 0 ; i < 2 ; i++)
			computeMean(values.leafTemp_avg[i], values.leafTemp_max[i], values.leafTemp_min[i]);
		for (int i = 0 ; i < 4 ; i++)
			computeMean(values.soilTemp_avg[i], values.soilTemp_max[i], values.soilTemp_min[i]);
		for (int i = 0 ; i < 3 ; i++)
			computeMean(values.extraTemp_avg[i], values.extraTemp_max[i], values.extraTemp_min[i]);

		if (!_dbMinmax.getWindValues(station, selectedDate, winds))
			goto skipped;

		for (auto&& w: winds) {
			if (w.second / 3.6 >= 2.0) {
				int rounded = ((w.first % 360) * 100 + 1125) / 2250;
				dirs[rounded % 16]++;
				count++;
			}
		}

		values.winddir.second.resize(16);
		for (int i = 0 ; i < 16 ; i++) {
			int v = dirs[i];
			values.winddir.second[i] = count == 0 ? 0 : v * 1000 / count;
		}
		values.winddir.first = true;

		ret = ret && _dbMinmax.insertDataPoint(station, selectedDate, values);
		selectedDate += date::days{1};

		continue;

		skipped:
		std::cerr << SD_WARNING << "Computation of minmax failed for station " << station << " at date " << selectedDate
				  << "; check for missing data." << std::endl;
		ret = false;
	}

	return ret;
}

} // meteodata