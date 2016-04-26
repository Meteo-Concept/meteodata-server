#ifndef VANTAGEPRO2CONNECTOR_H
#define VANTAGEPRO2CONNECTOR_H

#include <iostream>
#include <memory>

#include <boost/asio.hpp>
#include <boost/asio/deadline_timer.hpp>
#include <boost/msm/back/state_machine.hpp>
#include <boost/msm/front/state_machine_def.hpp>
#include <boost/msm/front/functor_row.hpp>
#include <boost/msm/front/euml/common.hpp>

#include "connector.h"
#include "message.h"

namespace msm = boost::msm;
namespace mpl = boost::mpl;
namespace ip = boost::asio::ip;
namespace asio = boost::asio;
namespace sys = boost::system; //system() is a function, it cannot be redefined
                               //as a namespace
using namespace msm::front;
using namespace std::placeholders;

namespace {
	//events

	struct timeout {};
	struct error
	{
		const sys::error_code code;
		error(const sys::error_code& e) :
			code(e)
		{}
	};
	struct sent
	{
		const unsigned int bytesTransferred;

		sent(unsigned int bytes = 0) :
			bytesTransferred(bytes)
		{}
	};
	struct recvd
	{
		const unsigned int bytesTransferred;

		recvd(unsigned int bytes = 0) :
			bytesTransferred(bytes)
		{}
	};


	// states
	struct Idle : public msm::front::state<>
	{
		template <class Event,class FSM>
		void on_entry(Event const&, FSM& fsm)
		{
			fsm._txrxErrors = 0;
			fsm._timetouts = 0;
		}
	};

	struct WakingUp : public msm::front::state<>
	{
		template <class Event,class FSM>
		void on_entry(Event const&, FSM& fsm)
		{
			async_write(fsm._sock, "\r\n\r\n",
				[&fsm](const sys::error_code& e,
				       unsigned int bytes) {
					if (e)
						fsm.process_event(error(e));
					else
						fsm.process_event(sent(bytes));
				}
			);
		}
	};

	struct WaitingForAck : public msm::front::state<>
	{
		template <class Event,class FSM>
		void on_entry(Event const&, FSM& fsm)
		{
			async_read_until(fsm._sock, _discardBuffer, '\n',
				[&fsm](const sys::error_code& e,
				       unsigned int bytes) {
					if (e) {
						fsm.process_event(error(e));
					} else {
						fsm.process_event(recvd(bytes));
					}
				});
		}
	};

	struct AskingForData : public msm::front::state<>
	{
		template <class Event,class FSM>
		void on_entry(Event const&, FSM& fsm)
		{
			//flush the socket first
			if (fsm._sock.available() > 0)
				_sock.read_some(_discardBuffer);
			_discardBuffer.consume(_discardBuffer.size());

			//then tell the station we want data
			async_write(fsm._sock, "LPS 3 2\r\n",
				[&fsm](const sys::error_code& e,
				       unsigned int bytes) {
					if (e)
						fsm.process_event(error(e));
					else
						fsm.process_event(sent(bytes));
				}
			);
		}
	};

	struct WaitingForAnswer : public msm::front::state<>
	{
		template <class Event,class FSM>
		void on_entry(Event const&, FSM& fsm)
		{
			async_read(fsm._sock, fsm._inputBuffer,
				[&fsm](const sys::error_code& e,
				       unsigned int bytes) {
					if (e) {
						fsm.process_event(error(e));
					} else {
						fsm.process_event(recvd(bytes));
					}
				});
		}
	};

	struct CheckingCRC : public msm::front::state<>
	{
		bool _valid;
		template <class Event,class FSM>
		void on_entry(Event const&, FSM& fsm)
		{
			_valid = fsm.validateCRC(
						static_cast<const char*>(fsm._l1),
						sizeof(Loop1)) &&
				fsm.validateCRC(
						static_cast<const char*>(fsm._l2),
						sizeof(Loop2));
		}
	};

	struct StoringData : public msm::front::state<>
	{
		//TODO
	};

	struct CleaningUp : public msm::front::state<>
	{
		template <class Event,class FSM>
		void on_entry(Event const&, FSM& fsm)
		{
			fsm.stop();
		}
	};

	// actions
	struct note_error
	{
		template <class Fsm,class Evt,class SourceState,class TargetState>
		void operator()(Evt const&, Fsm& fsm, SourceState&,TargetState& )
		{
			fsm._txrxErrors++;
		}
	};

	struct note_timeout
	{
		template <class Fsm,class Evt,class SourceState,class TargetState>
		void operator()(Evt const&, Fsm& fsm, SourceState&,TargetState& )
		{
			fsm._timeouts++;
		}
	};

	// gates
	struct too_many_errors
	{
		template <class Fsm,class Evt,class SourceState,class TargetState>
		bool operator()(Evt const&, Fsm& fsm, SourceState&,TargetState& )
		{
			return fsm._txrxErrors > 2;
		}
	};

	struct too_many_timeouts
	{
		template <class Fsm,class Evt,class SourceState,class TargetState>
		bool operator()(Evt const&, Fsm& fsm, SourceState&,TargetState& )
		{
			return fsm._timeouts > 2;
		}
	};

	struct stopped
	{
		template <class Fsm,class Evt,class SourceState,class TargetState>
		bool operator()(Evt const&, Fsm& fsm, SourceState&,TargetState& )
		{
			return fsm._stopped;
		}
	};

	struct validCRC
	{
		template <class Fsm,class Evt,class SourceState,class TargetState>
		bool operator()(Evt const&, Fsm&, SourceState& src,TargetState& )
		{
			return src._valid;
		}
	};

	//Transitions
	// none for now, everything is in on_entry() (which may not be fine actually...)

	// Transition table for the server
	struct transition_table : mpl::vector<
		//    Start          Event         Next             Action                      Guard
		//  +--------------+-------------+---------------+---------------------------+-------------------+
		Row < Idle         , timeout     , WakingUp      , none                      , none              >,
		//  +--------------+-------------+---------------+---------------------------+-------------------+
		Row < WakingUp     , sent        , WaitingForAck , none                      , none              >,
		Row < WakingUp     , error       , CleaningUp    , none                      , none              >,
		Row < WakingUp     , timeout     , WakingUp      , note_timeout              , none              >,
		Row < WakingUp     , timeout     , CleaningUp    , none                      , too_many_timeouts >,
		//  +--------------+-------------+---------------+---------------------------+-------------------+
		Row < WakingForAck , recvd       , AskingForData , none                      , none              >,
		Row < WakingForAck , error       , CleaningUp    , none                      , none              >,
		Row < WakingForAck , timeout     , WakingForAck  , note_timeout              , none              >,
		Row < WakingForAck , timeout     , CleaningUp    , none                      , too_many_timeouts >,
		//  +--------------+-------------+---------------+---------------------------+-------------------+
		Row < AskingForData, sent        , WaitingForAck , none                      , none              >,
		Row < AskingForData, error       , CleaningUp    , none                      , none              >,
		Row < AskingForData, timeout     , AskingForData , note_timeout              , none              >,
		Row < AskingForData, timeout     , CleaningUp    , none                      , too_many_timeouts >,
		//  +--------------+-------------+---------------+---------------------------+-------------------+
		Row < WakingForAck , recvd       , CheckingCRC   , none                      , none              >,
		Row < WakingForAck , error       , CleaningUp    , none                      , none              >,
		Row < WakingForAck , timeout     , WakingForAck  , note_timeout              , none              >,
		Row < WakingForAck , timeout     , CleaningUp    , none                      , too_many_timeouts >,
		//  +--------------+-------------+---------------+---------------------------+-------------------+
		Row < CheckingCRC  , none        , StoringData   , none                      , valid             >,
		Row < CheckingCRC  , none        , AskingForData , note_error                , Not_(valid)       >,
		Row < CheckingCRC  , none        , CleaningUp    , none                      , And_(Not_(valid),too_many_errors) >,
		//  +--------------+-------------+---------------+---------------------------+-------------------+
		Row < StoringData  , none        , Idle          , none                      , none              >
		//  +--------------+-------------+---------------+---------------------------+-------------------+
	> {};
}

namespace meteodata {

	struct Message;

	class VantagePro2Connector_ :
		public Connector,
		private msm::front::state_machine_def<EchoServer_>
	{
	public:
		VantagePro2Connector_(boost::asio::io_service& ioService);
		virtual ~VantagePro2Connector_();
		typedef Idle initial_state;

	private:
		static const int CRC_VALUES[];
		bool validateCrc(const char* msg, size_t len);
		void stop();
		std::shared_ptr<VantagePro2Connector> casted_shared_from_this() {
			return std::static_pointer_cast<VantagePro2Connector_>(shared_from_this());
		}
		asio::deadline_timer _timer;

		Loop1 _l1[1]; //it has to be an array to be used as an asio buffer
		Loop2 _l2[1];
		std::array<asio::mutable_buffer, 2> _inputBuffer = {
			asio::buffer(_l1),
			asio::buffer(_l2),
		};
		asio::streambuf _discardBuffer;
		bool _stopped = false;
		unsigned int _txrxErrors = 0;
		unsigned int _timeouts = 0;

	};
}

#endif
