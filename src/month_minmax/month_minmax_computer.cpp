/**
 * @file minmax_computer.cpp
 * @brief Definition of the MonthMinmaxComputer class
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

#include <cassobs/dbconnection_minmax.h>
#include <cassobs/dbconnection_month_minmax.h>
#include <cassobs/dbconnection_normals.h>
#include <date.h>

#include "month_minmax_computer.h"
#include "cassandra_utils.h"
#include "date_utils.h"

namespace meteodata
{


MonthMinmaxComputer::MonthMinmaxComputer(DbConnectionMonthMinmax& dbMonthMinmax, DbConnectionNormals& dbNormals) :
		_dbMonthMinmax{dbMonthMinmax},
		_dbNormals{dbNormals}
{
}

void MonthMinmaxComputer::compareMinmaxWithNormals(DbConnectionMonthMinmax::Values& values,
											  const DbConnectionNormals::Values& normals)
{
	if (values.outsideTemp_avg.first && normals.tm.first)
		values.diff_outsideTemp_avg = {true, values.outsideTemp_avg.second - normals.tm.second};
	else
		values.diff_outsideTemp_avg = {false, .0f};

	if (values.outsideTemp_min_min.first && normals.tn.first)
		values.diff_outsideTemp_min_min = {true, values.outsideTemp_min_min.second - normals.tn.second};
	else
		values.diff_outsideTemp_min_min = {false, .0f};

	if (values.outsideTemp_max_max.first && normals.tx.first)
		values.diff_outsideTemp_max_max = {true, values.outsideTemp_max_max.second - normals.tx.second};
	else
		values.diff_outsideTemp_max_max = {false, .0f};

	if (values.rainfall.first && normals.rainfall.first)
		values.diff_rainfall = {true, values.rainfall.second - normals.rainfall.second};
	else
		values.diff_rainfall = {false, .0f};

	if (values.insolationTime.first && normals.insolationTime.first)
		values.diff_insolationTime = {true, values.insolationTime.second - normals.insolationTime.second};
	else
		values.diff_insolationTime = {false, .0f};
}


bool MonthMinmaxComputer::computeMonthMinmax(const CassUuid& station, const date::year_month& begin, const date::year_month& end)
{
	using namespace date;

	DbConnectionMonthMinmax::Values values;
	DbConnectionNormals::Values normals;

	year_month_day today{date::floor<date::days>(chrono::system_clock::now())};

	auto stationsWithNormals = _dbNormals.getStationsWithNormalsNearby(station);

	bool ret = true;
	for (year_month selectedDate = begin ; selectedDate <= end ; selectedDate += date::months{1}) {
		int y = int(selectedDate.year());
		int m = static_cast<int>(unsigned(selectedDate.month()));

		auto day = sys_days{year_month_day{year{y} / m / 1}};
		auto e = sys_days{year_month_day{year{y} / m / last}};
		int count = 0;
		std::vector<std::pair<int, float>> winds;
		std::array<int, 16> dirs = {0};

		if (!_dbMonthMinmax.getDailyValues(station, y, m, values))
			goto skipped;

		while (day <= e && day <= today) {
			if (!_dbMonthMinmax.getWindValues(station, day, winds))
				goto skipped;
			day += days{1};
		}

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

		if (!stationsWithNormals.empty()) {
			_dbNormals.getMonthNormals(stationsWithNormals[0].id, normals, selectedDate.month());
			compareMinmaxWithNormals(values, normals);
		}

		ret = ret && _dbMonthMinmax.insertDataPoint(station, y, m, values);

		continue;

		skipped:
		std::cerr << SD_WARNING << "Computation of month minmax failed for station " << station << " at date " << selectedDate
				  << "; check for missing data." << std::endl;
		ret = false;
	}

	return ret;
}

bool MonthMinmaxComputer::computeMonthMinmax(const CassUuid& station, const date::sys_seconds& begin,
										const date::sys_seconds& end)
{
	date::year_month ymb = to_year_month(begin);
	date::year_month yme = to_year_month(end);
	return computeMonthMinmax(station, ymb, yme);
}

} // meteodata