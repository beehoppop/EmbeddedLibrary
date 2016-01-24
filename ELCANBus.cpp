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
	eSysMsg_SerialOutput = eCANBus_MaxMsgType,
	eSysMsg_Command = eCANBus_MaxMsgType + 1,
};

void
CANIDToComponents(
	uint32_t	inID,
	uint8_t&	outSrcNode,
	uint8_t&	outDstNode,
	uint8_t&	outMsgType)
{
	outMsgType = (inID >> 16) & 0xFF;
	outDstNode = (inID >> 8) & 0xFF;
	outSrcNode = (inID >> 0) & 0xFF;
}

uint32_t
CANIDFromComponents(
	uint8_t	inSrcNode,
	uint8_t	inDstNode,
	uint8_t	inMsgType)
{
	return ((uint32_t)inMsgType << 16) | ((uint32_t)inDstNode << 8) | ((uint32_t)inSrcNode << 0);
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
		serialCmdStates[i].msgType = 0xFF;
		serialCmdStates[i].serialCmdLength = 0;
	}
}

void
CModule_CANBus::Setup(
	void)
{
	gCommand->RegisterCommand("cb_send", this, static_cast<TCmdHandlerMethod>(&CModule_CANBus::SendCommand));
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

	while(sendFIFOPendingNext != sendFIFOOutgoingNext)
	{
		if(canBus.write(sendFIFO[sendFIFOOutgoingNext % eCANBus_SendBufferSize]) == 0)
		{
			break;
		}
		++sendFIFOOutgoingNext;
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
	uint8_t		inMsgSize,
	void const*	inMsgData)
{
	CAN_message_t	msg;

	//DebugMsg(eDbgLevel_Basic, "CAN: %02x TXM src=0x%x dst=0x%x typ=0x%x len=%d\n", gConfig->GetVal(gConfig->nodeIDIndex), gConfig->GetVal(gConfig->nodeIDIndex), inDstNode, inMsgType, inMsgSize);

	MReturnOnError(inMsgSize > eCANBus_MaxMsgLength);

	msg.id = CANIDFromComponents(gConfig->GetVal(gConfig->nodeIDIndex), inDstNode, inMsgType);
	msg.ext = 1;
	msg.timeout = 1;	// Specify a short timeout to ensure packets are sent in order
	msg.len = inMsgSize;
	if(inMsgData != NULL && inMsgSize > 0)
		memcpy(msg.buf, inMsgData, inMsgSize);

	sendFIFO[sendFIFOPendingNext++ % eCANBus_SendBufferSize] = msg;
}

void
CModule_CANBus::SendFormatMsg(
	uint8_t		inDstNode,
	uint8_t		inMsgType,
	char const*	inMsg,
	...)
{
	va_list	varArgs;
	va_start(varArgs, inMsg);
	char	vabuffer[256];
	vsnprintf(vabuffer, sizeof(vabuffer), inMsg, varArgs);
	va_end(varArgs);

	SendString(inDstNode, inMsgType, vabuffer, strlen(vabuffer));
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

	msg.id = CANIDFromComponents(gConfig->GetVal(gConfig->nodeIDIndex), inDstNode, inMsgType);
	msg.ext = 1;
	msg.timeout = 1;	// Specify a short timeout to ensure packets are sent in order

	int	msgLen = 0;

	++inBytes;	// Add room for termination byte

	while(inBytes-- > 0)
	{
		if(inBytes > 0)
		{
			msg.buf[msgLen++] = *inStr++;
		}
		else
		{
			msg.buf[msgLen++] = 0;
		}

		if(msgLen >= (int)sizeof(msg.buf))
		{
			msg.len = sizeof(msg.buf);
			sendFIFO[sendFIFOPendingNext++ % eCANBus_SendBufferSize] = msg;
			msgLen = 0;
		}
	}

	if(msgLen > 0)
	{
		msg.len = msgLen;
		sendFIFO[sendFIFOPendingNext++ % eCANBus_SendBufferSize] = msg;
	}
}

void
CModule_CANBus::ProcessCANMsg(
	CAN_message_t const&	inMsg)
{
	uint8_t	srcNode;
	uint8_t	dstNode;
	uint8_t	msgType;

	CANIDToComponents(inMsg.id, srcNode, dstNode, msgType);

	//Serial.printf("CAN: %02x RCV src=0x%x dst=0x%x typ=0x%x\n", gConfig->GetVal(gConfig->nodeIDIndex), srcNode, dstNode, msgType);
	
	if(dstNode != 0xFF && dstNode != gConfig->GetVal(gConfig->nodeIDIndex))
	{
		return;
	}

	if(msgType == eSysMsg_SerialOutput || msgType == eSysMsg_Command)
	{
		SSerialCmdState*	targetState = NULL;

		for(int i = 0; i < eCANBus_MaxSerialCmdStates; ++i)
		{
			if(serialCmdStates[i].srcNodeID == srcNode && serialCmdStates[i].msgType == msgType)
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
		targetState->msgType = msgType;

		MReturnOnError(targetState->serialCmdLength + inMsg.len > sizeof(targetState->serialCmdBuffer));
		memcpy(targetState->serialCmdBuffer + targetState->serialCmdLength, inMsg.buf, inMsg.len);
		targetState->serialCmdLength += inMsg.len;

		if(targetState->serialCmdBuffer[targetState->serialCmdLength - 1] == 0)
		{
			if(msgType == eSysMsg_Command)
			{
				targetNodeID = srcNode;
				gCommand->ProcessCommand(this, targetState->serialCmdBuffer);
				targetNodeID = 0xFF;
			}
			else
			{
				gSerialOut->write(targetState->serialCmdBuffer);
			}

			targetState->srcNodeID = 0xFF;
			targetState->msgType = 0xFF;
			targetState->serialCmdLength = 0;
		}
	}
	else
	{
		if(msgType < eCANBus_MaxMsgType && handlerList[msgType].handlerObject != NULL && handlerList[msgType].method != NULL)
		{
			(handlerList[msgType].handlerObject->*handlerList[msgType].method)(srcNode, dstNode, msgType, inMsg.len, inMsg.buf);
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

uint8_t
CModule_CANBus::SendCommand(
	IOutputDirector*	inOutput,
	int					inArgC,
	char const*			inArgV[])
{
	int	dstNodeID = atoi(inArgV[1]);

	char	buffer[256];
	size_t	bufferLen = 0;

	for(int i = 2; i < inArgC; ++i)
	{
		size_t	argLen = strlen(inArgV[i]);
		MReturnOnError(bufferLen + argLen >= sizeof(buffer), eCmd_Failed);
		memcpy(buffer + bufferLen, inArgV[i], argLen);
		bufferLen += argLen;
		if(i < inArgC - 1)
		{
			MReturnOnError(bufferLen + 1 > sizeof(buffer), eCmd_Failed);
			buffer[bufferLen++] = ' ';
		}
	}
	MReturnOnError(bufferLen + 1 > sizeof(buffer), eCmd_Failed);
	buffer[bufferLen++] = 0;

	if(dstNodeID == gConfig->GetVal(gConfig->nodeIDIndex))
	{
		gCommand->ProcessCommand(inOutput, buffer);
	}
	else
	{
		SendString(dstNodeID, eSysMsg_Command, buffer);
	}

	return eCmd_Pending;
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
