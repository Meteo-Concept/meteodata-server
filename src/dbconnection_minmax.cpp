/**
 * @file dbconnection_minmax.cpp
 * @brief Implementation of the DbConnectionMinmax class
 * @author Laurent Georget
 * @date 2017-10-25
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

#include <iostream>
#include <mutex>
#include <exception>
#include <chrono>
#include <unordered_map>

#include <cassandra.h>
#include <syslog.h>
#include <unistd.h>

#include "dbconnection_minmax.h"

namespace meteodata {

constexpr char DbConnectionMinmax::INSERT_DATAPOINT_STMT[];
constexpr char DbConnectionMinmax::SELECT_VALUES_0H_TO_0H_STMT[];
constexpr char DbConnectionMinmax::SELECT_VALUES_6H_TO_6H_STMT[];
constexpr char DbConnectionMinmax::SELECT_VALUES_18H_TO_18H_STMT[];
constexpr char DbConnectionMinmax::SELECT_YEARLY_VALUES_STMT[];
constexpr char DbConnectionMinmax::SELECT_YEARLY_VALUES_NOW_STMT[];
constexpr char DbConnectionMinmax::SELECT_ALL_STATIONS_STMT[];

namespace chrono = std::chrono;

DbConnectionMinmax::DbConnectionMinmax(const std::string& user, const std::string& password) :
	_cluster{cass_cluster_new()},
	_session{cass_session_new()},
	_selectAllStations{nullptr, cass_prepared_free},
	_selectValues6hTo6h{nullptr, cass_prepared_free},
	_selectValues0hTo0h{nullptr, cass_prepared_free},
	_selectValues18hTo18h{nullptr, cass_prepared_free},
	_selectYearlyValues{nullptr, cass_prepared_free},
	_selectYearlyValuesNow{nullptr, cass_prepared_free},
	_insertDataPoint{nullptr, cass_prepared_free}
{
	cass_cluster_set_contact_points(_cluster, "127.0.0.1");
	if (!user.empty() && !password.empty())
		cass_cluster_set_credentials_n(_cluster, user.c_str(), user.length(), password.c_str(), password.length());
	_futureConn = cass_session_connect(_session, _cluster);
	CassError rc = cass_future_error_code(_futureConn);
	if (rc != CASS_OK) {
		std::string desc("Impossible to connect to database: ");
		desc.append(cass_error_desc(rc));
		throw std::runtime_error(desc);
	} else {
		prepareStatements();
	}
}

void DbConnectionMinmax::prepareStatements()
{
	CassFuture* prepareFuture = cass_session_prepare(_session, "SELECT id FROM meteodata.stations");
	CassError rc = cass_future_error_code(prepareFuture);
	if (rc != CASS_OK) {
		std::string desc("Could not prepare statement selectAllStations: ");
		desc.append(cass_error_desc(rc));
		throw std::runtime_error(desc);
	}
	_selectAllStations.reset(cass_future_get_prepared(prepareFuture));
	cass_future_free(prepareFuture);

	prepareFuture = cass_session_prepare(_session, SELECT_VALUES_6H_TO_6H_STMT);
	rc = cass_future_error_code(prepareFuture);
	if (rc != CASS_OK) {
		std::string desc("Could not prepare statement _selectValues6hTo6h: ");
		desc.append(cass_error_desc(rc));
		throw std::runtime_error(desc);
	}
	_selectValues6hTo6h.reset(cass_future_get_prepared(prepareFuture));
	cass_future_free(prepareFuture);

	prepareFuture = cass_session_prepare(_session, SELECT_VALUES_18H_TO_18H_STMT);
	rc = cass_future_error_code(prepareFuture);
	if (rc != CASS_OK) {
		std::string desc("Could not prepare statement _selectValues18hTo18h: ");
		desc.append(cass_error_desc(rc));
		throw std::runtime_error(desc);
	}
	_selectValues18hTo18h.reset(cass_future_get_prepared(prepareFuture));
	cass_future_free(prepareFuture);

	prepareFuture = cass_session_prepare(_session, SELECT_VALUES_0H_TO_0H_STMT);
	rc = cass_future_error_code(prepareFuture);
	if (rc != CASS_OK) {
		std::string desc("Could not prepare statement _selectValues0hTo0h: ");
		desc.append(cass_error_desc(rc));
		throw std::runtime_error(desc);
	}
	_selectValues0hTo0h.reset(cass_future_get_prepared(prepareFuture));
	cass_future_free(prepareFuture);

	prepareFuture = cass_session_prepare(_session, SELECT_YEARLY_VALUES_STMT);
	rc = cass_future_error_code(prepareFuture);
	if (rc != CASS_OK) {
		std::string desc("Could not prepare statement _selectYearlyValues: ");
		desc.append(cass_error_desc(rc));
		throw std::runtime_error(desc);
	}
	_selectYearlyValues.reset(cass_future_get_prepared(prepareFuture));
	cass_future_free(prepareFuture);

	prepareFuture = cass_session_prepare(_session, SELECT_YEARLY_VALUES_NOW_STMT);
	rc = cass_future_error_code(prepareFuture);
	if (rc != CASS_OK) {
		std::string desc("Could not prepare statement _selectYearlyValuesNow: ");
		desc.append(cass_error_desc(rc));
		throw std::runtime_error(desc);
	}
	_selectYearlyValuesNow.reset(cass_future_get_prepared(prepareFuture));
	cass_future_free(prepareFuture);

	prepareFuture = cass_session_prepare(_session, INSERT_DATAPOINT_STMT);
	rc = cass_future_error_code(prepareFuture);
	if (rc != CASS_OK) {
		std::string desc("Could not prepare statement insertdataPoint: ");
		desc.append(cass_error_desc(rc));
		throw std::runtime_error(desc);
	}
	_insertDataPoint.reset(cass_future_get_prepared(prepareFuture));
	cass_future_free(prepareFuture);
}

bool DbConnectionMinmax::getAllStations(std::vector<CassUuid>& stations)
{
	CassFuture* query;
	CassStatement* statement = cass_prepared_bind(_selectAllStations.get());
	std::cerr << "Statement prepared" << std::endl;
	query = cass_session_execute(_session, statement);
	std::cerr << "Executed statement" << std::endl;
	cass_statement_free(statement);

	const CassResult* result = cass_future_get_result(query);
	bool ret = false;
	if (result) {
		CassIterator* iterator = cass_iterator_from_result(result);
		CassUuid uuid;
		while (cass_iterator_next(iterator)) {
			const CassRow* row = cass_iterator_get_row(iterator);
			cass_value_get_uuid(cass_row_get_column(row,0), &uuid);
			stations.push_back(uuid);
		}
		ret = true;
		cass_iterator_free(iterator);
	}
	cass_result_free(result);
	cass_future_free(query);

	return ret;
}

bool DbConnectionMinmax::getValues6hTo6h(const CassUuid& uuid, const date::sys_days& date, DbConnectionMinmax::Values& values)
{
	CassFuture* query;
	CassStatement* statement = cass_prepared_bind(_selectValues6hTo6h.get());
	std::cerr << "Statement prepared" << std::endl;
	cass_statement_bind_uuid(statement, 0, uuid);
//	cass_statement_bind_int32(statement, 1, date.time_since_epoch().count());
//	cass_statement_bind_int32(statement, 2, (date + date::days(1)).time_since_epoch().count());
	date::sys_time<chrono::milliseconds> morning = date + chrono::hours(6);
	date::sys_time<chrono::milliseconds> nextMorning = date + date::days(1) + chrono::hours(6);
	cass_statement_bind_int64(statement, 1, morning.time_since_epoch().count());
	cass_statement_bind_int64(statement, 2, nextMorning.time_since_epoch().count());
	query = cass_session_execute(_session, statement);
	std::cerr << "Executed statement" << std::endl;
	cass_statement_free(statement);

	const CassResult* result = cass_future_get_result(query);
	bool ret = false;
	if (result) {
		const CassRow* row = cass_result_first_row(result);
		if (row) {
			int param = 0;
			cass_value_get_float(cass_row_get_column(row, param++),  &values.insideTemp_max);
			for (int i=0 ; i<4 ; i++)
				cass_value_get_float(cass_row_get_column(row, param++),  &values.leafTemp_max[i]);
			cass_value_get_float(cass_row_get_column(row, param++), &values.outsideTemp_max);
			for (int i=0 ; i<4 ; i++)
				cass_value_get_float(cass_row_get_column(row, param++), &values.soilTemp_max[i]);
			for (int i=0 ; i<4 ; i++)
				cass_value_get_float(cass_row_get_column(row, param++), &values.extraTemp_max[i]);
			ret = true;
		}
	}
	cass_result_free(result);
	cass_future_free(query);

	return ret;
}

bool DbConnectionMinmax::getValues18hTo18h(const CassUuid& uuid, const date::sys_days& date, DbConnectionMinmax::Values& values)
{
	CassFuture* query;
	CassStatement* statement = cass_prepared_bind(_selectValues18hTo18h.get());
	std::cerr << "Statement prepared" << std::endl;
	cass_statement_bind_uuid(statement, 0, uuid);
//	cass_statement_bind_int32(statement, 1, date.time_since_epoch().count());
//	cass_statement_bind_int32(statement, 2, (date + date::days(1)).time_since_epoch().count());
	date::sys_time<chrono::milliseconds> previousEvening = date - date::days(1) + chrono::hours(18);
	date::sys_time<chrono::milliseconds> evening         = date + chrono::hours(18);
	cass_statement_bind_int64(statement, 1, previousEvening.time_since_epoch().count());
	cass_statement_bind_int64(statement, 2, evening.time_since_epoch().count());
	query = cass_session_execute(_session, statement);
	std::cerr << "Executed statement" << std::endl;
	cass_statement_free(statement);

	const CassResult* result = cass_future_get_result(query);
	bool ret = false;
	if (result) {
		const CassRow* row = cass_result_first_row(result);
		if (row) {
			int param = 0;
			cass_value_get_float(cass_row_get_column(row, param++),  &values.insideTemp_min);
			for (int i=0 ; i<4 ; i++)
				cass_value_get_float(cass_row_get_column(row, param++),  &values.leafTemp_min[i]);
			cass_value_get_float(cass_row_get_column(row, param++), &values.outsideTemp_min);
			for (int i=0 ; i<4 ; i++)
				cass_value_get_float(cass_row_get_column(row, param++), &values.soilTemp_min[i]);
			for (int i=0 ; i<4 ; i++)
				cass_value_get_float(cass_row_get_column(row, param++), &values.extraTemp_min[i]);
			ret = true;
		}
	}
	cass_result_free(result);
	cass_future_free(query);

	return ret;
}

bool DbConnectionMinmax::getValues0hTo0h(const CassUuid& uuid, const date::sys_days& date, DbConnectionMinmax::Values& values)
{
	CassFuture* query;
	CassStatement* statement = cass_prepared_bind(_selectValues0hTo0h.get());
	std::cerr << "Statement prepared" << std::endl;
	cass_statement_bind_uuid(statement, 0, uuid);
	cass_statement_bind_int64(statement, 1, date::sys_time<chrono::milliseconds>(date).time_since_epoch().count());
	cass_statement_bind_int64(statement, 2, date::sys_time<chrono::milliseconds>(date + date::days(1)).time_since_epoch().count());
//	cass_statement_bind_int32(statement, 1, date.time_since_epoch().count());
//	cass_statement_bind_int32(statement, 2, (date + date::days(1)).time_since_epoch().count());
//	date::sys_time<chrono::milliseconds> previousEvening = date - date::days(1) + chrono::hours(18);
//	date::sys_time<chrono::milliseconds> evening         = date + chrono::hours(18);
//	cass_statement_bind_int64(statement, 3, previousEvening.time_since_epoch().count());
//	cass_statement_bind_int64(statement, 4, evening.time_since_epoch().count());
	query = cass_session_execute(_session, statement);
	std::cerr << "Executed statement" << std::endl;
	cass_statement_free(statement);

	const CassResult* result = cass_future_get_result(query);
	bool ret = false;
	if (result) {
		const CassRow* row = cass_result_first_row(result);
		if (row) {
			int res = 0;
			cass_value_get_float(cass_row_get_column(row, res++), &values.barometer_min);
			cass_value_get_float(cass_row_get_column(row, res++), &values.barometer_max);
			cass_value_get_float(cass_row_get_column(row, res++), &values.barometer_avg);
			for (int i=0 ; i<4 ; i++) {
				cass_value_get_int32(cass_row_get_column(row, res++), &values.leafWetnesses_min[i]);
				cass_value_get_int32(cass_row_get_column(row, res++), &values.leafWetnesses_max[i]);
				cass_value_get_int32(cass_row_get_column(row, res++), &values.leafWetnesses_avg[i]);
			}
			for (int i=0 ; i<4 ; i++) {
				cass_value_get_int32(cass_row_get_column(row, res++), &values.soilMoistures_min[i]);
				cass_value_get_int32(cass_row_get_column(row, res++), &values.soilMoistures_max[i]);
				cass_value_get_int32(cass_row_get_column(row, res++), &values.soilMoistures_avg[i]);
			}
			cass_value_get_int32(cass_row_get_column(row, res++), &values.insideHum_min);
			cass_value_get_int32(cass_row_get_column(row, res++), &values.insideHum_max);
			cass_value_get_int32(cass_row_get_column(row, res++), &values.insideHum_avg);
			cass_value_get_int32(cass_row_get_column(row, res++), &values.outsideHum_min);
			cass_value_get_int32(cass_row_get_column(row, res++), &values.outsideHum_max);
			cass_value_get_int32(cass_row_get_column(row, res++), &values.outsideHum_avg);
			for (int i=0 ; i<7 ; i++) {
				cass_value_get_int32(cass_row_get_column(row, res++), &values.extraHum_min[i]);
				cass_value_get_int32(cass_row_get_column(row, res++), &values.extraHum_max[i]);
				cass_value_get_int32(cass_row_get_column(row, res++), &values.extraHum_avg[i]);
			}
			cass_value_get_int32(cass_row_get_column(row, res++), &values.solarRad_max);
			cass_value_get_int32(cass_row_get_column(row, res++), &values.solarRad_avg);
			cass_value_get_int32(cass_row_get_column(row, res++), &values.uv_max);
			cass_value_get_int32(cass_row_get_column(row, res++), &values.uv_avg);
			cass_value_get_int32(cass_row_get_column(row, res++), &values.winddir);
			cass_value_get_float(cass_row_get_column(row, res++), &values.windgust_max);
			cass_value_get_float(cass_row_get_column(row, res++), &values.windgust_avg);
			cass_value_get_float(cass_row_get_column(row, res++), &values.windspeed_max);
			cass_value_get_float(cass_row_get_column(row, res++), &values.windspeed_avg);
			cass_value_get_float(cass_row_get_column(row, res++), &values.rainrate_max);
			cass_value_get_float(cass_row_get_column(row, res++), &values.dewpoint_min);
			cass_value_get_float(cass_row_get_column(row, res++), &values.dewpoint_max);
			cass_value_get_float(cass_row_get_column(row, res++), &values.dewpoint_avg);
			ret = true;
		}
	}
	cass_result_free(result);
	cass_future_free(query);

	return ret;
}

bool DbConnectionMinmax::getYearlyValues(const CassUuid& uuid, const date::sys_days& date, float& rain, float& et)
{
	CassFuture* query;
	CassStatement* statement = cass_prepared_bind(_selectYearlyValues.get());
	std::cerr << "Statement prepared" << std::endl;
	cass_statement_bind_uuid(statement, 0, uuid);
//	cass_statement_bind_int32(statement, 1, date.time_since_epoch().count());
	cass_statement_bind_int64(statement, 1, (date::sys_time<chrono::milliseconds>(date) + chrono::hours(6)).time_since_epoch().count());
	cass_statement_bind_int64(statement, 2, (date::sys_time<chrono::milliseconds>(date) + chrono::hours(6) + chrono::minutes(5)).time_since_epoch().count());
	query = cass_session_execute(_session, statement);
	std::cerr << "Executed statement" << std::endl;
	cass_statement_free(statement);

	const CassResult* result = cass_future_get_result(query);
	bool ret = false;
	if (result) {
		const CassRow* row = cass_result_first_row(result);
		if (row) {
			cass_value_get_float(cass_row_get_column(row, 0), &rain);
			cass_value_get_float(cass_row_get_column(row, 1), &et);
			ret = true;
		}
	}
	cass_result_free(result);
	cass_future_free(query);

	return ret;
}

bool DbConnectionMinmax::getYearlyValuesNow(const CassUuid& uuid, float& rain, float& et)
{
	CassFuture* query;
	CassStatement* statement = cass_prepared_bind(_selectYearlyValuesNow.get());
	std::cerr << "Statement prepared" << std::endl;
	cass_statement_bind_uuid(statement, 0, uuid);
	query = cass_session_execute(_session, statement);
	std::cerr << "Executed statement" << std::endl;
	cass_statement_free(statement);

	const CassResult* result = cass_future_get_result(query);
	bool ret = false;
	if (result) {
		const CassRow* row = cass_result_first_row(result);
		if (row) {
			cass_value_get_float(cass_row_get_column(row, 0), &rain);
			cass_value_get_float(cass_row_get_column(row, 1), &et);
			ret = true;
		}
	}
	cass_result_free(result);
	cass_future_free(query);

	return ret;
}


bool DbConnectionMinmax::insertDataPoint(const CassUuid& station, const date::sys_days& date, const Values& values)
{
	CassFuture* query;
	std::cerr << "About to insert data point in database" << std::endl;
	CassStatement* statement = cass_prepared_bind(_insertDataPoint.get());
	int param = 0;
	cass_statement_bind_uuid(statement,  param++, station);
	cass_statement_bind_uint32(statement, param++, cass_date_from_epoch(date::sys_seconds(date).time_since_epoch().count()));
	cass_statement_bind_float(statement, param++, values.barometer_min);
	cass_statement_bind_float(statement, param++, values.barometer_max);
	cass_statement_bind_float(statement, param++, values.barometer_avg);
	cass_statement_bind_float(statement, param++, values.dayEt);
	cass_statement_bind_float(statement, param++, values.monthEt);
	cass_statement_bind_float(statement, param++, values.yearEt);
	cass_statement_bind_float(statement, param++, values.dayRain);
	cass_statement_bind_float(statement, param++, values.monthRain);
	cass_statement_bind_float(statement, param++, values.yearRain);
	cass_statement_bind_float(statement, param++, values.dewpoint_min);
	cass_statement_bind_float(statement, param++, values.dewpoint_max);
	cass_statement_bind_float(statement, param++, values.dewpoint_avg);
	cass_statement_bind_int32(statement, param++, values.insideHum_min);
	cass_statement_bind_int32(statement, param++, values.insideHum_max);
	cass_statement_bind_int32(statement, param++, values.insideHum_avg);
	cass_statement_bind_float(statement, param++, values.insideTemp_min);
	cass_statement_bind_float(statement, param++, values.insideTemp_max);
	cass_statement_bind_float(statement, param++, values.insideTemp_avg);
	for (int i=0 ; i<4 ; i++) {
		cass_statement_bind_float(statement, param++, values.leafTemp_min[i]);
		cass_statement_bind_float(statement, param++, values.leafTemp_max[i]);
		cass_statement_bind_float(statement, param++, values.leafTemp_avg[i]);
	}
	for (int i=0 ; i<4 ; i++) {
		cass_statement_bind_int32(statement, param++, values.leafWetnesses_min[i]);
		cass_statement_bind_int32(statement, param++, values.leafWetnesses_max[i]);
		cass_statement_bind_int32(statement, param++, values.leafWetnesses_avg[i]);
	}
	cass_statement_bind_int32(statement, param++, values.outsideHum_min);
	cass_statement_bind_int32(statement, param++, values.outsideHum_max);
	cass_statement_bind_int32(statement, param++, values.outsideHum_avg);
	cass_statement_bind_float(statement, param++, values.outsideTemp_min);
	cass_statement_bind_float(statement, param++, values.outsideTemp_max);
	cass_statement_bind_float(statement, param++, values.outsideTemp_avg);
	cass_statement_bind_float(statement, param++, values.rainrate_max);
	for (int i=0 ; i<4 ; i++) {
		cass_statement_bind_int32(statement, param++, values.soilMoistures_min[i]);
		cass_statement_bind_int32(statement, param++, values.soilMoistures_max[i]);
		cass_statement_bind_int32(statement, param++, values.soilMoistures_avg[i]);
	}
	for (int i=0 ; i<4 ; i++) {
		cass_statement_bind_float(statement, param++, values.soilTemp_min[i]);
		cass_statement_bind_float(statement, param++, values.soilTemp_max[i]);
		cass_statement_bind_float(statement, param++, values.soilTemp_avg[i]);
	}
	for (int i=0 ; i<7 ; i++) {
		cass_statement_bind_float(statement, param++, values.extraTemp_min[i]);
		cass_statement_bind_float(statement, param++, values.extraTemp_max[i]);
		cass_statement_bind_float(statement, param++, values.extraTemp_avg[i]);
	}
	for (int i=0 ; i<7 ; i++) {
		cass_statement_bind_int32(statement, param++, values.extraHum_min[i]);
		cass_statement_bind_int32(statement, param++, values.extraHum_max[i]);
		cass_statement_bind_int32(statement, param++, values.extraHum_avg[i]);
	}
	cass_statement_bind_int32(statement, param++, values.solarRad_max);
	cass_statement_bind_int32(statement, param++, values.solarRad_avg);
	cass_statement_bind_int32(statement, param++, values.uv_max);
	cass_statement_bind_int32(statement, param++, values.uv_avg);
	cass_statement_bind_int32(statement, param++, values.winddir);
	cass_statement_bind_float(statement, param++, values.windgust_max);
	cass_statement_bind_float(statement, param++, values.windgust_avg);
	cass_statement_bind_float(statement, param++, values.windspeed_max);
	cass_statement_bind_float(statement, param++, values.windspeed_avg);
	query = cass_session_execute(_session, statement);
	cass_statement_free(statement);

	const CassResult* result = cass_future_get_result(query);
	bool ret = true;
	if (!result) {
		const char* error_message;
		size_t error_message_length;
		cass_future_error_message(query, &error_message, &error_message_length);
		std::cerr << "Error from Cassandra: " << error_message << std::endl;
		ret = false;
	}
	cass_result_free(result);
	cass_future_free(query);

	return ret;
}

DbConnectionMinmax::~DbConnectionMinmax()
{
	CassFuture* futureClose = cass_session_close(_session);
	cass_future_wait(futureClose);
	cass_future_free(futureClose);
	cass_future_free(_futureConn);
	cass_cluster_free(_cluster);
	cass_session_free(_session);
}
}
