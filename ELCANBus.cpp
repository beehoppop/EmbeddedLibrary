#include <ELCANBus.h>
#include <ELAssert.h>
#include <ELConfig.h>

void
CANIDToComponents(
	uint32_t	inID,
	uint8_t&	outSrcNode,
	uint8_t&	outDstNode,
	uint8_t&	outMsgType,
	uint8_t&	outMsgFlags)
{
	outMsgType = (inID >> 24) & 0x1F;
	outDstNode = (inID >> 16) & 0xFF;
	outSrcNode = (inID >> 8) & 0xFF;
	outMsgFlags = (inID >> 0) & 0xFF;
}

uint32_t
CANIDFromComponents(
	uint8_t	inSrcNode,
	uint8_t	inDstNode,
	uint8_t	inMsgType,
	uint8_t	inMsgFlags)
{
	return ((uint32_t)inMsgType << 24) | ((uint32_t)inDstNode << 16) | ((uint32_t)inSrcNode << 8) | ((uint32_t)inMsgFlags << 0);
}

CModule_CANBus::CModule_CANBus(
	)
	:
	CModule("canb", 0, 0, 100, 254),
	canBus(500000)
{
	canBus.begin();
	ready = true;
}

void
CModule_CANBus::Update(
	uint32_t	inDeltaTimeUS)
{
	CAN_message_t	msg;

	while(canBus.available())
	{
		canBus.read(msg);
		ProcessCANMsg(msg);
	}
}

void
CModule_CANBus::RegisterMsgHandler(
	uint8_t				inMsgType,
	ICANBusMsgHandler*	inHandlerObject,
	TCANBusMsgHandler	inMethod)
{

}

void
CModule_CANBus::SendMsg(
	uint8_t		inDstNode,
	uint8_t		inMsgType,
	uint8_t		inMsgFlags,
	uint8_t		inMsgSize,
	void const*	inMsgData)
{
	CAN_message_t	msg;

	//DebugMsg(eDbgLevel_Basic, "CAN: %02x TXM src=0x%x dst=0x%x typ=0x%x flg=0x%x\n", gConfig->GetVal(eConfigVar_NodeID), gConfig->GetVal(eConfigVar_NodeID), inDstNode, inMsgType, inMsgFlags);

	MAssert(inMsgSize <= eCANBus_MaxMsgLength);

	msg.id = CANIDFromComponents(gConfig->GetVal(eConfigVar_NodeID), inDstNode, inMsgType, inMsgFlags);
	msg.ext = 1;
	msg.timeout = 100;
	msg.len = inMsgSize;
	if(inMsgData != NULL && inMsgSize > 0)
		memcpy(msg.buf, inMsgData, inMsgSize);
	
	//DumpMsg(msg);

	canBus.write(msg);
}

void
CModule_CANBus::SendFormatMsg(
	uint8_t		inDstNode,
	uint8_t		inMsgType,
	char const*	inMsg,
	...)
{
	CAN_message_t	msg;

	msg.id = CANIDFromComponents(gConfig->GetVal(eConfigVar_NodeID), inDstNode, inMsgType, 0);
	msg.ext = 1;
	msg.timeout = 100;

	int	msgLen = 0;

	for(;;)
	{
		char c = *inMsg++;

		if(c == 0)
			break;

		msg.buf[msgLen++] = c;

		if(msgLen >= 8)
		{
			msg.len = 8;
			canBus.write(msg);
			msgLen = 0;
		}
	}

	if(msgLen > 0)
	{
		msg.len = msgLen;
		canBus.write(msg);
	}
}

bool
CModule_CANBus::Ready(
	void)
{
	return ready;
}

void
CModule_CANBus::ProcessCANMsg(
	CAN_message_t const&	inMsg)
{
	uint8_t	srcNode;
	uint8_t	dstNode;
	uint8_t	msgType;
	uint8_t	flags;

	CANIDToComponents(inMsg.id, srcNode, dstNode, msgType, flags);

	//DebugMsg(eDbgLevel_Basic, "CAN: %02x RCV src=0x%x dst=0x%x typ=0x%x flg=0x%x\n", gConfig->GetVal(eConfigVar_NodeID), srcNode, dstNode, msgType, flags);
	//DumpMsg(inMsg);

	if((dstNode != 0xFF && dstNode != gConfig->GetVal(eConfigVar_NodeID)) || msgType >= eCANBus_MaxMsgType || handlerList[msgType].handlerObject == NULL ||  handlerList[msgType].method == NULL)
	{
		return;
	}

	(handlerList[msgType].handlerObject->*handlerList[msgType].method)(srcNode, dstNode, msgType, flags, inMsg.len, inMsg.buf);

}

void
CModule_CANBus::DumpMsg(
	CAN_message_t const&	inMsg)
{
	Serial.printf("id: 0x%08x\n", inMsg.id);
	Serial.printf("ext: 0x%x\n", inMsg.ext);
	Serial.printf("len: 0x%x\n", inMsg.len);
	Serial.printf("timeout: 0x%x\n", inMsg.timeout);
	Serial.printf("data: %02x %02x %02x %02x %02x %02x %02x %02x\n",
		inMsg.buf[0], 
		inMsg.buf[1], 
		inMsg.buf[2], 
		inMsg.buf[3], 
		inMsg.buf[4], 
		inMsg.buf[5], 
		inMsg.buf[6], 
		inMsg.buf[7]
		);
}

CModule_CANBus	gCANBus;
