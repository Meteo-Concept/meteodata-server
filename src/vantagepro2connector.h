#ifndef VANTAGEPRO2CONNECTOR_H
#define VANTAGEPRO2CONNECTOR_H

#include <iostream>
#include <memory>
#include <chrono>

#include <boost/asio.hpp>
#include <boost/asio/system_timer.hpp>

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

	protected:
		void handleWrite(const boost::system::error_code& error,
			size_t bytes_transferred) override;

	private:
		static const int CRC_VALUES[];
		bool validateCrc(const Message& msg);
		void writePeriodically(const boost::system::error_code&);
		std::shared_ptr<VantagePro2Connector> casted_shared_from_this() {
			return std::static_pointer_cast<VantagePro2Connector>(shared_from_this());
		}
		boost::asio::basic_waitable_timer<std::chrono::system_clock> _timer;

		//bool write(std::string& buffer);
		//bool read(std::string& buffer);
	};
}

#endif
