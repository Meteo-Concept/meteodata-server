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

#include "vantagepro2connector.h"
#include "connector.h"
#include "vantagepro2message.h"

namespace meteodata {

namespace ip = boost::asio::ip;
namespace asio = boost::asio;
namespace sys = boost::system; //system() is a function, it cannot be redefined
//as a namespace
namespace chrono = boost::posix_time;

using namespace std::placeholders;

VantagePro2Connector::VantagePro2Connector(boost::asio::io_service& ioService, const std::string& user, const std::string& password) :
	Connector(ioService, user, password),
	_timer(ioService)
{}

void VantagePro2Connector::start()
{
	int txrxErrors = 0;
	int dataTimeouts = 0;
	sys::error_code e;
	_timer.async_wait(std::bind(&VantagePro2Connector::checkDeadline, std::static_pointer_cast<VantagePro2Connector>(shared_from_this()), _1));

	_timer.expires_from_now(chrono::pos_infin);
	int16_t coords[4]; // elevation, latitude, longitude, CRC
	do {
		if (wakeUp()) {
			stop();
			return;
		}
		e = askForData("EEBRD 0B 06\n", 12, asio::buffer(coords));
		if (!VantagePro2Message::validateCRC(coords,sizeof(coords))) {
			txrxErrors++;
			e = sys::errc::make_error_code(sys::errc::io_error);
		}
		if (e == sys::errc::timed_out)
			++dataTimeouts;
	} while(dataTimeouts < 3 && txrxErrors < 3 && e);

	// irrecoverable error
	if (e) {
		std::cerr << "Error " << e.message() << std::endl;
		stop();
		return;
	}

	// From documentation, latitude, longitude and elevation are stored contiguously
	// in this order in the station's EEPROM
	_station = _db.getStationByCoords(coords[2], coords[0], coords[1]);

	for (;;)
	{
		txrxErrors = 0;
		_timeouts = 0;
		dataTimeouts = 0;
		_timer.expires_from_now(chrono::pos_infin);
		do {
			if (wakeUp()) {
				stop();
				return;
			}
			e = askForData("LPS 3 2\n", 8, _message.getBuffer());
			if (!_message.isValid()) {
				txrxErrors++;
				e = sys::errc::make_error_code(sys::errc::io_error);
			}
			if (e == sys::errc::timed_out)
				++dataTimeouts;
		} while(dataTimeouts < 3 && txrxErrors < 3 && e);

		// irrecoverable error
		if (e) {
			std::cerr << "Error " << e.message() << std::endl;
			stop();
			return;
		}

		// I/O finished
		storeData();
		_timer.cancel();
		_timer.expires_from_now(chrono::minutes(5));
		_timer.wait();
	}
}


void VantagePro2Connector::checkDeadline(const sys::error_code& e)
{
	std::cerr << "Expired!" << std::endl;
	// if the connector has already stopped, then let it die
	if (_stopped)
		return;
	std::cerr << "Checking the deadline..." << std::endl;

	// verify that the timeout is not spurious
	if (_timer.expires_at() <= asio::deadline_timer::traits_type::now() &&
			e != sys::errc::operation_canceled) {
		std::cerr << "Timed out!" << std::endl;
		_timeouts++;
		_sock.cancel();
		_timer.expires_from_now(chrono::pos_infin);
	}

	// restart the timer
	_timer.async_wait(std::bind(&VantagePro2Connector::checkDeadline, std::static_pointer_cast<VantagePro2Connector>(shared_from_this()), _1));
}

void VantagePro2Connector::stop()
{
	_stopped = true;
	_timer.cancel();
	_sock.close();
}

sys::error_code VantagePro2Connector::wakeUp()
{
	sys::error_code e;
	do {
		char ack[] = "\n";
		std::cerr << "Waking up the console" << std::endl;
		write(_sock, asio::buffer(ack,1), e);
		if (!e) {
			_timer.expires_from_now(chrono::seconds(6));
			std::cerr << "Receiving acknowledgement from station" << std::endl;
			e = asio::error::would_block;
			async_read_until(_sock, _discardBuffer, '\n',
					[&e,this](const sys::error_code& ec, size_t) {
					e = ec;
					});
			do _ioService.run_one(); while (e == asio::error::would_block);
			_timer.expires_from_now(chrono::pos_infin);
			flushSocket();
		}
		std::cerr << e.value() << ": " << e.message() << std::endl;
	} while (_timeouts < 3 && e == sys::errc::operation_canceled);
			if (_timeouts >= 3)
				e = sys::errc::make_error_code(sys::errc::timed_out);
			return e;
}

template <typename MutableBuffer>
sys::error_code VantagePro2Connector::askForData(const char* req, int reqSize, const MutableBuffer& buffer)
{
	sys::error_code e;
	std::cerr << "Asking for data" << std::endl;
	//flush the socket first
	flushSocket();

	write(_sock, asio::buffer(req, reqSize), e);
	std::cerr << "Waiting for data" << std::endl;

	_timer.expires_from_now(chrono::seconds(4));
	e = asio::error::would_block;
	char ack;
	async_read(_sock, asio::buffer(&ack, 1),
			[&e](const sys::error_code& ec, size_t) {
			e = ec;
			});
	do _ioService.run_one(); while (e == asio::error::would_block);
	_timer.expires_from_now(chrono::pos_infin);
	if (ack != 0x06)
		e = sys::errc::make_error_code(sys::errc::io_error);

	if (!e) {
		_timer.expires_from_now(chrono::seconds(6));
		e = asio::error::would_block;
		async_read(_sock, buffer,
				[&e,this](const sys::error_code& ec, size_t bytes) {
				e = ec;
				std::cerr << "Received " << bytes << " bytes of data" << std::endl;
				});
		do _ioService.run_one(); while (e == asio::error::would_block);
		_timer.expires_from_now(chrono::pos_infin);
	}

	// If a read operation has been interrupted, this is because
	// it failed to meet the deadline, in this case return a more
	// meaningful error code
	if (e == sys::errc::operation_canceled)
		e = sys::errc::make_error_code(sys::errc::timed_out);

	return e;
}

void VantagePro2Connector::flushSocket()
{
	if (_sock.available() > 0) {
		asio::streambuf::mutable_buffers_type bufs = _discardBuffer.prepare(512);
		std::size_t bytes = _sock.receive(bufs);
		_discardBuffer.commit(bytes);
		std::cerr << "Cleared " << bytes << " bytes" << std::endl;
	}
	_discardBuffer.consume(_discardBuffer.size());
}

void VantagePro2Connector::storeData()
{
	std::cerr << "Message validated" << std::endl;
	_db.insertDataPoint(_station, _message);
}

}
