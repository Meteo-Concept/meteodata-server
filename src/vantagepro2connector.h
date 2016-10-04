#ifndef VANTAGEPRO2CONNECTOR_H
#define VANTAGEPRO2CONNECTOR_H

#include <iostream>
#include <memory>
#include <array>
#include <functional>

#include <boost/system/error_code.hpp>
#include <boost/asio.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/asio/deadline_timer.hpp>

#include <syslog.h>
#include <unistd.h>

#include <cassandra.h>

#include "connector.h"
#include "vantagepro2message.h"


namespace meteodata {

namespace ip = boost::asio::ip;
namespace asio = boost::asio;
namespace sys = boost::system; //system() is a function, it cannot be redefined
//as a namespace
namespace chrono = boost::posix_time;

using namespace std::placeholders;
using namespace meteodata;

class VantagePro2Connector : public Connector
{
public:
	VantagePro2Connector(boost::asio::io_service& ioService, const std::string& user, const std::string& password);

	//main loop
	void start();

private:
	void checkDeadline(const sys::error_code& e);
	void stop();
	sys::error_code wakeUp();
	void flushSocket();
	bool validateCoords();
	void storeData();

	template <typename MutableBuffer>
	sys::error_code askForData(const char* req, int reqSize, const MutableBuffer& buffer);

	asio::deadline_timer _timer;
	asio::streambuf _discardBuffer;
	VantagePro2Message _message;

	int16_t _coords[4]; // elevation, latitude, longitude, CRC
	bool _stopped = false;
	int _timeouts = 0;
	CassUuid _station;
};

}

#endif
