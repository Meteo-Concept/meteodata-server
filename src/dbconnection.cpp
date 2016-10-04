#include <iostream>

#include <cassandra.h>
#include <syslog.h>
#include <unistd.h>

#include "dbconnection.h"

namespace meteodata {
	DbConnection::DbConnection(const std::string& user, const std::string& password) :
		_cluster{cass_cluster_new()},
		_session{cass_session_new()},
		_selectStationById{nullptr, cass_prepared_free},
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
		CassFuture* prepareFuture = cass_session_prepare(_session, "SELECT address, port, polling_period FROM meteodata.stations WHERE id = ?");
		CassError rc = cass_future_error_code(prepareFuture);
		if (rc != CASS_OK) {
			syslog(LOG_ERR, "Could not prepare statement selectStationById: %s", cass_error_desc(rc));
		}
		_selectStationById.reset(cass_future_get_prepared(prepareFuture));

		prepareFuture = cass_session_prepare(_session, "SELECT station FROM meteodata.coordinates WHERE elevation = ? AND latitude = ? AND longitude = ?");
		rc = cass_future_error_code(prepareFuture);
		if (rc != CASS_OK) {
			syslog(LOG_ERR, "Could not prepare statement selectStationByCoords: %s", cass_error_desc(rc));
		}
		_selectStationByCoords.reset(cass_future_get_prepared(prepareFuture));

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
	}

	std::tuple<std::string,int,int> DbConnection::getStationById(const std::string& id)
	{
		CassStatement* statement = cass_prepared_bind(_selectStationById.get());
		CassUuid stationId;
		cass_uuid_from_string_n(id.c_str(), id.length(), &stationId);
		cass_statement_bind_uuid(statement, 0, stationId);
		CassFuture* query = cass_session_execute(_session, statement);

		cass_statement_free(statement);
		const CassResult* result = cass_future_get_result(query);
		if (result) {
			const CassRow* row = cass_result_first_row(result);
			const char* address;
			size_t addressLength;
			cass_value_get_string(cass_row_get_column(row,0), &address, &addressLength);
			int32_t port = 0;
			cass_value_get_int32(cass_row_get_column(row,1), &port);
			int32_t pollingPeriod = 0;
			cass_value_get_int32(cass_row_get_column(row,2), &pollingPeriod);
			cass_result_free(result);
			return std::make_tuple(address, port, pollingPeriod);
		}
		return std::make_tuple("",0,0);
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
		if (result) {
			std::cerr << "We have a result" << std::endl;
			const CassRow* row = cass_result_first_row(result);
			if (row) {
				CassUuid uuid;
				cass_value_get_uuid(cass_row_get_column(row,0), &uuid);
				cass_result_free(result);
				return uuid;
			}
		}

		std::cerr << "No result" << std::endl;
		CassUuid defaultUuid;
		cass_uuid_from_string("000000000-0000-0000-0000-000000000000", &defaultUuid);
		return defaultUuid;
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
			cass_result_free(result);
		} else {
			const char* error_message;
			size_t error_message_length;
			cass_future_error_message(query, &error_message, &error_message_length);
			std::cerr << "Error: " << error_message << std::endl;
		}
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
