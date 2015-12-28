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

	
*/

#include <FlexCAN.h>

#include "ELModule.h"

enum
{
	eCANBus_MaxMsgLength = 8,
	eCANBus_MaxMsgType = 32,
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
	uint8_t		inMsgFlags,
	uint8_t		inMsgSize,
	void const*	inMsgData);

class CModule_CANBus : public CModule
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
		uint8_t		inMsgFlags,
		uint8_t		inMsgSize,
		void const*	inMsgData);

	void
	SendFormatMsg(
		uint8_t		inDstNode,
		uint8_t		inMsgType,
		char const*	inMsg,
		...);

private:
	
	CModule_CANBus(
		);

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

	SMsgHandler	handlerList[eCANBus_MaxMsgType];
	FlexCAN	canBus;

	static CModule_CANBus	module;
};

extern CModule_CANBus*	gCANBus;

#endif /* _ELCANBUS_H_ */


