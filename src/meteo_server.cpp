#include <syslog.h>

#include <functional>

#include <boost/asio.hpp>

#include "meteo_server.h"
#include "connector.h"
#include "vantagepro2connector.h"

using namespace boost::asio;
using namespace boost::asio::ip;

namespace meteodata
{

MeteoServer::MeteoServer(boost::asio::io_service& ioService, const std::string& user, const std::string& password) :
	_acceptor(ioService, tcp::endpoint(tcp::v4(), 5886)),
	_user(user),
	_password(password)
{
	startAccepting();
}

void MeteoServer::startAccepting()
{
	Connector::ptr newConnector =
		Connector::create<VantagePro2Connector>(_acceptor.get_io_service(), _user, _password);
	_acceptor.async_accept(newConnector->socket(),
		std::bind(&MeteoServer::runNewConnector, this,
			newConnector, std::placeholders::_1)
	);
}

void MeteoServer::runNewConnector(Connector::ptr c,
		const boost::system::error_code& error)
{
	if (!error) {
		c->start();
		startAccepting();
	}
}
}
