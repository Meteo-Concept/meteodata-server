#ifndef CONNECTOR_H
#define CONNECTOR_H

#include <iostream>
#include <memory>

#include <boost/asio.hpp>

#include "dbconnection.h"

namespace meteodata
{
	/**
	 * @brief Parent class of all meteo stations connectors
	 */
	class Connector : public std::enable_shared_from_this<Connector>
	{
	public:
		/**
		 * @brief Connector::ptr is the type to use to manipulate a
		 * generic connector
		 */
		typedef std::shared_ptr<Connector> ptr;

		/**
		 * @brief Instantiate a new connector for a given meteo station
		 * type
		 *
		 * Meteo stations connectors should never be instantiated
		 * directly: use this method instead. This lets the connector
		 * be deallocated automatically once it is no longer used.
		 *
		 * @tparam T The actual meteo station connector type, e.g.
		 * VantagePro2Connector for a VantagePro2 (R) station
		 * @param ioService The Boost::Asio asynchronous service that
		 * the connector will have to use for all Boost:Asio operations
		 * @param user The username to connect to the database
		 * @param password The password to connect to the database
		 *
		 * @return A auto-managed shared pointer to the connector
		 */
		template<typename T>
		static
		typename std::enable_if<std::is_base_of<Connector,T>::value,
				Connector::ptr>::type
		create(boost::asio::io_service& ioService, const std::string& user, const std::string& password)
		{
			std::cerr << "new connector" << std::endl;
			return Connector::ptr(new T(std::ref(ioService), std::ref(user), std::ref(password)));
		}

		/**
		 * @brief Destroy the connector
		 */
		virtual ~Connector() = default;
		/**
		 * @brief Connect to the database and start polling the meteo
		 * station periodically for data
		 *
		 * This method should basically be an infinite loop and return
		 * when the connection to the meteo station is lost.
		 */
		virtual void start() = 0;
		/**
		 * @brief Give the TCP socket allocated for the connector to
		 * communicate with the meteo station
		 *
		 * The connector should terminate all operations when it detects
		 * a connectivity loss.
		 *
		 * @return The socket used to communicate with the meteo station
		 */
		boost::asio::ip::tcp::socket& socket() { return _sock; }

	protected:
		/**
		 * @brief Construct a connector
		 *
		 * This method is only callable by actual child classes
		 * connectors.
		 * @param ioService The Boost::Asio service to use for all
		 * Boost::Asio asynchronous operations
		 * @param user The username to connect to the database
		 * @param password The password to connect to the database
		 */
		Connector(boost::asio::io_service& ioService, const std::string& user, const std::string& password);
		/**
		 * @brief The TCP socket used to communicate to the meteo
		 * station
		 */
		boost::asio::ip::tcp::socket _sock;
		/**
		 * @brief The Boost::Asio service to use for asynchronous
		 * operations
		 */
		boost::asio::io_service& _ioService;
		/**
		 * @brief The connection to the database
		 */
		DbConnection _db;
	};
}

#endif // CONNECTOR_H
