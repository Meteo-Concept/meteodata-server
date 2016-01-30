#include <cassandra.h>

#include "dbconnection.h"

namespace meteodata {
	DbConnection::DbConnection() :
		_cluster{cass_cluster_new()},
		_session{cass_session_new()}
	{
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
