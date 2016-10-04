#ifndef DBCONNECTION_H
#define DBCONNECTION_H

#include <cassandra.h>

#include <functional>
#include <tuple>
#include <memory>

#include "message.h"

namespace meteodata {
	class DbConnection
	{
	public:
		DbConnection(const std::string& user = "", const std::string& password = "");
		virtual ~DbConnection();
		std::tuple<std::string,int,int> getStationById(const std::string& id);
		CassUuid getStationByCoords(int latitude, int longitude, int altitude);
		bool insertDataPoint(const CassUuid station, const Message& message);

	private:
		CassFuture* _futureConn;
		CassCluster* _cluster;
		CassSession* _session;
		std::unique_ptr<const CassPrepared, std::function<void(const CassPrepared*)>> _selectStationById;
		std::unique_ptr<const CassPrepared, std::function<void(const CassPrepared*)>> _selectStationByCoords;
		std::unique_ptr<const CassPrepared, std::function<void(const CassPrepared*)>> _insertDataPoint;
		void prepareStatements();
	};
}

#endif
