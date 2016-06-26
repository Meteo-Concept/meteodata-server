#include <iostream>

#include <cassandra.h>
#include <syslog.h>
#include <unistd.h>

#include "dbconnection.h"

namespace meteodata {
	DbConnection::DbConnection() :
		_cluster{cass_cluster_new()},
		_session{cass_session_new()},
		_selectStationById{nullptr, cass_prepared_free},
		_insertDataPoint{nullptr, cass_prepared_free}
	{
		cass_log_set_level(CASS_LOG_INFO);
		cass_cluster_set_contact_points(_cluster, "127.0.0.1");
		_futureConn = cass_session_connect(_session, _cluster);
		CassError rc = cass_future_error_code(_futureConn);
		if (rc != CASS_OK) {
			syslog(LOG_ERR, "Impossible to connect to database");
		}
		prepareStatements();
	}

	void DbConnection::prepareStatements()
	{
		CassFuture* prepareFuture = cass_session_prepare(_session, "SELECT address, port, polling_period FROM meteodata.stations WHERE id = ?");
		CassError rc = cass_future_error_code(prepareFuture);
		if (rc != CASS_OK) {
			syslog(LOG_ERR, "Could not prepare statement selectStationById: %s", cass_error_desc(rc));
		}
		_selectStationById.reset(cass_future_get_prepared(prepareFuture));

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
			return std::make_tuple(address, port, pollingPeriod);
		}
		return std::make_tuple("",0,0);
	}

	bool DbConnection::insertDataPoint(const Loop1 l1, const Loop2 l2)
	{
		std::cerr << "About to insert data point in database" << std::endl;
		CassStatement* statement = cass_prepared_bind(_insertDataPoint.get());
		populateDataPoint(l1, l2, statement);
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
