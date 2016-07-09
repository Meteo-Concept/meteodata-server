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

#include "connector.h"
#include "message.h"


namespace meteodata {

namespace ip = boost::asio::ip;
namespace asio = boost::asio;
namespace sys = boost::system; //system() is a function, it cannot be redefined
//as a namespace
namespace chrono = boost::posix_time;

using namespace std::placeholders;
using namespace meteodata;

struct Message;

class VantagePro2Connector : public Connector
{
public:
	VantagePro2Connector(boost::asio::io_service& ioService) :
		Connector(ioService),
		_timer(ioService)
	{}

	//main loop
	void start()
	{
		int txrxErrors = 0;
		sys::error_code e;

		for (;;)
		{
			_timer.expires_from_now(chrono::pos_infin);
			_timer.async_wait(std::bind(&VantagePro2Connector::checkDeadline, std::static_pointer_cast<VantagePro2Connector>(shared_from_this()), _1));
			do {
				if (wakeUp())
					stop();
				e = askForData();
				if (! validateMessage()) {
					txrxErrors++;
					e = sys::errc::make_error_code(sys::errc::io_error);
				}
				if (e == sys::errc::timed_out)
					++_timeouts;
			} while(_timeouts < 3 && txrxErrors < 3 && e);

			// irrecoverable error
			if (e) {
				std::cerr << "Error " << e.message() << std::endl;
				stop();
			}

			// I/O finished

			storeData();
			_timer.cancel();
			_timer.expires_from_now(chrono::minutes(5));
			_timer.wait();
		}
	}

	private:
	static constexpr int CRC_VALUES[] =
	{
		0x0, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5, 0x60c6, 0x70e7,
		0x8108, 0x9129, 0xa14a, 0xb16b, 0xc18c, 0xd1ad, 0xe1ce, 0xf1ef,
		0x1231, 0x210, 0x3273, 0x2252, 0x52b5, 0x4294, 0x72f7, 0x62d6,
		0x9339, 0x8318, 0xb37b, 0xa35a, 0xd3bd, 0xc39c, 0xf3ff, 0xe3de,
		0x2462, 0x3443, 0x420, 0x1401, 0x64e6, 0x74c7, 0x44a4, 0x5485,
		0xa56a, 0xb54b, 0x8528, 0x9509, 0xe5ee, 0xf5cf, 0xc5ac, 0xd58d,
		0x3653, 0x2672, 0x1611, 0x630, 0x76d7, 0x66f6, 0x5695, 0x46b4,
		0xb75b, 0xa77a, 0x9719, 0x8738, 0xf7df, 0xe7fe, 0xd79d, 0xc7bc,
		0x48c4, 0x58e5, 0x6886, 0x78a7, 0x840, 0x1861, 0x2802, 0x3823,
		0xc9cc, 0xd9ed, 0xe98e, 0xf9af, 0x8948, 0x9969, 0xa90a, 0xb92b,
		0x5af5, 0x4ad4, 0x7ab7, 0x6a96, 0x1a71, 0xa50, 0x3a33, 0x2a12,
		0xdbfd, 0xcbdc, 0xfbbf, 0xeb9e, 0x9b79, 0x8b58, 0xbb3b, 0xab1a,
		0x6ca6, 0x7c87, 0x4ce4, 0x5cc5, 0x2c22, 0x3c03, 0xc60, 0x1c41,
		0xedae, 0xfd8f, 0xcdec, 0xddcd, 0xad2a, 0xbd0b, 0x8d68, 0x9d49,
		0x7e97, 0x6eb6, 0x5ed5, 0x4ef4, 0x3e13, 0x2e32, 0x1e51, 0xe70,
		0xff9f, 0xefbe, 0xdfdd, 0xcffc, 0xbf1b, 0xaf3a, 0x9f59, 0x8f78,
		0x9188, 0x81a9, 0xb1ca, 0xa1eb, 0xd10c, 0xc12d, 0xf14e, 0xe16f,
		0x1080, 0xa1, 0x30c2, 0x20e3, 0x5004, 0x4025, 0x7046, 0x6067,
		0x83b9, 0x9398, 0xa3fb, 0xb3da, 0xc33d, 0xd31c, 0xe37f, 0xf35e,
		0x2b1, 0x1290, 0x22f3, 0x32d2, 0x4235, 0x5214, 0x6277, 0x7256,
		0xb5ea, 0xa5cb, 0x95a8, 0x8589, 0xf56e, 0xe54f, 0xd52c, 0xc50d,
		0x34e2, 0x24c3, 0x14a0, 0x481, 0x7466, 0x6447, 0x5424, 0x4405,
		0xa7db, 0xb7fa, 0x8799, 0x97b8, 0xe75f, 0xf77e, 0xc71d, 0xd73c,
		0x26d3, 0x36f2, 0x691, 0x16b0, 0x6657, 0x7676, 0x4615, 0x5634,
		0xd94c, 0xc96d, 0xf90e, 0xe92f, 0x99c8, 0x89e9, 0xb98a, 0xa9ab,
		0x5844, 0x4865, 0x7806, 0x6827, 0x18c0, 0x8e1, 0x3882, 0x28a3,
		0xcb7d, 0xdb5c, 0xeb3f, 0xfb1e, 0x8bf9, 0x9bd8, 0xabbb, 0xbb9a,
		0x4a75, 0x5a54, 0x6a37, 0x7a16, 0xaf1, 0x1ad0, 0x2ab3, 0x3a92,
		0xfd2e, 0xed0f, 0xdd6c, 0xcd4d, 0xbdaa, 0xad8b, 0x9de8, 0x8dc9,
		0x7c26, 0x6c07, 0x5c64, 0x4c45, 0x3ca2, 0x2c83, 0x1ce0, 0xcc1,
		0xef1f, 0xff3e, 0xcf5d, 0xdf7c, 0xaf9b, 0xbfba, 0x8fd9, 0x9ff8,
		0x6e17, 0x7e36, 0x4e55, 0x5e74, 0x2e93, 0x3eb2, 0xed1, 0x1ef0
	};

	void checkDeadline(const sys::error_code& e)
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

	bool validateCRC(const void* msg, size_t len)
	{
		//byte-wise reading
		const char* bytes = reinterpret_cast<const char*>(msg);
		unsigned int crc = 0;
		for (unsigned int i=0 ; i<len ; i++) {
			uint8_t index = (crc >> 8) ^ bytes[i];
			crc = CRC_VALUES[index] ^ ((crc << 8) & 0xFFFF);
		}

		return crc == 0;
	}

	void stop()
	{
		_stopped = true;
		_timer.cancel();
		_sock.close();
	}

	sys::error_code wakeUp()
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
			}
			std::cerr << e.value() << ": " << e.message() << std::endl;
		} while (_timeouts < 3 && e == sys::errc::operation_canceled);
		if (_timeouts >= 3)
			e = sys::errc::make_error_code(sys::errc::timed_out);
		return e;
	}

	sys::error_code askForData()
	{
		sys::error_code e;
		char req[9] = "LPS 3 2\n";
		std::cerr << "Asking for data" << std::endl;
		//flush the socket first
		if (_sock.available() > 0) {
			asio::streambuf::mutable_buffers_type bufs = _discardBuffer.prepare(512);
			std::size_t bytes = _sock.receive(bufs);
			_discardBuffer.commit(bytes);
			std::cerr << "Cleared " << bytes << " bytes" << std::endl;
		}
		_discardBuffer.consume(_discardBuffer.size());

		write(_sock, asio::buffer(req, 8), e);
		std::cerr << "Waiting for data" << std::endl;
		_timer.expires_from_now(chrono::seconds(6));

		e = asio::error::would_block;
		async_read_until(_sock, _discardBuffer, 0x06,
			[&e](const sys::error_code& ec, size_t) {
				e = ec;
			});
		do _ioService.run_one(); while (e == asio::error::would_block);
		if (e)
			return e;

		e = asio::error::would_block;
		async_read(_sock, _inputBuffer,
			[&e,this](const sys::error_code& ec, size_t) {
				e = ec;
			});
		do _ioService.run_one(); while (e == asio::error::would_block);
		_timer.expires_from_now(chrono::seconds(15));

		return e;
	}

	bool validateMessage()
	{
		std::cerr << "Message received" << std::endl;
		return validateCRC(_l1, sizeof(Loop1)) &&
		       validateCRC(_l2, sizeof(Loop2));
	};

	void storeData()
	{
		std::cerr << "Message validated" << std::endl;
		_db.insertDataPoint(_l1[0], _l2[0]);
	}

	asio::deadline_timer _timer;
	asio::streambuf _discardBuffer;

	Loop1 _l1[1]; //it has to be an array to be used as an asio buffer
	Loop2 _l2[1];
	std::array<asio::mutable_buffer, 2> _inputBuffer = {
			asio::buffer(_l1),
			asio::buffer(_l2),
	};
	bool _stopped = false;
	int _timeouts = 0;
};

}

#endif
