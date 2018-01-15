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
#include <mutex>
#include <exception>
#include <vector>
#include <utility>

#include <cassandra.h>
#include <syslog.h>
#include <unistd.h>

#include "dbconnection.h"

namespace meteodata {
	DbConnection::DbConnection(const std::string& address, const std::string& user, const std::string& password) :
		_cluster{cass_cluster_new()},
		_session{cass_session_new()},
		_selectStationByCoords{nullptr, cass_prepared_free},
		_selectStationDetails{nullptr, cass_prepared_free},
		_selectLastDataInsertionTime{nullptr, cass_prepared_free},
		_insertDataPoint{nullptr, cass_prepared_free},
		_updateLastArchiveDownloadTime{nullptr, cass_prepared_free},
		_selectWeatherlinkStations{nullptr, cass_prepared_free}
	{
		cass_cluster_set_contact_points(_cluster, address.c_str());
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

	void DbConnection::prepareStatements()
	{
		CassFuture* prepareFuture = cass_session_prepare(_session, "SELECT station FROM meteodata.coordinates WHERE elevation = ? AND latitude = ? AND longitude = ?");
		CassError rc = cass_future_error_code(prepareFuture);
		if (rc != CASS_OK) {
			std::string desc("Could not prepare statement selectStationByCoords: ");
			desc.append(cass_error_desc(rc));
			throw std::runtime_error(desc);
		}
		_selectStationByCoords.reset(cass_future_get_prepared(prepareFuture));
		cass_future_free(prepareFuture);

		prepareFuture = cass_session_prepare(_session, "SELECT name,polling_period,last_archive_download FROM meteodata.stations WHERE id = ?");
		rc = cass_future_error_code(prepareFuture);
		if (rc != CASS_OK) {
			std::string desc("Could not prepare statement selectStationDetails: ");
			desc.append(cass_error_desc(rc));
			throw std::runtime_error(desc);
		}
		_selectStationDetails.reset(cass_future_get_prepared(prepareFuture));
		cass_future_free(prepareFuture);

		prepareFuture = cass_session_prepare(_session, "SELECT time FROM meteodata.meteo WHERE station = ? LIMIT 1");
		rc = cass_future_error_code(prepareFuture);
		if (rc != CASS_OK) {
			std::string desc("Could not prepare statement selectLastInsertionTime: ");
			desc.append(cass_error_desc(rc));
			throw std::runtime_error(desc);
		}
		_selectLastDataInsertionTime.reset(cass_future_get_prepared(prepareFuture));
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
			"sunrise, sunset,"
			"rain_archive, etp_archive)"
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
			"?,?,"
			"?,?)");

		rc = cass_future_error_code(prepareFuture);
		if (rc != CASS_OK) {
			std::string desc("Could not prepare statement insertdataPoint: ");
			desc.append(cass_error_desc(rc));
			throw std::runtime_error(desc);
		}
		_insertDataPoint.reset(cass_future_get_prepared(prepareFuture));
		cass_future_free(prepareFuture);

		prepareFuture = cass_session_prepare(_session, "UPDATE meteodata.stations SET last_archive_download = ? WHERE id = ?");
		rc = cass_future_error_code(prepareFuture);
		if (rc != CASS_OK) {
			std::string desc("Could not prepare statement updateLastArchiveDownloadTime: ");
			desc.append(cass_error_desc(rc));
			throw std::runtime_error(desc);
		}
		_updateLastArchiveDownloadTime.reset(cass_future_get_prepared(prepareFuture));
		cass_future_free(prepareFuture);

		prepareFuture = cass_session_prepare(_session, "SELECT * FROM meteodata.weatherlink");
		rc = cass_future_error_code(prepareFuture);
		if (rc != CASS_OK) {
			std::string desc("Could not prepare statement selectWeatherlinkStations: ");
			desc.append(cass_error_desc(rc));
			throw std::runtime_error(desc);
		}
		_selectWeatherlinkStations.reset(cass_future_get_prepared(prepareFuture));
		cass_future_free(prepareFuture);
	}

	// /!\ must be called under _selectMutex lock
	bool DbConnection::getStationDetails(const CassUuid& uuid, std::string& name, int& pollPeriod, time_t& lastArchiveDownloadTime)
	{
		CassFuture* query;
		CassStatement* statement = cass_prepared_bind(_selectStationDetails.get());
		std::cerr << "Statement prepared" << std::endl;
		cass_statement_bind_uuid(statement, 0, uuid);
		query = cass_session_execute(_session, statement);
		std::cerr << "Executed statement getStationDetails" << std::endl;
		cass_statement_free(statement);

		const CassResult* result = cass_future_get_result(query);
		bool ret = false;
		if (result) {
			const CassRow* row = cass_result_first_row(result);
			if (row) {
				const char *stationName;
				size_t size;
				cass_value_get_string(cass_row_get_column(row,0), &stationName, &size);
				cass_value_get_int32(cass_row_get_column(row,1), &pollPeriod);
				cass_int64_t timeMillisec;
				cass_value_get_int64(cass_row_get_column(row,2), &timeMillisec);
				lastArchiveDownloadTime = timeMillisec/1000;
				name.clear();
				name.insert(0, stationName, size);
				ret = true;
			}
		}
		cass_result_free(result);
		cass_future_free(query);

		return ret;
	}
	
	// /!\ must be called under _selectMutex lock
	bool DbConnection::getLastDataInsertionTime(const CassUuid& uuid, time_t& lastDataInsertionTime)
	{
		CassFuture* query;
		CassStatement* statement = cass_prepared_bind(_selectLastDataInsertionTime.get());
		std::cerr << "Statement prepared" << std::endl;
		cass_statement_bind_uuid(statement, 0, uuid);
		query = cass_session_execute(_session, statement);
		std::cerr << "Executed statement getLastDataInsertionTime" << std::endl;
		cass_statement_free(statement);

		const CassResult* result = cass_future_get_result(query);
		bool ret = false;
		if (result) {
			const CassRow* row = cass_result_first_row(result);
			ret = true;
			if (row) {
				cass_int64_t insertionTimeMillisec;
				cass_value_get_int64(cass_row_get_column(row,0), &insertionTimeMillisec);
				std::cerr << "Last insertion was at " << insertionTimeMillisec << std::endl;
				lastDataInsertionTime = insertionTimeMillisec / 1000;
			} else {
				lastDataInsertionTime = 0;
			}
		}
		cass_result_free(result);
		cass_future_free(query);

		return ret;
	}

	bool DbConnection::getStationByCoords(int elevation, int latitude, int longitude, CassUuid& station, std::string& name, int& pollPeriod, time_t& lastArchiveDownloadTime, time_t& lastDataInsertionTime)
	{
		CassFuture* query;
		{ /* mutex scope */
			std::lock_guard<std::mutex> queryMutex{_selectMutex};
			CassStatement* statement = cass_prepared_bind(_selectStationByCoords.get());
			std::cerr << "Statement prepared" << std::endl;
			cass_statement_bind_int32(statement, 0, elevation);
			cass_statement_bind_int32(statement, 1, latitude);
			cass_statement_bind_int32(statement, 2, longitude);
			query = cass_session_execute(_session, statement);
			std::cerr << "Executed statement getStationByCoords" << std::endl;
			cass_statement_free(statement);
		}

		const CassResult* result = cass_future_get_result(query);
		bool ret = false;
		if (result) {
			const CassRow* row = cass_result_first_row(result);
			if (row) {
				cass_value_get_uuid(cass_row_get_column(row,0), &station);
				ret = getStationDetails(station, name, pollPeriod, lastArchiveDownloadTime);
				if (ret)
					getLastDataInsertionTime(station, lastDataInsertionTime);
			}
		}
		cass_result_free(result);
		cass_future_free(query);

		return ret;
	}

	bool DbConnection::insertDataPoint(const CassUuid station, const Message& msg)
	{
		CassFuture* query;
		{ /* mutex scope */
			std::lock_guard<std::mutex> queryMutex{_insertMutex};
			std::cerr << "About to insert data point in database" << std::endl;
			CassStatement* statement = cass_prepared_bind(_insertDataPoint.get());
			msg.populateDataPoint(station, statement);
			query = cass_session_execute(_session, statement);
			cass_statement_free(statement);
		}

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

	bool DbConnection::updateLastArchiveDownloadTime(const CassUuid station, const time_t& time)
	{
		CassFuture* query;
		{ /* mutex scope */
			std::lock_guard<std::mutex> queryMutex{_updateLastArchiveDownloadMutex};
			std::cerr << "About to update an archive download time in database" << std::endl;
			CassStatement* statement = cass_prepared_bind(_updateLastArchiveDownloadTime.get());
			cass_statement_bind_int64(statement, 0, time * 1000);
			cass_statement_bind_uuid(statement, 1, station);
			query = cass_session_execute(_session, statement);
			cass_statement_free(statement);
		}

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

	bool DbConnection::getAllWeatherlinkStations(std::vector<std::tuple<CassUuid, std::string, int>>& stations)
	{
		std::unique_ptr<CassStatement, void(&)(CassStatement*)> statement{
			cass_prepared_bind(_selectWeatherlinkStations.get()),
			cass_statement_free
		};
		std::unique_ptr<CassFuture, void(&)(CassFuture*)> query{
			cass_session_execute(_session, statement.get()),
			cass_future_free
		};

		const CassResult* result = cass_future_get_result(query.get());
		bool ret = false;
		if (result) {
			std::unique_ptr<CassIterator, void(&)(CassIterator*)> iterator{
				cass_iterator_from_result(result),
				cass_iterator_free
			};
			while (cass_iterator_next(iterator.get())) {
				const CassRow* row = cass_iterator_get_row(iterator.get());
				CassUuid station;
				cass_value_get_uuid(cass_row_get_column(row,0), &station);
				const char *authString;
				size_t size;
				cass_value_get_string(cass_row_get_column(row,1), &authString, &size);
				int timezone;
				cass_value_get_int32(cass_row_get_column(row,2), &timezone);
				stations.emplace_back(station, std::string{authString, size}, timezone);
			}
			ret = true;
		}

		return ret;
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
