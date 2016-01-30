#ifndef STATION_H
#define STATION_H

#include <memory>
#include <string>

#include "dbconnection.h"
#include "connector.h"

namespace meteodata {
	class Station
	{
	public:
		Station(std::string id, std::shared_ptr<DbConnection> db);

	private:
		std::string _id;
		std::string _address;
		int _port;
		std::unique_ptr<Connector> _handle;
		std::shared_ptr<DbConnection> _db;
	};
}

#endif // STATION_H
