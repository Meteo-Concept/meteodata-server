/**
 * @file vantagepro2connector.h
 * @brief Definition of the VantagePro2Connector class
 * @author Laurent Georget
 * @date 2016-10-05
 */
/*
 * Copyright (C) 2016  SAS Météo Concept <contact@meteo-concept.fr>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef VANTAGEPRO2CONNECTOR_H
#define VANTAGEPRO2CONNECTOR_H

#include <iostream>
#include <memory>
#include <array>
#include <vector>
#include <functional>

#include <boost/system/error_code.hpp>
#include <boost/asio.hpp>
#include <boost/asio/basic_waitable_timer.hpp>

#include <syslog.h>
#include <unistd.h>

#include <cassandra.h>
#include <date/date.h>
#include <date/tz.h>

#include "connector.h"
#include "vantagepro2message.h"
#include "vantagepro2archivepage.h"


namespace meteodata {

namespace ip = boost::asio::ip;
namespace asio = boost::asio;
namespace sys = boost::system; //system() is a function, it cannot be redefined
			       //as a namespace
namespace chrono = std::chrono;

using namespace std::placeholders;
using namespace meteodata;

/**
 * @brief A Connector designed for the VantagePro2 (R) station by Davis
 * Instruments (R)
 */
class VantagePro2Connector : public Connector
{
public:
	/**
	 * @brief Construct a new Connector for a VantagePro2 station
	 *
	 * @param ioService the Boost::Asio service to use for all aynchronous
	 * network operations
	 * @param db The handle to the database
	 */
	VantagePro2Connector(boost::asio::io_service& ioService, DbConnection& db);

	//main loop
	virtual void start() override;

private:
	/**
	 * @brief Represent a state in the state machine
	 */
	enum class State {
		STARTING,                       /*!< Initial state                                                      */
		WAITING_NEXT_MEASURE_TICK,      /*!< Waiting for the timer to hit the deadline for the next measurement */
		SENDING_WAKE_UP_STATION,        /*!< Waiting for the wake up request to be sent                         */
		WAITING_ECHO_STATION,           /*!< Waiting for the station to answer the wake up request              */
		SENDING_REQ_STATION,            /*!< Waiting for the identification request to be sent                  */
		WAITING_ACK_STATION,            /*!< Waiting for the identification request to be acknowledgement       */
		WAITING_DATA_STATION,           /*!< Waiting for the station to answer the identification               */
		SENDING_REQ_TIMEZONE,           /*!< Waiting for the timezone information request to be sent            */
		WAITING_ACK_TIMEZONE,           /*!< Waiting for the station to acknowledge the timezone information request */
		WAITING_DATA_TIMEZONE,          /*!< Waiting for the station to answer the timezone information request */
		SENDING_WAKE_UP_MEASURE,        /*!< Waiting for the wake up request to be sent (before measurement)    */
		WAITING_ECHO_MEASURE,           /*!< Waiting for the station to answer the wake up request              */
		SENDING_REQ_MEASURE,            /*!< Waiting for a measurement request to be sent                       */
		WAITING_ACK_MEASURE,            /*!< Waiting for the station to acknowledge the measurement request     */
		WAITING_DATA_MEASURE,           /*!< Waiting for the station to answer the measurement request          */
		SENDING_WAKE_UP_ARCHIVE,        /*!< Waiting for the wake up order to be sent to download archive       */
		WAITING_ECHO_ARCHIVE,           /*!< Waiting for the station to answer the wake up request to start downloading archive */
		SENDING_REQ_ARCHIVE,            /*!< Waiting for the archive request to be sent                         */
		WAITING_ACK_ARCHIVE,            /*!< Waiting for the archive request acknowledgement                    */
		SENDING_ARCHIVE_PARAMS,         /*!< Waiting for the archive download parameters to be sent             */
		WAITING_ACK_ARCHIVE_PARAMS,     /*!< Waiting for the archive download parameters acknowledgement        */
		WAITING_ARCHIVE_NB_PAGES,       /*!< Waiting for the archive size                                       */
		SENDING_ACK_ARCHIVE_DOWNLOAD,   /*!< Waiting for the archive download acknowledgement to be sent        */
		SENDING_ABORT_ARCHIVE_DOWNLOAD, /*!< Waiting for the archive download abortion notice to be sent        */
		WAITING_ARCHIVE_PAGE,           /*!< Waiting for the next page of archive                               */
		SENDING_ARCHIVE_PAGE_ANSWER,    /*!< Waiting for the archive page confirmation to be sent               */
		SENDING_SETTIME,                /*!< Waiting for the time setting request to be sent                    */
		WAITING_ACK_SETTIME,            /*!< Waiting for the station to acknowledge the time setting request    */
		SENDING_SETTIME_PARAMS,         /*!< Waiting for the time setting parameters to be sent                 */
		WAITING_ACK_TIME_SET,           /*!< Waiting for the station to confirm the completion of the time setting */
		STOPPED                         /*!< Final state for cleanup operations                                 */
	};
	/* Events have type sys::error_code */

	/**
	 * @brief The buffer that can be sent to the station to set the clock
	 */
	struct SettimeRequestParams
	{
		uint8_t seconds; /*!< The seconds     */
		uint8_t minutes; /*!< The minutes     */
		uint8_t hours;   /*!< The hour        */
		uint8_t day;     /*!< The day         */
		uint8_t month;   /*!< The month       */
		uint8_t year;    /*!< The year - 2000 */
		uint16_t crc;    /*!< The CRC of the above fields */
	} __attribute((packed));

	/**
	 * @brief The buffer that can be sent to the station to download archived data
	 */
	struct ArchiveRequestParams
	{
		/**
		 * @brief The date of an existing archive entry used to
		 * indicate the start of the download in the station's archive
		 *
		 * This field is built as | year - 2000 (7 bits) | month (counting from 1) (4 bits) | day of month (5 bits) |
		 **/
		uint16_t date;
		/**
		 * @brief The time of an existing archive entry used to
		 * indicate the start of the download in the station's archive
		 *
		 * This field is built as hours * 100 + minutes
		 **/
		uint16_t time;
		uint16_t crc; /*!< The CRC of the above fields */
	} __attribute((packed));

	/**
	 * @brief Check an event and handle generic errors such as timeouts and
	 * I/O errors
	 *
	 * This method can be called upon receiving an event from the station,
	 * it does nothing in case of a success or a cancelation event. It calls
	 * the restart function in case of a timeout or an I/O error if the
	 * maximum number of (respectively) timeouts and transmission errors has
	 * not reached a predefined threshold and aborts the connection in any
	 * other case.
	 *
	 * @tparam Restarter the type of the function to call to retry the
	 * operation
	 * @param e the event from the station
	 * @param restartState the state in which the state machine must jump
	 * before calling the restart function
	 * @param restart a function to call to retry the current operation
	 * (which is about to fail)
	 */
	template <typename Restarter>
	void handleGenericErrors(const sys::error_code& e, State restartState,
		Restarter restart);

	/**
	 * @brief Transition function of the state machine
	 *
	 * Thi smethod receives an event from the station and reacts according
	 * to the current state before waiting for the next event.
	 *
	 * @param e an event from the station (success, timeout, I/O error,
	 * etc.)
	 */
	void handleEvent(const sys::error_code& e);

	/**
	 * @brief Verify that the timer that has just expired really means that
	 * the associated deadline has expired
	 *
	 * This method is associated with \a _timer .
	 *
	 * @param e an error code which is positioned to a specific value if the
	 * timer has been interrupted before the deadline
	 */
	void checkDeadline(const sys::error_code& e);
	/**
	 * @brief Verify that the timer that has just expired really means that
	 * the associated deadline has expired
	 *
	 * This method is associated with \a _setTimeTimer .
	 *
	 * @param e an error code which is positioned to a specific value if the
	 * timer has been interrupted before the deadline
	 */
	void handleSetTimeDeadline(const sys::error_code& e);
	/**
	 * @brief Cease all operations and close the socket
	 *
	 * Calling this method allows for the VantagePro2Connector to be
	 * destroyed and its memory reclaimed because the only permanent shared
	 * pointer to it is owned by the Boost::Asio::io_service which exits
	 * when the socket is closed.
	 */
	void stop();

	/**
	 * @brief Program the timer to send a timeout event when the clock hit
	 * the next multiple of five minutes
	 */
	void waitForNextMeasure();

	/**
	 * @brief Send a message to the station
	 *
	 * @param req the request
	 * @param reqsize the request size
	 */
	void sendRequest(const char* req, int reqsize);

	/**
	 * @brief Send a buffer over to the station
	 *
	 * @tparam PODType The type of the buffer that is to be sent, it must
	 * be a packed structure
	 * @param req A shared pointer onto the buffer
	 * @param reqsize The size in bytes of the buffer
	 */
	template<typename PODType>
	void sendBuffer(const std::shared_ptr<PODType>& req, int reqsize);
	/**
	 * @brief Wait for the station to answer a wake up request
	 */
	void recvWakeUp();
	/**
	 * @brief Wait for the station to acknowledge last request
	 */
	void recvAck();
	/**
	 * @brief Send an acknowledgement (ACK) to the station
	 */
	void sendAck();
	/**
	 * @brief Send a negative acknowledgement (NAK) to the station
	 */
	void sendNak();
	/**
	 * @brief Send a notice to the station that the current request
	 * is cancelled
	 */
	void sendAbort();
	/**
	 * @brief Wait for the station to answer the request
	 *
	 * @tparam MutableBuffer the type of the buffer in which the answer
	 * must be stored
	 * @param buffer the buffer in which the answere must be stored, this
	 * function will not send the success event until the buffer is full
	 */
	template <typename MutableBuffer>
	void recvData(const MutableBuffer& buffer);
	/**
	 * @brief Empty the communication buffer
	 *
	 * @tparam Restarter the type of the function to call to retry the
	 * operation
	 * @param restartState the state in which the state machine must jump
	 * before calling the restart function
	 * @param restart a function to call to retry the current operation
	 * (which is about to fail)
	 */
	template <typename Restarter>
	void flushSocketAndRetry(State restartState, Restarter restart);

	/**
	 * @brief Construct an \a ArchiveRequestParams from the timestamp
	 * of an archive entry retrieved from the station
	 *
	 * After the return of this method, the caller is the only owner
	 * of the \a ArchiveRequestParams returned by this function. The
	 * \a ArchiveRequestParams is returned as a shared pointer so that
	 * it can be shared easily with an asynchronous I/O function.
	 *
	 * @param time The timestamp of an entry which must exist in the
	 * station's archive (otherwise the entire archive will be downloaded)
	 *
	 * @return A shared pointer on a newly built \a ArchiveRequestParams
	 */
	std::shared_ptr<ArchiveRequestParams> buildArchiveRequestParams(const date::sys_seconds& time);
	/**
	 * @brief Construct an \a SettimeRequestParams from the current
	 * server (POSIX) time
	 *
	 * After the return of this method, the caller is the only owner
	 * of the \a SettimeRequestParams returned by this function. The
	 * \a SettimeRequestParams is returned as a shared pointer so that
	 * it can be shared easily with an asynchronous I/O function.
	 *
	 * @return A shared pointer on a newly built \a SettimeRequestParams
	 */
	std::shared_ptr<SettimeRequestParams> buildSettimeParams();

	/**
	 * @brief The current state of the state machine
	 */
	State _currentState;
	/**
	 * @brief A Boost::Asio timer used to time out on network operations and
	 * to wait between two measurements
	 */
	asio::basic_waitable_timer<chrono::steady_clock> _timer;
	/**
	 * @brief A Boost::Asio timer used to periodically trigger a clock
	 * setting operation
	 */
	asio::basic_waitable_timer<chrono::steady_clock> _setTimeTimer;

	/**
	 * @brief A boolean flag indicating that the station clock should
	 * be set when possible
	 */
	bool _setTimeRequested = true;
	/**
	 * @brief A dummy buffer to store acknowledgements and the like from
	 * the station
	 */
	asio::streambuf _discardBuffer;
	/**
	 * @brief A message in which one data point from the station can be
	 * received
	 */
	VantagePro2Message _message;

	/**
	 * @brief A boolean that tells whether the connector has stopped all
	 * communications with the station
	 *
	 * This boolean is necessary because the VantagePro2Connector will not
	 * die immediately when the socket is closed if the _timer is still in
	 * activity for example. If this happens, then the _timer knows thanks
	 * to this boolean that it should not reset itself but rather stop.
	 */
	bool _stopped = false;
	/**
	 * @brief The number of timeouts registered since the last successful
	 * communication
	 *
	 * When this counter hits a given threshold, the VantagePro2Connector
	 * will consider that the connection to the station is lost and exit.
	 */
	int _timeouts = 0;

	/**
	 * @brief The number of transmission errors registered since the last
	 * successful communication
	 *
	 * When this counter hits a given threshold, the VantagePro2Connector
	 * will abort the connection and exit.
	 */
	int _transmissionErrors = 0;
	/**
	 * @brief The connected station's identifier in the database
	 */
	CassUuid _station;
	/**
	 * @brief The station's name
	 */
	std::string _stationName;
	/**
	 * @brief The amount of time between two queries for data to the stations
	 */
	int _pollingPeriod;

	/**
	 * @brief A one-character buffer to receive acknowledgements from the
	 * station
	 *
	 * When receiving a request, the station usually answers with 0x06, this
	 * buffer stores this acknowledgement.
	 */
	char _ackBuffer;
	/**
	 * @brief A buffer to receive the coordinates from the station
	 */
	int16_t _coords[4];
	/**
	 * @brief A buffer to receive the timezone setting of the station
	 */
	TimeOffseter::VantagePro2TimezoneBuffer _timezoneBuffer;

	/**
	 * @brief A type of buffer able to receive the answer of the station to
	 * an archive download request
	 */
	struct ArchiveSizeBuffer
	{
		uint16_t pagesLeft; /*!< The number of pages that will be sent by the station */
		uint16_t index;     /*!< The index (in [0-4]) in the first page of data of the
				      first entry posterior to the timestamp sent as a parameter
				      to the download request */
		uint16_t crc;       /*!< The CRC of the above fields */
	} __attribute__((packed));

	/**
	 * @brief A buffer used to store the answer of the station to an
	 * archive download request
	 */
	ArchiveSizeBuffer _archiveSize;

	/**
	 * @brief An \a ArchivePage which will receive all downloaded archive
	 * entries from the station
	 *
	 * The \a VantagePro2Connector uses only one \a VantagePro2ArchivePage even if
	 * typically several pages are sent during a download. The buffer is recycled
	 * for each page. Before receiving a new page, the connector must call
	 * \ref VantagePro2ArchivePage::storeToMessages to save the data in the
	 * current page before the buffer can be reused.
	 */
	VantagePro2ArchivePage _archivePage;

	/**
	 * @brief The timestamp (in POSIX time) of the last archive entry
	 * retrieved from the station
	 */
	date::sys_seconds _lastArchive;
	/**
	 * @brief The timestamp (in POSIX time) of the last data retrieved from
	 * the station
	 */
	date::sys_seconds _lastData;

	/**
	 * @brief The \a TimeOffseter to use to convert timestamps between the
	 * station's time and POSIX time
	 */
	TimeOffseter _timeOffseter;

	/**
	 * @brief An echo request, typically used for the wake up procedure
	 */
	static constexpr char _echoRequest[] = "\n";
	/**
	 * @brief An identification request, querying the coordinates of the
	 * station
	 */
	static constexpr char _getStationRequest[] = "EEBRD 0B 06\n";
	/**
	 * @brief A measurement request, querying one data point
	 */
	static constexpr char _getMeasureRequest[] = "LPS 3 2\n";
	/**
	 * @brief An archive request, for a range an archived data points
	 */
	static constexpr char _getArchiveRequest[] = "DMPAFT\n";
	/**
	 * @brief A clock setting request
	 */
	static constexpr char _settimeRequest[] = "SETTIME\n";
	/**
	 * @brief A timezone information request
	 */
	static constexpr char _getTimezoneRequest[] = "EEBRD 11 06\n";
	/**
	 * @brief An acknowledgement
	 */
	static constexpr char _ack[] = "\x06";
	/**
	 * @brief A negative acknowledgement
	 */
	static constexpr char _nak[] = "\x21";
	/**
	 * @brief An abort/cancel order
	 */
	static constexpr char _abort[] = "\x1B";
};

}

#endif
