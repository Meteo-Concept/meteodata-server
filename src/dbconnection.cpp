#include <cassandra.h>
#include <syslog.h>
#include <unistd.h>

#include "dbconnection.h"

namespace meteodata {
	DbConnection::DbConnection() :
		_cluster{cass_cluster_new()},
		_session{cass_session_new()}
	{
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
		CassFuture* prepareFuture = cass_session_prepare(_session, "SELECT (address, port, polling_period) FROM stations WHERE id = '?';");
		CassError rc = cass_future_error_code(prepareFuture);
		if (rc != CASS_OK) {
			syslog(LOG_ERR, "Could not prepare statement selectStationById");
		}
		_selectStationById.reset(cass_future_get_prepared(prepareFuture));
	}

	std::tuple<std::string,int,int> DbConnection::getStationById(std::string id)
	{
		CassStatement* statement = cass_prepared_bind(_selectStationById.get());
		cass_statement_bind_string(statement, 0, id.c_str());
		CassFuture* query = cass_session_execute(_session, statement);

		cass_statement_free(statement);
		const CassResult* result = cass_future_get_result(query);
		if (result) {
			const CassRow* row = cass_result_first_row(result);
			const char* address;
			size_t addressLength;
			cass_value_get_string(cass_row_get_column(row,0), &address, &addressLength);
			cass_int32_t port;
			cass_value_get_int32(cass_row_get_column(row,1), &port);
			cass_int32_t pollingPeriod;
			cass_value_get_int32(cass_row_get_column(row,2), &pollingPeriod);
			return std::make_tuple(address, port, pollingPeriod);
		}
		return std::make_tuple("",0,0);
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
