#include <iostream>

#include <boost/asio.hpp>

#include "connector.h"

namespace meteodata
{

Connector::Connector(boost::asio::io_service& ioService) :
	_sock(ioService),
	_ioService(ioService)
{}

}
