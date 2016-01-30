#include <iostream>
#include <boost/asio.hpp>

#include "connector.h"

struct Message;

class VantagePro2Connector : public Connector
{
public:
	VantagePro2Connector(std::string remote, int port);

private:
	static const int CRC_VALUES[];
	bool validateCrc(const Message& msg);

	boost::asio::io_service _ioService;
	boost::asio::ip::tcp::resolver _resolver;
	boost::asio::ip::tcp::resolver::query _query;
	boost::asio::ip::tcp::resolver::iterator _endpointIterator;
};
