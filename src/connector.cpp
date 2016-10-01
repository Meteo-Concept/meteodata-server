#include <iostream>

#include <boost/asio.hpp>

#include "connector.h"

namespace meteodata
{

Connector::Connector(boost::asio::io_service& ioService, const std::string& user, const std::string& password) :
	_sock(ioService),
	_ioService(ioService),
	_db(user, password)
{}

}
