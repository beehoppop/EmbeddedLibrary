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

#include <ELCANBus.h>
#include <ELAssert.h>
#include <ELConfig.h>
#include <ELOutput.h>

CModule_CANBus*	gCANBus;
CModule_CANBus	CModule_CANBus::module;

enum
{
	eSysMsg_SerialOutput = 128,
	eSysMsg_SerialCmd = 129,
};

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
	CModule("canb", 0, 0, NULL, 100, 254),
	canBus(500000)
{
	gCANBus = this;
	canBus.begin();

	targetNodeID = 0xFF;

	for(int i = 0; i < eCANBus_MaxSerialCmdStates; ++i)
	{
		serialCmdStates[i].srcNodeID = 0xFF;
		serialCmdStates[i].serialCmdLength = 0;
	}
}

void
CModule_CANBus::Setup(
	void)
{
	gCmd->RegisterCommand("cb_send", this, static_cast<TCmdHandlerMethod>(&CModule_CANBus::SerialCmdSend));
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
	MReturnOnError(inMsgType >= eCANBus_MaxMsgType);
	handlerList[inMsgType].handlerObject = inHandlerObject;
	handlerList[inMsgType].method = inMethod;
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

	msg.id = CANIDFromComponents(gConfig->GetVal(gConfig->nodeIDIndex), inDstNode, inMsgType, inMsgFlags);
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

	va_list	varArgs;
	va_start(varArgs, inMsg);
	char	vabuffer[256];
	vsnprintf(vabuffer, sizeof(vabuffer), inMsg, varArgs);
	va_end(varArgs);

	msg.id = CANIDFromComponents(gConfig->GetVal(gConfig->nodeIDIndex), inDstNode, inMsgType, 0);
	msg.ext = 1;
	msg.timeout = 100;

	int	msgLen = 0;
	char*	cp = vabuffer;

	for(;;)
	{
		char c = *cp++;

		msg.buf[msgLen++] = c;

		if(c == 0)
			break;

		if(msgLen >= (int)sizeof(msg.buf))
		{
			msg.len = sizeof(msg.buf);
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

void
CModule_CANBus::SendString(
	uint8_t		inDstNode,
	uint8_t		inMsgType,
	char const*	inStr)
{
	SendString(inDstNode, inMsgType, inStr, strlen(inStr));
}

void
CModule_CANBus::SendString(
	uint8_t		inDstNode,
	uint8_t		inMsgType,
	char const*	inStr,
	size_t		inBytes)
{
	CAN_message_t	msg;

	msg.id = CANIDFromComponents(gConfig->GetVal(gConfig->nodeIDIndex), inDstNode, inMsgType, 0);
	msg.ext = 1;
	msg.timeout = 100;

	int	msgLen = 0;

	while(inBytes-- > 0)
	{
		char c = *inStr++;

		msg.buf[msgLen++] = c;

		if(msgLen >= (int)sizeof(msg.buf))
		{
			msg.len = sizeof(msg.buf);
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

	if(dstNode != 0xFF && dstNode != gConfig->GetVal(gConfig->nodeIDIndex))
	{
		return;
	}

	if(msgType == eSysMsg_SerialOutput)
	{
		gSerialOut->write((char*)inMsg.buf, inMsg.len);
	}
	else if(msgType == eSysMsg_SerialCmd)
	{
		SSerialCmdState*	targetState = NULL;

		for(int i = 0; i < eCANBus_MaxSerialCmdStates; ++i)
		{
			if(serialCmdStates[i].srcNodeID == srcNode)
			{
				targetState = serialCmdStates + i;
				break;
			}
			else if(targetState == NULL && serialCmdStates[i].srcNodeID == 0xFF)
			{
				targetState = serialCmdStates + i;
			}
		}
		MReturnOnError(targetState == NULL);

		targetState->srcNodeID = srcNode;

		memcpy(targetState->serialCmdBuffer + targetState->serialCmdLength, inMsg.buf, inMsg.len);
		targetState->serialCmdLength += inMsg.len;

		if(targetState->serialCmdBuffer[targetState->serialCmdLength - 1] == 0)
		{
			targetNodeID = srcNode;
			gCmd->ProcessCommand(this, targetState->serialCmdBuffer);
			targetNodeID = 0xFF;

			targetState->srcNodeID = 0xFF;
			targetState->serialCmdLength = 0;
		}
	}
	else
	{
		if(msgType < eCANBus_MaxMsgType && handlerList[msgType].handlerObject != NULL && handlerList[msgType].method != NULL)
		{
			(handlerList[msgType].handlerObject->*handlerList[msgType].method)(srcNode, dstNode, msgType, flags, inMsg.len, inMsg.buf);
		}
	}
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

bool
CModule_CANBus::SerialCmdSend(
	IOutputDirector*	inOutput,
	int					inArgC,
	char const*			inArgV[])
{
	int	dstNodeID = atoi(inArgV[1]);

	char	buffer[256];
	int		bufferLen = 0;

	for(int i = 2; i < inArgC; ++i)
	{
		int	argLen = strlen(inArgV[i]);
		memcpy(buffer + bufferLen, inArgV[i], argLen);
		bufferLen += argLen;
		buffer[bufferLen++] = ' ';
	}
	buffer[bufferLen++] = 0;

	SendString(dstNodeID, eSysMsg_SerialCmd, buffer);

	return true;
}

void
CModule_CANBus::write(
	char const*	inMsg,
	size_t		inBytes)
{
	if(targetNodeID != 0xFF)
	{
		SendString(targetNodeID, eSysMsg_SerialOutput, inMsg, inBytes);
	}
}
