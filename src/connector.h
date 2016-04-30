#ifndef CONNECTOR_H
#define CONNECTOR_H

#include <memory>

#include <boost/asio.hpp>

namespace meteodata
{
	class Connector : public std::enable_shared_from_this<Connector>
	{
	public:
		typedef std::shared_ptr<Connector> ptr;

		template<typename T>
		static
		typename std::enable_if<std::is_base_of<Connector,T>::value,
				Connector::ptr>::type
		create(boost::asio::io_service& ioService)
		{
			return Connector::ptr(new T(std::ref(ioService)));
		}
		virtual ~Connector() = default;
		virtual void start() = 0;
		boost::asio::ip::tcp::socket& socket() { return _sock; }

	protected:
		Connector(boost::asio::io_service& ioService);
		boost::asio::ip::tcp::socket _sock;
		boost::asio::io_service& _ioService;
	};
}

#endif // CONNECTOR_H
