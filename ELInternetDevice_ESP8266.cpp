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

#include <limits.h>

#include <ELModule.h>
#include <ELAssert.h>
#include <ELInternet.h>
#include <ELUtilities.h>
#include <ELRealTime.h>
#include <ELInternetDevice_ESP8266.h>

#if 0
#define MESPDebugMsg(inMsg, ...) Serial.printf(inMsg, ## __VA_ARGS__)
#else
#define MESPDebugMsg(inMsg, ...)
#endif

MModuleImplementation_Start(CModule_ESP8266,
	uint8_t			inChannelCount,
	HardwareSerial*	inSerialPort,
	uint8_t			inRstPin,
	uint8_t			inChPDPin,
	uint8_t			inGPIO0,
	uint8_t			inGPIO2)
MModuleImplementation_Finish(CModule_ESP8266, inChannelCount, inSerialPort, inRstPin, inChPDPin, inGPIO0, inGPIO2)

CModule_ESP8266::CModule_ESP8266(
	uint8_t			inChannelCount,
	HardwareSerial*	inSerialPort,
	uint8_t			inRstPin,
	uint8_t			inChPDPin,
	uint8_t			inGPIO0,
	uint8_t			inGPIO2)
	:
	CModule(0, 0, NULL, 1000, true),
	serialPort(inSerialPort),
	rstPin(inRstPin),
	chPDPin(inChPDPin),
	gpio0Pin(inGPIO0),
	gpio2Pin(inGPIO2),
	curIPDChannel(NULL),
	ipdParsing(false),
	commandHead(0),
	commandTail(0),
	serialInputBufferLength(0),
	serverPort(0),
	channelCount(inChannelCount),
	ipdCurLinkIndex(-1),
	ipdTotalBytes(0),
	ipdCurByte(0),
	wifiConnected(false),
	gotIP(false)
{
	channelArray = (SChannel*)malloc(sizeof(SChannel) * channelCount);
	MAssert(channelArray != NULL);

	memset(commandQueue, 0, sizeof(commandQueue));
	memset(channelArray, 0, sizeof(SChannel) * channelCount);
	for(int i = 0; i < channelCount; ++i)
	{
		channelArray[i].linkIndex = -1;
	}
}

void
CModule_ESP8266::Setup(
	void)
{
	MReturnOnError(serialPort == NULL);
	serialPort->begin(115200);

	if(rstPin != 0xFF)
	{
		pinMode(rstPin, OUTPUT);
		delay(10);	// Allow time for reset to settle
		digitalWriteFast(rstPin, 1);
	}

	if(chPDPin != 0xFF)
	{
		pinMode(chPDPin, OUTPUT);
		digitalWriteFast(chPDPin, 1);
	}

	if(gpio0Pin != 0xFF)
	{
		pinMode(gpio0Pin, OUTPUT);
		digitalWriteFast(gpio0Pin, 1);
	}
	if(gpio2Pin != 0xFF)
	{
		pinMode(gpio2Pin, OUTPUT);
		digitalWriteFast(gpio2Pin, 1);
	}

	IssueCommand("ready", NULL, 5);
	IssueCommand("ATE0", NULL, 5);
	IssueCommand("AT+CWMODE=3", NULL, 5);
	IssueCommand("AT+CIPMUX=1", NULL, 5);
}

void
CModule_ESP8266::ProcessPendingCommand(
	void)
{
	if(!IsCommandPending())
	{
		return;	// no command to process
	}

	// There is a pending command

	SPendingCommand*	curCommand = GetCurrentCommand();
	SChannel*			targetChannel = curCommand->targetChannel;

	if(curCommand->commandStartMS == 0)
	{
		// We have a pending command ready to go
		curCommand->commandStartMS = millis();

		MESPDebugMsg("Processing cmd: %s\n", curCommand->command);

		if(strcmp(curCommand->command, "ready") == 0)
		{
			// Don't issue the ready command - its special for startup
		}
		else if(targetChannel == NULL)
		{
			// Always send out commands not associated with a channel
			MESPDebugMsg("  Sending\n");
			serialPort->write(curCommand->command);
			serialPort->write("\r\n");
		}
		else if(strstr(curCommand->command, "AT+CIPSTART") != NULL)
		{
			// Pick an available link
			int	linkIndex = FindHighestAvailableLink();
			if(linkIndex >= 0)
			{
				targetChannel->linkIndex = linkIndex;
				curCommand->command[12] = '0' + linkIndex;
				MESPDebugMsg("  Sending Start %s\n", curCommand->command);
				serialPort->write(curCommand->command);
				serialPort->write("\r\n");
			}
			else
			{
				// no link is available
				MESPDebugMsg("  No link is available\n");
				targetChannel->connectionFailed = true;
				++commandTail;
			}
		}
		else if(strstr(curCommand->command, "AT+CIPSENDEX") != NULL)
		{
			// only send data if the link is in use and has a valid connection
			// When this command is acknowledged and the '>' is received the actual data is sent
			if(targetChannel->linkIndex >= 0 && !targetChannel->connectionFailed)
			{
				MESPDebugMsg("  Sending\n");
				serialPort->write(curCommand->command);
				serialPort->write("\r\n");
			}
			else
			{
				MESPDebugMsg("  Not issuing send data cmd lnk=%d inuse=%d cf=%d\n", targetChannel->linkIndex, targetChannel->inUse, targetChannel->connectionFailed);
				targetChannel->outgoingTotalBytes = 0;
				++commandTail;
			}
		}
		else if(strstr(curCommand->command, "AT+CIPCLOSE") != NULL)
		{
			// Only close if we have a valid connection
			if(targetChannel->linkIndex >= 0)
			{
				MESPDebugMsg("  Sending\n");
				serialPort->write(curCommand->command);
				serialPort->write("\r\n");
			}
			else
			{
				MESPDebugMsg("Not closing lnk=%d inuse=%d cf=%d\n", targetChannel->linkIndex, targetChannel->inUse, targetChannel->connectionFailed);
				++commandTail;
			}
			targetChannel->inUse = false;
		}
		else
		{
			// Unknown command
			MAssert(0);
		}
	}
	else if(millis() - curCommand->commandStartMS >= curCommand->timeoutMS)
	{
		// command has timed out
		MESPDebugMsg("Command TIMEOUT \"%s\"\n", curCommand->command);
		if(strstr(curCommand->command, "AT+CIPCLOSE") != NULL)
		{
			if(targetChannel != NULL)
			{
				targetChannel->ConnectionClosed();
			}
		}
		else if(targetChannel != NULL)
		{
			targetChannel->connectionFailed = true;
			targetChannel->outgoingTotalBytes = 0;
		}
		++commandTail;
	}
}

void
CModule_ESP8266::ProcessSerialInput(
	void)
{
	size_t	bytesAvailable = serialPort->available();
	char	tmpBuffer[256];

	if(bytesAvailable == 0)
	{
		return;	// no input to process
	}

	bytesAvailable = MMin(bytesAvailable, sizeof(tmpBuffer));
	serialPort->readBytes(tmpBuffer, bytesAvailable);

	if(curIPDChannel != NULL)
	{
		curIPDChannel->lastUseTimeMS = millis();
	}

	for(size_t i = 0; i < bytesAvailable; ++i)
	{
		char c = tmpBuffer[i];

		if(ipdTotalBytes > 0)
		{
			// We are collecting incoming data from the IPD command
			if(curIPDChannel != NULL)
			{
				curIPDChannel->incomingBuffer[ipdCurByte++] = c;
			}

			if(ipdCurByte >= ipdTotalBytes)
			{
				// we have all the ipd data now so stop collecting, hold on to the data until it is collected from the GetData() method
				MESPDebugMsg("Finished ipd lnk=%d bytes=%d\n", ipdCurLinkIndex, ipdCurByte);

				if(curIPDChannel != NULL)
				{
					curIPDChannel->incomingTotalBytes = ipdTotalBytes;
					curIPDChannel = NULL;
				}

				ipdCurLinkIndex = -1;
				ipdTotalBytes = 0;
				ipdCurByte = 0;
			}

			continue;
		}

		if(ipdParsing)
		{
			if(c == ':')
			{
				// we are done processing the IPD command
				serialInputBuffer[serialInputBufferLength++] = 0;
				ipdCurByte = 0;
				sscanf(serialInputBuffer, "%d,%d", &ipdCurLinkIndex, &ipdTotalBytes);
				MESPDebugMsg("Collecting ipd lnk=%d bytes=%d\n", ipdCurLinkIndex, ipdTotalBytes);
				curIPDChannel = FindChannel(ipdCurLinkIndex);
				if(curIPDChannel != NULL)
				{
					curIPDChannel->lastUseTimeMS = millis();
				}
				else
				{
					// We should have received a CONNECT msg from the chip
					MESPDebugMsg("WARNING: IPD lnk=%d not in use\n", ipdCurLinkIndex);
				}

				ipdParsing = false;
				serialInputBufferLength = 0;
			}
			else
			{
				serialInputBuffer[serialInputBufferLength++] = c;
			}

			continue;
		}

		if(c == '\n' || c == '\r')
		{
			// Line end, check for a command from the chip
			if(serialInputBufferLength > 0)
			{
				// got a command, process it
				serialInputBuffer[serialInputBufferLength] = 0;
				serialInputBufferLength = 0;

				ProcessInputResponse(serialInputBuffer);
			}

			continue;
		}

		// keep collecting input
		if(serialInputBufferLength < sizeof(serialInputBuffer) - 1)
		{
			serialInputBuffer[serialInputBufferLength++] = c;

			if(serialInputBufferLength == 1 && c == '>' && IsCommandPending())
			{
				// We are ready to send the outgoing buffer
				SPendingCommand*	curCommand = GetCurrentCommand();
				SChannel*			targetChannel = curCommand->targetChannel;
				MAssert(targetChannel != NULL);
				MESPDebugMsg("Sending data lnk=%d bytes=%d\n", targetChannel->linkIndex, targetChannel->outgoingTotalBytes);
				if(targetChannel->linkIndex >= 0)
				{
					serialPort->write((uint8_t*)targetChannel->outgoingBuffer, targetChannel->outgoingTotalBytes);
				}
				else
				{
					// the connection was closed so just send a 0 to terminate the send
					serialPort->write((uint8_t)0);
				}
				// Tail is advanced after SEND OK has been received
				serialInputBufferLength = 0;
				targetChannel->lastUseTimeMS = millis();
			}
			else if(serialInputBufferLength == 5 && strncmp(serialInputBuffer, "+IPD,", 5) == 0)
			{
				// We are receiving an IPD command so start special parsing
				ipdParsing = true;
				serialInputBufferLength = 0;
			}
		}
	}
}

void
CModule_ESP8266::ProcessInputResponse(
	char*	inCmd)
{
	if(strcmp(inCmd, "ERROR") == 0 || strcmp(inCmd, "SEND FAIL") == 0)
	{
		if(IsCommandPending())
		{
			SPendingCommand*	curCommand = GetCurrentCommand();
			MESPDebugMsg("CMD FAILED \"%s\"\n", curCommand->command); 

			SChannel*	targetChannel = curCommand->targetChannel;
			if(targetChannel != NULL)
			{
				targetChannel->ConnectionFailed();
			}

			++commandTail;
		}
		else
		{
			MESPDebugMsg("Got ERROR without pending cmd\n");
		}
	}
	else if(strcmp(inCmd, "ready") == 0)
	{
		// Command completed successfully
		MESPDebugMsg("Got ready\n");

		if(IsCommandPending())
		{
			++commandTail;
		}
	}
	else if(strcmp(inCmd, "OK") == 0)
	{
		// Command completed successfully
		MESPDebugMsg("Got OK\n");

		if(IsCommandPending())
		{
			// Only advance the command if we are not sending data - otherwise we need to wait for the '>' - the tail will be advanced after the SEND OK has been received
			SPendingCommand*	curCommand = GetCurrentCommand();
			if(strncmp(curCommand->command, "AT+CIPSENDEX", 12) != 0)
			{
				++commandTail;
			}
		}
	}
	else if(strcmp(inCmd, "SEND OK") == 0)
	{
		// Command completed successfully
		MESPDebugMsg("Got SEND OK\n");

		if(IsCommandPending())
		{
			SPendingCommand*	curCommand = GetCurrentCommand();
			SChannel*			targetChannel = curCommand->targetChannel;
			MAssert(targetChannel != NULL);
			targetChannel->outgoingTotalBytes = 0;
			targetChannel->sendPending = false;
			++commandTail;
		}
	}
	else if(strstr(inCmd, ",CONNECT") != NULL)
	{
		int	linkIndex = atoi(inCmd);
		SChannel*	targetChannel = FindChannel(linkIndex);
		bool		isServer = targetChannel == NULL;
		MESPDebugMsg("Incoming Connect lnk=%d server=%d\n", linkIndex, isServer);
		if(isServer)
		{
			targetChannel = FindAvailableChannel();
			MReturnOnError(targetChannel == NULL);
			targetChannel->Initialize(true);
		}
		targetChannel->ConnectionOpened(linkIndex);
		DumpState();
	}
	else if(strcmp(inCmd + 1, ",CONNECT FAIL") == 0 && isdigit(inCmd[0]))
	{
		int	linkIndex = inCmd[0] - '0';
		SChannel*	targetChannel = FindChannel(linkIndex);
		MESPDebugMsg("Incoming CONNECT FAIL lnk=%d %s\n", linkIndex, targetChannel != NULL ? (targetChannel->serverPort ? "server" : "client") : "unknown");
		if(targetChannel != NULL)
		{
			targetChannel->ConnectionFailed();
		}
	}
	else if(strcmp(inCmd + 1, ",CLOSED") == 0 && isdigit(inCmd[0]))
	{
		int	linkIndex = inCmd[0] - '0';
		SChannel*	targetChannel = FindChannel(linkIndex);
		MESPDebugMsg("Incoming Close lnk=%d %s\n", linkIndex, targetChannel != NULL ? (targetChannel->serverPort ? "server" : "client") : "unknown");
		if(targetChannel)
		{
			targetChannel->ConnectionClosed();
		}
		DumpState();
	}
	else if(strcmp(inCmd, "WIFI CONNECTED") == 0)
	{
		wifiConnected = true;
	}
	else if(strcmp(inCmd, "WIFI DISCONNECTED") == 0)
	{
		wifiConnected = false;
	}
	else if(strcmp(inCmd, "WIFI GOT IP") == 0)
	{
		gotIP = true;
	}
	else
	{
		MESPDebugMsg("Received: \"%s\"\n", inCmd); 
	}
}

void
CModule_ESP8266::ProcessChannelTimeouts(
	void)
{
	SChannel*	curChannel = channelArray;
	for(int i = 0; i < channelCount; ++i, ++curChannel)
	{
		if(curChannel->serverPort && curChannel->linkIndex >= 0 && millis() - curChannel->lastUseTimeMS > eConnectionTimeoutMS)
		{
			MESPDebugMsg("chn=%d timedout lnk=%d\n", i, curChannel->linkIndex);
			curChannel->lastUseTimeMS = millis();	// prevent this from timing out repeatedly
			CloseConnection(i);
		}
	}

	if(curIPDChannel != NULL)
	{
		if(millis() - curIPDChannel->lastUseTimeMS > eIPDTimeout)
		{
			MESPDebugMsg("ipd timedout lnk=%d, this probably means serial buffer overrun, increase RX_BUFFER_SIZE in serial1.c\n", curChannel->linkIndex);
			curIPDChannel->ConnectionFailed();
			curIPDChannel = NULL;
		}
	}
}

void
CModule_ESP8266::Update(
	uint32_t	inDeltaTimeUS)
{
	ProcessPendingCommand();
	ProcessSerialInput();
	ProcessChannelTimeouts();
}

void
CModule_ESP8266::ConnectToAP(
	char const*		inSSID,
	char const*		inPassword,
	EWirelessPWEnc	inPasswordEncryption)
{
	IssueCommand("AT+CWJAP=\"%s\",\"%s\"", NULL, 15, inSSID, inPassword);
}

void
CModule_ESP8266::SetIPAddr(
	uint32_t	inIPAddr,
	uint32_t	inSubnetAddr,
	uint32_t	inGatewayAddr)
{
	IssueCommand("AT+CIPSTA_CUR=\"%d.%d.%d.%d\",\"%d.%d.%d.%d\",\"%d.%d.%d.%d\"", 
		NULL, 15, 
		(inIPAddr >> 24) & 0xFF,
		(inIPAddr >> 16) & 0xFF,
		(inIPAddr >> 8) & 0xFF,
		(inIPAddr >> 0) & 0xFF,
		(inGatewayAddr >> 24) & 0xFF,
		(inGatewayAddr >> 16) & 0xFF,
		(inGatewayAddr >> 8) & 0xFF,
		(inGatewayAddr >> 0) & 0xFF,
		(inSubnetAddr >> 24) & 0xFF,
		(inSubnetAddr >> 16) & 0xFF,
		(inSubnetAddr >> 8) & 0xFF,
		(inSubnetAddr >> 0) & 0xFF
		);
}

bool
CModule_ESP8266::Server_Open(
	uint16_t	inServerPort)
{
	MReturnOnError(serverPort != 0, false);

	serverPort = inServerPort;
	IssueCommand("AT+CIPSERVER=1,%d", NULL, 5, inServerPort);
	return true;
}

void
CModule_ESP8266::Server_Close(
	uint16_t	inServerPort)
{
	serverPort = 0;
	IssueCommand("AT+CIPSERVER=0,%d", NULL, 5, inServerPort);
}

int
CModule_ESP8266::Client_RequestOpen(
	uint16_t	inRemoteServerPort,	
	char const*	inRemoteServerAddress)
{
	SChannel*	targetChannel = FindAvailableChannel();

	if(targetChannel == NULL)
	{
		return -1;
	}

	MESPDebugMsg("Client_RequestOpen chn=%d\n", targetChannel - channelArray);

	targetChannel->Initialize(false);

	IssueCommand("AT+CIPSTART=0,\"TCP\",\"%s\",%d", targetChannel, 5, inRemoteServerAddress, inRemoteServerPort);

	return (int)(targetChannel - channelArray);
}

bool
CModule_ESP8266::Client_OpenCompleted(
	int			inOpenRef,
	bool&		outSuccess,
	uint16_t&	outLocalPort)
{
	outSuccess = false;
	outLocalPort = 0;

	if(inOpenRef < 0 || inOpenRef >= channelCount)
	{
		SystemMsg("Client_OpenCompleted %d is not valid", inOpenRef);
		return true;
	}

	SChannel*	targetChannel = channelArray + inOpenRef;

	if(targetChannel->connectionFailed || !targetChannel->inUse)
	{
		MESPDebugMsg("Client_OpenCompleted failed chn=%d\n", inOpenRef);
		targetChannel->inUse = false;
		DumpState();

		return true;
	}

	if(targetChannel->isConnected)
	{
		MESPDebugMsg("Client_OpenCompleted success chn=%d\n", inOpenRef);
		DumpState();

		outLocalPort = inOpenRef;
		outSuccess = true;
		return true;
	}

	return false;
}

void
CModule_ESP8266::GetData(
	uint16_t&	outPort,
	uint16_t&	outReplyPort,	
	size_t&		ioBufferSize,		
	char*		outBuffer)
{
	SChannel*	curChannel = channelArray;

	for(int i = 0; i < channelCount; ++i, ++curChannel)
	{
		//MESPDebugMsg("chn=%d inuse=%d lnk=%d bytes=%d", i, curChannel->inUse, curChannel->linkIndex, curChannel->incomingTotalBytes);
		if(curChannel->inUse && curChannel->incomingTotalBytes > 0)
		{
			MESPDebugMsg("Got data chn=%d lnk=%d srp=%d bytes=%d\n", i, curChannel->linkIndex, curChannel->serverPort, curChannel->incomingTotalBytes);

			ioBufferSize = MMin(ioBufferSize, curChannel->incomingTotalBytes);
			memcpy(outBuffer, curChannel->incomingBuffer, ioBufferSize);
			outReplyPort = i;
			curChannel->incomingTotalBytes = 0;
			outPort = curChannel->serverPort ? serverPort : i;
			return;
		}
	}

	ioBufferSize = 0;
}

bool
CModule_ESP8266::SendData(
	uint16_t	inPort,	
	size_t		inBufferSize,
	char const*	inBuffer,
	bool		inFlush)
{
	MReturnOnError(inPort >= channelCount, false);

	SChannel*	targetChannel = channelArray + inPort;

	MReturnOnError(!targetChannel->inUse, false);
	MReturnOnError(targetChannel->linkIndex < 0, false);
	MReturnOnError(targetChannel->connectionFailed, false);

	while(inBufferSize > 0)
	{
		// We need to wait for the last transmit to complete before putting new data in the outgoing buffer
		if(WaitPendingTransmitComplete(targetChannel) == false)
		{
			// The pending transmit failed so bail
			return false;
		}

		// Now it is safe to copy new data into the outgoing buffer
		int	bytesToCopy = MMin(inBufferSize, sizeof(targetChannel->outgoingBuffer) - targetChannel->outgoingTotalBytes);
		memcpy(targetChannel->outgoingBuffer + targetChannel->outgoingTotalBytes, inBuffer, bytesToCopy);
		targetChannel->outgoingTotalBytes += bytesToCopy;
		inBufferSize -= bytesToCopy;
		inBuffer += bytesToCopy;

		// If the buffer is full transmit the pending data
		if(targetChannel->outgoingTotalBytes >= (int)sizeof(targetChannel->outgoingBuffer))
		{
			TransmitPendingData(targetChannel);
		}
	}

	if(inFlush)
	{
		// No need to wait here since we are not adding any new data to the outgoing buffer, just flushing what is there
		TransmitPendingData(targetChannel);
	}

	return true;
}

uint32_t
CModule_ESP8266::GetPortState(
	uint16_t	inPort)
{
	MReturnOnError(inPort >= channelCount, 0);

	SChannel*	targetChannel = channelArray + inPort;
		
	if(!targetChannel->inUse)
	{
		return 0;
	}

	uint32_t	result = 0;

	if(targetChannel->isConnected)
	{
		result |= ePortState_IsOpen;
	}

	if(targetChannel->incomingTotalBytes > 0)
	{
		result |= ePortState_HasIncommingData;
	}

	if(targetChannel->sendPending == false)
	{
		result |= ePortState_CanSendData;
	}

	if(targetChannel->connectionFailed)
	{
		result |= ePortState_Failure;
	}

	return result;
}

void
CModule_ESP8266::CloseConnection(
	uint16_t	inPort)
{
	MESPDebugMsg("CloseConnection chn=%d\n", inPort);
	MReturnOnError(inPort >= channelCount);

	SChannel*	targetChannel = channelArray + inPort;

	// Sometimes the channel can be closed before the client closes it so handle that case
	if(targetChannel->linkIndex >= 0)
	{
		if(targetChannel->inUse)
		{
			TransmitPendingData(targetChannel);
		}

		// inUse will be marked false when this command is processed
		IssueCommand("AT+CIPCLOSE=%d", targetChannel, 5, targetChannel->linkIndex);
	}

	targetChannel->inUse = false;

	DumpState();
}

bool
CModule_ESP8266::ConnectedToInternet(
	void)
{
	return wifiConnected && gotIP;
}

void
CModule_ESP8266::TransmitPendingData(
	SChannel*	inChannel)
{
	if(inChannel->outgoingTotalBytes == 0)
	{
		return;
	}

	MReturnOnError(inChannel->connectionFailed || inChannel->linkIndex < 0);

	inChannel->sendPending = true;
	IssueCommand("AT+CIPSENDEX=%d,%d", inChannel, 5, inChannel->linkIndex, inChannel->outgoingTotalBytes);
}
	
// return true on success, false if there was a channel failure or a timeout
bool
CModule_ESP8266::WaitPendingTransmitComplete(
	SChannel*	inChannel)
{
	bool		waitingOn = false;
	uint32_t	waitStartTime = millis();

	while(inChannel->sendPending)
	{

		MReturnOnError(inChannel->connectionFailed || inChannel->linkIndex < 0, false);

		if(!waitingOn)
		{
			MESPDebugMsg("Waiting on transmit\n");
			waitingOn = true;
		}

		// Allow time for the serial port
		delay(1);

		// Check serial input to monitor the send command
		Update(0);

		// Check for timeout
		if(millis() - waitStartTime > eTransmitTimeoutMS)
		{
			// we have timed out
			MESPDebugMsg("transmit has timed out\n");
			inChannel->connectionFailed = true;
			return false;
		}
	}

	if(waitingOn)
	{
		MESPDebugMsg("Done waiting on transmit\n");
	}

	return true;
}

void
CModule_ESP8266::IssueCommand(
	char const*	inCommand,
	SChannel*	inChannel,
	uint32_t	inTimeoutSeconds,
	...)
{
	MESPDebugMsg("IssueCommand: %s\n", inCommand);

	bool		waitingOn = false;
	while((commandHead - commandTail) >= eMaxPendingCommands)
	{
		if(!waitingOn)
		{
			MESPDebugMsg("Waiting on command queue\n");
			waitingOn = true;
		}
		delay(1);
		Update(0);
	}
	if(waitingOn)
	{
		MESPDebugMsg("Done waiting on command queue\n");
	}

	SPendingCommand*	targetCommand = commandQueue + (commandHead++ % eMaxPendingCommands);

	va_list	varArgs;
	va_start(varArgs, inTimeoutSeconds);
	vsnprintf(targetCommand->command, sizeof(targetCommand->command) - 1, inCommand, varArgs);
	targetCommand->command[sizeof(targetCommand->command) - 1] = 0;	// Ensure a valid string
	va_end(varArgs);

	MESPDebugMsg("Queuing: %s\n", targetCommand->command);
	targetCommand->timeoutMS = inTimeoutSeconds * 1000;
	targetCommand->commandStartMS = 0;
	targetCommand->targetChannel = inChannel;
}

int
CModule_ESP8266::FindHighestAvailableLink(
	void)
{
	int	usedLinks = 0;
	for(int i = 0; i < channelCount; ++i)
	{
		if(channelArray[i].linkIndex >= 0)
		{
			usedLinks |= 1 << channelArray[i].linkIndex;
		}
	}

	for(int i = channelCount - 1; i >= 0; --i)
	{
		if(!(usedLinks & (1 << i)))
		{
			return i;
		}
	}

	return -1;
}

CModule_ESP8266::SPendingCommand*
CModule_ESP8266::GetCurrentCommand(
	void)
{
	return commandQueue + (commandTail % eMaxPendingCommands);
}

bool
CModule_ESP8266::IsCommandPending(
	void)
{
	return commandTail != commandHead;
}

CModule_ESP8266::SChannel*
CModule_ESP8266::FindAvailableChannel(
	void)
{
	for(int i = 0; i < channelCount; ++i)
	{
		if(channelArray[i].inUse == false && channelArray[i].linkIndex < 0)
		{
			return channelArray + i;
		}
	}

	return NULL;
}

CModule_ESP8266::SChannel*
CModule_ESP8266::FindChannel(
	int	inLinkIndex)
{
	for(int i = 0; i < channelCount; ++i)
	{
		if(channelArray[i].linkIndex == inLinkIndex)
		{
			return channelArray + i;
		}
	}

	return NULL;
}

void
CModule_ESP8266::DumpState(
	void)
{
	MESPDebugMsg("***\n");

	SChannel*	curChannel = channelArray;
	for(int i = 0; i < channelCount; ++i, ++curChannel)
	{
		MESPDebugMsg("chn=%d lnk=%d inu=%d srp=%d cnf=%d sdp=%d\n", 
			i,
			curChannel->linkIndex,
			curChannel->inUse,
			curChannel->serverPort,
			curChannel->connectionFailed,
			curChannel->sendPending);
	}

	MESPDebugMsg("***\n");
}
