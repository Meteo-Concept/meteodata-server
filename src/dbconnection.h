#include <cassandra.h>

#include <tuple>
#include <memory>

namespace meteodata {
	class DbConnection
	{
	public:
		DbConnection();
		virtual ~DbConnection();
		std::tuple<std::string,int,int> getStationById(std::string id);

	private:
		CassFuture* _futureConn;
		CassCluster* _cluster;
		CassSession* _session;
		std::unique_ptr<const CassPrepared> _selectStationById;
		void prepareStatements();
	};
}
