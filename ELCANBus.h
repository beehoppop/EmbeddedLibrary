#ifndef _ELCANBUS_H_
#define _ELCANBUS_H_
/*
	Author: Brent Pease (embeddedlibraryfeedback@gmail.com)

	The MIT License (MIT)

	Copyright (c) 2015-FOREVER Brent Pease

	Permission is hereby granted, free of charge, to any person obtaining a copy
	of this software and associated documentation files (the "Software"), to deal
	in the Software without restriction, including without limitation the rights
	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
	copies of the Software, and to permit persons to whom the Software is
	furnished to do so, subject to the following conditions:

	The above copyright notice and this permission notice shall be included in all
	copies or substantial portions of the Software.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
	SOFTWARE.
*/

/*
	ABOUT

	Send and receive CAN bus messages
*/

#if defined(WIN32)

	#include <Arduino.h>

	struct CAN_message_t
	{
		uint32_t id; // can identifier
		uint8_t ext; // identifier is extended
		uint8_t len; // length of data
		uint16_t timeout; // milliseconds, zero will disable waiting
		uint8_t buf[8];
	};

	class FlexCAN
	{
	public:
		
		FlexCAN(
			uint32_t baud /* = 125000 */)
		{
		}

		void
		begin(
			void)
		{
		}

		bool
		available(
			void)
		{
			return false;
		}

		int
		read(
			CAN_message_t&	outMsg)
		{
			return 0;
		}

		int
		write(
			CAN_message_t const&	outMsg)
		{
			return 0;
		}
	};
#else
	#include <FlexCAN.h>
#endif

#include "ELModule.h"
#include "ELCommand.h"
#include "ELAssert.h"

enum
{
	eCANBus_MaxMsgLength = 8,
	eCANBus_MaxMsgType = 32,
	eCANBus_MaxSerialCmdStates = 4,
	eCANBus_SendBufferSize = 16,
};

class ICANBusMsgHandler
{
public:
};

typedef void
(ICANBusMsgHandler::*TCANBusMsgHandler)(
	uint8_t		inSrcNodeID,
	uint8_t		inDstNodeID,
	uint8_t		inMsgType,
	uint8_t		inMsgSize,
	void const*	inMsgData);

class CModule_CANBus : public CModule, public ICmdHandler, public IOutputDirector
{
public:

	void
	RegisterMsgHandler(
		uint8_t				inMsgType,
		ICANBusMsgHandler*	inHandlerObject,
		TCANBusMsgHandler	inMethod);

	void
	SendMsg(
		uint8_t		inDstNode,
		uint8_t		inMsgType,
		uint8_t		inMsgSize,
		void const*	inMsgData);

	void
	SendFormatMsg(
		uint8_t		inDstNode,
		uint8_t		inMsgType,
		char const*	inMsg,
		...);

	void
	SendString(
		uint8_t		inDstNode,
		uint8_t		inMsgType,
		char const*	inStr);

	void
	SendString(
		uint8_t		inDstNode,
		uint8_t		inMsgType,
		char const*	inStr,
		size_t		inBytes);

private:
	
	CModule_CANBus(
		);

	virtual void
	Setup(
		void);

	virtual void
	Update(
		uint32_t	inDeltaTimeUS);
	
	struct SMsgHandler
	{
		ICANBusMsgHandler*	handlerObject;
		TCANBusMsgHandler	method;
	};

	void
	ProcessCANMsg(
		CAN_message_t const&	inMsg);

	void
	DumpMsg(
		CAN_message_t const&	inMsg);

	bool
	SendCommand(
		IOutputDirector*	inOutput,
		int					inArgC,
		char const*			inArgV[]);

	virtual void
	write(
		char const*	inMsg,
		size_t		inBytes);

	struct SSerialCmdState
	{
		uint8_t	srcNodeID;
		uint8_t	serialCmdLength;
		char	serialCmdBuffer[128];
	};

	SMsgHandler		handlerList[eCANBus_MaxMsgType];
	FlexCAN			canBus;
	SSerialCmdState	serialCmdStates[eCANBus_MaxSerialCmdStates];

	uint8_t	sendFIFOPendingNext;
	uint8_t	sendFIFOOutgoingNext;

	CAN_message_t	sendFIFO[eCANBus_SendBufferSize];

	uint8_t	targetNodeID;

	static CModule_CANBus	module;
};

extern CModule_CANBus*	gCANBus;

#endif /* _ELCANBUS_H_ */


