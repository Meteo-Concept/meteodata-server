#include "station.h"

namespace meteodata {
	Station::Station(std::string id, std::shared_ptr<DbConnection> db) : _id(id), _db(db)
	{

	}
}

