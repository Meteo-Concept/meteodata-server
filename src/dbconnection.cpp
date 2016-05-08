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

	bool DbConnection::insertDataPoint(const DataPoint& dp)
	{
		CassStatement* statement = cass_prepared_bind(_insertDataPoint.get());
		CassUuid stationId;
		cass_uuid_from_string_n(dp.station.c_str(), dp.station.length(), &stationId);
		cass_statement_bind_uuid(statement, 0, stationId);
		cass_statement_bind_int64(statement, 1, dp.time);
		cass_statement_bind_int32(statement, 2, dp.bartrend);
		cass_statement_bind_int32(statement, 3, dp.barometer);
		cass_statement_bind_int32(statement, 4, dp.barometer_abs);
		cass_statement_bind_int32(statement, 5, dp.barometer_raw);
		cass_statement_bind_int32(statement, 6, dp.insidetemp);
		cass_statement_bind_int32(statement, 7, dp.outsidetemp);
		cass_statement_bind_int32(statement, 8, dp.insidehum);
		cass_statement_bind_int32(statement, 9, dp.outsidehum);
		cass_statement_bind_int32(statement, 10, dp.extratemp[0]);
		cass_statement_bind_int32(statement, 11, dp.extratemp[1]);
		cass_statement_bind_int32(statement, 12, dp.extratemp[2]);
		cass_statement_bind_int32(statement, 13, dp.extratemp[3]);
		cass_statement_bind_int32(statement, 14, dp.extratemp[4]);
		cass_statement_bind_int32(statement, 15, dp.extratemp[5]);
		cass_statement_bind_int32(statement, 16, dp.extratemp[6]);
		cass_statement_bind_int32(statement, 17, dp.soiltemp[0]);
		cass_statement_bind_int32(statement, 18, dp.soiltemp[1]);
		cass_statement_bind_int32(statement, 19, dp.soiltemp[2]);
		cass_statement_bind_int32(statement, 20, dp.soiltemp[3]);
		cass_statement_bind_int32(statement, 21, dp.leaftemp[0]);
		cass_statement_bind_int32(statement, 22, dp.leaftemp[1]);
		cass_statement_bind_int32(statement, 23, dp.leaftemp[2]);
		cass_statement_bind_int32(statement, 24, dp.leaftemp[3]);
		cass_statement_bind_int32(statement, 25, dp.extrahum[0]);
		cass_statement_bind_int32(statement, 26, dp.extrahum[1]);
		cass_statement_bind_int32(statement, 27, dp.extrahum[2]);
		cass_statement_bind_int32(statement, 28, dp.extrahum[3]);
		cass_statement_bind_int32(statement, 29, dp.extrahum[4]);
		cass_statement_bind_int32(statement, 30, dp.extrahum[5]);
		cass_statement_bind_int32(statement, 31, dp.extrahum[6]);
		cass_statement_bind_int32(statement, 32, dp.soilmoistures[0]);
		cass_statement_bind_int32(statement, 33, dp.soilmoistures[1]);
		cass_statement_bind_int32(statement, 34, dp.soilmoistures[2]);
		cass_statement_bind_int32(statement, 35, dp.soilmoistures[3]);
		cass_statement_bind_int32(statement, 36, dp.leafwetnesses[0]);
		cass_statement_bind_int32(statement, 37, dp.leafwetnesses[1]);
		cass_statement_bind_int32(statement, 38, dp.leafwetnesses[2]);
		cass_statement_bind_int32(statement, 39, dp.leafwetnesses[3]);
		cass_statement_bind_int32(statement, 40, dp.windspeed);
		cass_statement_bind_int32(statement, 41, dp.winddir);
		cass_statement_bind_int32(statement, 42, dp.avgwindspeed_10min);
		cass_statement_bind_int32(statement, 43, dp.avgwindspeed_2min);
		cass_statement_bind_int32(statement, 44, dp.windgust_10min);
		cass_statement_bind_int32(statement, 45, dp.windgustdir);
		cass_statement_bind_int32(statement, 46, dp.rainrate);
		cass_statement_bind_int32(statement, 47, dp.rain_15min);
		cass_statement_bind_int32(statement, 48, dp.rain_1h);
		cass_statement_bind_int32(statement, 49, dp.rain_24h);
		cass_statement_bind_int32(statement, 50, dp.dayrain);
		cass_statement_bind_int32(statement, 51, dp.monthrain);
		cass_statement_bind_int32(statement, 52, dp.yearrain);
		cass_statement_bind_int32(statement, 53, dp.stormrain);
		cass_statement_bind_uint32(statement, 54, dp.stormstartdate);
		cass_statement_bind_int32(statement, 55, dp.UV);
		cass_statement_bind_int32(statement, 56, dp.solarrad);
		cass_statement_bind_int32(statement, 57, dp.dewpoint);
		cass_statement_bind_int32(statement, 58, dp.heatindex);
		cass_statement_bind_int32(statement, 59, dp.windchill);
		cass_statement_bind_int32(statement, 60, dp.thswindex);
		cass_statement_bind_int32(statement, 61, dp.dayET);
		cass_statement_bind_int32(statement, 62, dp.monthET);
		cass_statement_bind_int32(statement, 63, dp.yearET);
		cass_statement_bind_string(statement, 64, dp.forecast.c_str());
		cass_statement_bind_int32(statement, 65, dp.forecast_icons);
		cass_statement_bind_int64(statement, 66, dp.sunrise);
		cass_statement_bind_int64(statement, 67, dp.sunset);
		cass_session_execute(_session, statement);
		cass_statement_free(statement);
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
