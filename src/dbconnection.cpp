/**
 * @file dbconnection.cpp
 * @brief Implementation of the DbConnection class
 * @author Laurent Georget
 * @date 2016-10-05
 */
/*
 * Copyright (C) 2016  SAS Météo Concept <contact@meteo-concept.fr>
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

#include <cassandra.h>
#include <syslog.h>
#include <unistd.h>

#include "dbconnection.h"

namespace meteodata {
	DbConnection::DbConnection(const std::string& user, const std::string& password) :
		_cluster{cass_cluster_new()},
		_session{cass_session_new()},
		_selectStationByCoords{nullptr, cass_prepared_free},
		_insertDataPoint{nullptr, cass_prepared_free}
	{
		cass_log_set_level(CASS_LOG_INFO);
		cass_cluster_set_contact_points(_cluster, "127.0.0.1");
		if (!user.empty() && !password.empty())
			cass_cluster_set_credentials_n(_cluster, user.c_str(), user.length(), password.c_str(), password.length());
		_futureConn = cass_session_connect(_session, _cluster);
		CassError rc = cass_future_error_code(_futureConn);
		if (rc != CASS_OK) {
			syslog(LOG_ERR, "Impossible to connect to database");
		} else {
			prepareStatements();
		}
	}

	void DbConnection::prepareStatements()
	{
		CassFuture* prepareFuture = cass_session_prepare(_session, "SELECT station FROM meteodata.coordinates WHERE elevation = ? AND latitude = ? AND longitude = ?");
		CassError rc = cass_future_error_code(prepareFuture);
		if (rc != CASS_OK) {
			syslog(LOG_ERR, "Could not prepare statement selectStationByCoords: %s", cass_error_desc(rc));
		}
		_selectStationByCoords.reset(cass_future_get_prepared(prepareFuture));
		cass_future_free(prepareFuture);

		prepareFuture = cass_session_prepare(_session,
			"INSERT INTO meteodata.meteo ("
			"station,"
			"time,"
			"bartrend,barometer,barometer_abs,barometer_raw,"
			"insidetemp,outsidetemp,"
			"insidehum,outsidehum,"
			"extratemp1,extratemp2, extratemp3,extratemp4,"
				"extratemp5, extratemp6,extratemp7,"
			"soiltemp1, soiltemp2, soiltemp3, soiltemp4,"
			"leaftemp1, leaftemp2, leaftemp3, leaftemp4,"
			"extrahum1, extrahum2, extrahum3, extrahum4,"
				"extrahum5, extrahum6, extrahum7,"
			"soilmoistures1, soilmoistures2, soilmoistures3,"
				"soilmoistures4,"
			"leafwetnesses1, leafwetnesses2, leafwetnesses3,"
				"leafwetnesses4,"
			"windspeed, winddir,"
			"avgwindspeed_10min, avgwindspeed_2min,"
			"windgust_10min, windgustdir,"
			"rainrate, rain_15min, rain_1h, rain_24h,"
			"dayrain, monthrain, yearrain,"
			"stormrain, stormstartdate,"
			"UV, solarrad,"
			"dewpoint, heatindex, windchill, thswindex,"
			"dayET, monthET, yearET,"
			"forecast, forecast_icons,"
			"sunrise, sunset)"
			"VALUES ("
			"?,"
			"?,"
			"?,?,?,?,"
			"?,?,"
			"?,?,"
			"?,?,?,?,"
				"?,?,?,"
			"?,?,?,?,"
			"?,?,?,?,"
			"?,?,?,?,"
				"?,?,?,"
			"?,?,?,"
				"?,"
			"?,?,?,"
				"?,"
			"?,?,"
			"?,?,"
			"?,?,"
			"?,?,?,?,"
			"?,?,?,"
			"?,?,"
			"?,?,"
			"?,?,?,?,"
			"?,?,?,"
			"?,?,"
			"?,?)");

		rc = cass_future_error_code(prepareFuture);
		if (rc != CASS_OK) {
			syslog(LOG_ERR, "Could not prepare statement insertDataPoint: %s", cass_error_desc(rc));
		}
		_insertDataPoint.reset(cass_future_get_prepared(prepareFuture));
		cass_future_free(prepareFuture);
	}

	CassUuid DbConnection::getStationByCoords(int elevation, int latitude, int longitude)
	{
		CassStatement* statement = cass_prepared_bind(_selectStationByCoords.get());
		std::cerr << "Statement prepared" << std::endl;
		cass_statement_bind_int32(statement, 0, elevation);
		cass_statement_bind_int32(statement, 1, latitude);
		cass_statement_bind_int32(statement, 2, longitude);
		CassFuture* query = cass_session_execute(_session, statement);

		std::cerr << "Executed statement" << std::endl;
		cass_statement_free(statement);
		const CassResult* result = cass_future_get_result(query);
		CassUuid uuid;
		if (result) {
			std::cerr << "We have a result" << std::endl;
			const CassRow* row = cass_result_first_row(result);
			if (row) {
				cass_value_get_uuid(cass_row_get_column(row,0), &uuid);
			}
		} else {
			std::cerr << "No result" << std::endl;
			/** FIXME returning a default UUID is probably not a sane
			 * thing to do. Shouldn't we raise an exception instead? */
			cass_uuid_from_string("000000000-0000-0000-0000-000000000000", &uuid);
		}
		cass_result_free(result);
		cass_future_free(query);

		return uuid;
	}

	bool DbConnection::insertDataPoint(const CassUuid station, const Message& msg)
	{
		std::cerr << "About to insert data point in database" << std::endl;
		CassStatement* statement = cass_prepared_bind(_insertDataPoint.get());
		msg.populateDataPoint(station, statement);
		CassFuture* query = cass_session_execute(_session, statement);
		cass_statement_free(statement);

		const CassResult* result = cass_future_get_result(query);
		if (result) {
			std::cerr << "inserted" << std::endl;
		} else {
			const char* error_message;
			size_t error_message_length;
			cass_future_error_message(query, &error_message, &error_message_length);
			std::cerr << "Error: " << error_message << std::endl;
		}
		cass_result_free(result);
		cass_future_free(query);

		return true;
	}

	DbConnection::~DbConnection()
	{
		CassFuture* futureClose = cass_session_close(_session);
		cass_future_wait(futureClose);
		cass_future_free(futureClose);
		cass_future_free(_futureConn);
		cass_cluster_free(_cluster);
		cass_session_free(_session);
	}
}
