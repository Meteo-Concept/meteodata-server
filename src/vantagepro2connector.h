#ifndef VANTAGEPRO2CONNECTOR_H
#define VANTAGEPRO2CONNECTOR_H

#include <iostream>
#include <memory>

#include <boost/asio.hpp>
#include <boost/asio/deadline_timer.hpp>

#include "connector.h"

namespace meteodata {
	struct Message;

	class VantagePro2Connector : public Connector
	{
	public:
		VantagePro2Connector(boost::asio::io_service& ioService);
		virtual ~VantagePro2Connector();
		//void getOneDataPoint();
		void start() override;

	private:
		static const int CRC_VALUES[];
		bool validateCrc(const Message& msg);
		std::shared_ptr<VantagePro2Connector> casted_shared_from_this() {
			return std::static_pointer_cast<VantagePro2Connector>(shared_from_this());
		}
		boost::asio::deadline_timer _timer;
		boost::asio::streambuf _inputBuffer;
		bool _stopped = false;

		// the console has been sent \n\n
		void handleWakenUp(const boost::system::error_code& error);
		// the console has answered and is awake
		void handleAnswerWakeUp(const boost::system::error_code& error);
		
		// the console has been asked for data
		void handleAskedForData(const boost::system::error_code& error);
		// the console has sent data
		void handleAnsweredData(const boost::system::error_code& error,
				unsigned int bytes_transferred);

		void stop();

		// an operation has timeout
		void checkDeadline();
	};
}

#endif
