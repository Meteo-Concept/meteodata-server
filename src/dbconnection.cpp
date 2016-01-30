#include <cassandra.h>

#include "dbconnection.h"

namespace meteodata {
	DbConnection::DbConnection() :
		_cluster{cass_cluster_new()},
		_session{cass_session_new()}
	{
		cass_cluster_set_contact_points(cluster, "127.0.0.1");
		_futureConn = cass_session_connect(session, cluster);
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
