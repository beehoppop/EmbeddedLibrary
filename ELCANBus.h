#ifndef _ELCANBUS_H_
#define _ELCANBUS_H_

// TCCANBus.h

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
	
	CModule_CANBus(
		);

	virtual void
	Update(
		uint32_t	inDeltaTimeUS);

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

	bool
	Ready(
		void);

private:
	
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
	bool	ready;

};

extern CModule_CANBus	gCANBus;

#endif /* _ELCANBUS_H_ */


