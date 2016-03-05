#ifndef METEO_SERVER_H
#define METEO_SERVER_H

#include <boost/asio.hpp>

#include "meteo_server.h"
#include "connector.h"

namespace meteodata {
class MeteoServer {

	public:
		MeteoServer(boost::asio::io_service& io);
		void startAccepting();
		void runNewConnector(Connector::ptr c,
			const boost::system::error_code& error);

	private:
		boost::asio::ip::tcp::acceptor _acceptor;
		// _runningConnectors
};
}

#endif /* ifndef METEO_SERVER_H */
