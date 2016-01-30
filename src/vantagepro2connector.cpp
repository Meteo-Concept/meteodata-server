#include <iostream>
#include <boost/array.hpp>
#include <boost/asio.hpp>
#include "vantagepro2connector.h"
#include "message.h"

using namespace boost::asio;

VantagePro2Connector::VantagePro2Connector(std::string remote, int port) :
	_resolver(_ioService),
	_query(remote, std::to_string(port),
		ip::resolver_query_base::numeric_service |
		ip::resolver_query_base::passive |
		ip::resolver_query_base::address_configured),
	_endpointIterator(_resolver.resolve(_query))
{

}


// assume the CRC is the last two bytes
bool VantagePro2Connector::validateCrc(const Message& msg)
{
	unsigned int crc = 0;
	for (char byte : msg) {
		unsigned int index = (crc >> 8) ^ byte;
		crc = VantagePro2Connector::CRC_VALUES[index] ^((crc << 8) & 0xFFFF);
	}

	return crc == 0;
}


