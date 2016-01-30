#include <cassandra.h>

namespace meteodata {
    class DbConnection
    {
        public:
            DbConnection();
            virtual ~DbConnection();
            void reconnect();

        private:
            CassFuture* _futureConn;
            CassCluster* _cluster;
            CassSession* _session;
    };
}
