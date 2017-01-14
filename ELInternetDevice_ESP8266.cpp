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

#define MESPDebug 1

#if MESPDebug
#define MESPDebugMsg(inMsg, ...) do{if(logDebugData){Serial.printf("[%lu] ", millis()); Serial.printf(inMsg, ## __VA_ARGS__);}}while(0)
#else
#define MESPDebugMsg(inMsg, ...)
#endif

MModuleImplementation_Start(CModule_ESP8266,
	HardwareSerial*	inSerialPort,
	uint8_t			inRstPin,
	uint8_t			inChPDPin,
	uint8_t			inGPIO0,
	uint8_t			inGPIO2)
MModuleImplementation_Finish(CModule_ESP8266, inSerialPort, inRstPin, inChPDPin, inGPIO0, inGPIO2)

CModule_ESP8266::CModule_ESP8266(
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
	commandHead(0),
	commandTail(0),
	serverPort(0),
	simpleCommandInProcess(false),
	ipdInProcess(false),
	ipdTotalBytes(0),
	ipdCurByte(0),
	ipdCurChannel(NULL),
	sendState(eSendState_None),
	lastSerialInputTimeMS(0),
	ssid(NULL),
	pw(NULL),
	ipAddr(0),
	subnetAddr(0),
	gatewayAddr(0),
	wifiConnected(false),
	gotIP(false),
	gotReady(false),
	gotCR(false),
	deviceIsHorked(false),
	attemptingConnection(false)
{
	//logDebugData = true;
	memset(commandQueue, 0, sizeof(commandQueue));
	for(int i = 0; i < eChannelCount; ++i)
	{
		channelArray[i].channelIndex = i;
	}

	MAssert(inSerialPort->availableForWrite() > 1000);	// Ensure that the serial port transmit and receive buffers are at least 1024 bytes each
}

void
CModule_ESP8266::Setup(
	void)
{
	MReturnOnError(serialPort == NULL);
	serialPort->begin(115200);

	connectionStatusEvent = MRealTimeCreateEvent("ESP8266 CnctSts", CModule_ESP8266::CheckConnectionStatus, NULL);
	gRealTime->ScheduleEvent(connectionStatusEvent, 10 * 1000000, false);

	if(rstPin != 0xFF)
	{
		pinMode(rstPin, OUTPUT);
		digitalWriteFast(rstPin, 0);
		delay(100);	// Allow time for reset to settle
		digitalWriteFast(rstPin, 1);
		delay(100);	// Allow time for reset to settle
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

	IssueCommand("ready", NULL, 2 * 60 * 1000);
	IssueCommand("ATE0", NULL, eSimpleCommandTimeoutMS);
	IssueCommand("AT+CWMODE_CUR=1", NULL, eSimpleCommandTimeoutMS);
	IssueCommand("AT+CIPMUX=1", NULL, eSimpleCommandTimeoutMS);

	InitiateConnectionAttempt();

	if(ipAddr != 0)
	{
		IssueCommand("AT+CIPSTA_CUR=\"%d.%d.%d.%d\",\"%d.%d.%d.%d\",\"%d.%d.%d.%d\"", 
			NULL, 
			eCommandTimeoutMS, 
			(ipAddr >> 24) & 0xFF,
			(ipAddr >> 16) & 0xFF,
			(ipAddr >> 8) & 0xFF,
			(ipAddr >> 0) & 0xFF,
			(gatewayAddr >> 24) & 0xFF,
			(gatewayAddr >> 16) & 0xFF,
			(gatewayAddr >> 8) & 0xFF,
			(gatewayAddr >> 0) & 0xFF,
			(subnetAddr >> 24) & 0xFF,
			(subnetAddr >> 16) & 0xFF,
			(subnetAddr >> 8) & 0xFF,
			(subnetAddr >> 0) & 0xFF
			);
	}
}

void
CModule_ESP8266::ProcessPendingCommand(
	void)
{
	SPendingCommand*	curCommand = GetCurrentCommand();

	if(curCommand == NULL 
		|| simpleCommandInProcess 
		|| ipdInProcess 
		|| sendState != eSendState_None 
		|| (millis() - lastSerialInputTimeMS <= eCommandPauseTimeMS)
		|| serialPort->available() > 0)
	{
		// do not process new commands if we are processing a command or we have not paused between commands for enough time or if there is serial port data waiting to be processed
		return;
	}

	// We have a pending command ready to go
	SChannel*	targetChannel = curCommand->targetChannel;

	MESPDebugMsg("Processing Command (%d): %s\n", commandTail, (char*)curCommand->command);

	if(curCommand->command.Contains("AT+CIPSENDEX"))
	{
		// only send data if the channel is in the right state
		if((targetChannel->state == eChannelState_Server || targetChannel->state == eChannelState_ClientConnected || targetChannel->state == eChannelState_ClosePending) && targetChannel->sendPending)
		{
			// When this command is acknowledged and the '>' is received the actual data is sent
			serialPort->write(curCommand->command);
			serialPort->write("\r\n");
			serialPort->flush();
			sendState = eSendState_WaitingOnReady;
		}
		else
		{
			DumpChannelState(targetChannel, "Not issuing send data cmd", true);
			targetChannel->outgoingTotalBytes = 0;
			targetChannel->sendPending = false;
			++commandTail;
		}
	}
	else if(curCommand->command == "ready")
	{
		// Don't issue the ready command - its special for startup
		if(gotReady)
		{
			// We already received the ready msg so just advance command queue
			++commandTail;
		}
		else
		{
			// We have not received the ready yet so wait for it
			simpleCommandInProcess = true;
		}
	}
	else if(targetChannel == NULL)
	{
		// Always send out commands not associated with a channel
		serialPort->write(curCommand->command);
		serialPort->write("\r\n");
		serialPort->flush();
		simpleCommandInProcess = true;
	}
	else if(curCommand->command.Contains("AT+CIPSTART"))
	{
		// Pick an available link
		int	linkIndex = FindHighestAvailableLink();
		if(linkIndex >= 0)
		{
			targetChannel->linkIndex = linkIndex;
			curCommand->command.SetChar(12, '0' + linkIndex);
			MESPDebugMsg("  Submitting Start Command lnk=%d\n", linkIndex);
			serialPort->write(curCommand->command);
			serialPort->write("\r\n");
			serialPort->flush();
			simpleCommandInProcess = true;
		}
		else
		{
			// no link is available
			SystemMsg("ESP8266: ERROR: No link is available\n");
			targetChannel->state = eChannelState_ClientStartFailed;
			++commandTail;
		}
	}
	else if(curCommand->command.Contains("AT+CIPCLOSE"))
	{
		// Only close if we have a valid connection - its possible the esp8266 has already closed the port on us
		if(targetChannel->linkIndex >= 0)
		{
			serialPort->write(curCommand->command);
			serialPort->write("\r\n");
			serialPort->flush();
			simpleCommandInProcess = true;
		}
		else
		{
			DumpChannelState(targetChannel, "Not submitting close cmd", false);
			targetChannel->Reset();
			++commandTail;
		}
	}
	else if(curCommand->command.GetLength() == 0)
	{
		// this command was canceled so skip it
		++commandTail;
	}
	else
	{
		// Unknown command
		SystemMsg("ESP8266: ERROR: Unknown Command %s", curCommand->command);
		++commandTail;
	}
	
	lastSerialInputTimeMS = millis();	// Update this here to avoid premature timeout

	if(targetChannel != NULL)
	{
		targetChannel->lastUseTimeMS = lastSerialInputTimeMS;
	}
}

void
CModule_ESP8266::ProcessSerialChar(
	char	inChar)
{
	if(gotCR && inChar == '\n')
	{
		// skip '\n' after a CR
		gotCR = false;
		return;
	}

	if(ipdInProcess)
	{
		// We are processing an IDP command
		if(ipdTotalBytes > 0)
		{
			// We are collecting incoming data from the IPD command
			if(ipdCurChannel != NULL && ipdCurByte < (int)sizeof(ipdCurChannel->incomingBuffer))
			{
				ipdBuffer[ipdCurByte] = inChar;
			}
			++ipdCurByte;	// increment this here because we still need to terminate the ipd even if we did not meet the above conditions for storing the char

			if(ipdCurByte >= ipdTotalBytes)
			{
				// we have all the ipd data now so stop collecting, hold on to the data until it is collected from the GetData() method

				#if 0
				Serial.printf("+++start+++\n");
				for(size_t i = 0; i < (size_t)ipdTotalBytes; ++i)
				{
					uint8_t c = (uint8_t)ipdBuffer[i];
					if(c >= 32 && c < 0x7F && isprint(c))
					{
						Serial.write(c);
					}
					else
					{
						Serial.printf("\\x%02x", c);
					}
				}
				Serial.printf("\n+++end+++\n");
				#endif

				if(ipdCurChannel != NULL)
				{
					memcpy(ipdCurChannel->incomingBuffer, ipdBuffer, ipdTotalBytes);
					ipdCurChannel->incomingTotalBytes = ipdTotalBytes;
					DumpChannelState(ipdCurChannel, "Finished ipd", false);
					ipdCurChannel = NULL;
				}
				else
				{
					SystemMsg("ESP8266: ERROR: Finished ipd but no channel");
				}

				ipdTotalBytes = 0;
				ipdCurByte = 0;
				ipdInProcess = false;
			}
		}
		else if(inChar == ':')
		{
			// we are done processing the IPD command

			int	ipdCurLinkIndex;
			sscanf((char*)serialInputBuffer, "%d,%d", &ipdCurLinkIndex, &ipdTotalBytes);

			ipdCurChannel = FindChannel(ipdCurLinkIndex);
			if(ipdCurChannel != NULL)
			{
				MESPDebugMsg("Collecting ipd chn=%d lnk=%d totalbytes=%d\n", ipdCurChannel->channelIndex, ipdCurLinkIndex, ipdTotalBytes);
			}
			else
			{
				// We should have received a CONNECT msg from the chip
				SystemMsg("ESP8266: ERROR: IPD lnk=%d not in use\n", ipdCurLinkIndex);
			}

			ipdCurByte = 0;
			serialInputBuffer.Clear();
		}
		else
		{
			// Continue collecting the ipd command chars
			serialInputBuffer.Append(inChar);
		}
	}
	else
	{
		if(sendState == eSendState_WaitingOnReady && inChar == '>')
		{
			// We are ready to send the outgoing buffer
			SPendingCommand*	curCommand = GetCurrentCommand();
			SChannel*			targetChannel = curCommand->targetChannel;
			MAssert(targetChannel != NULL);
			if(targetChannel->linkIndex >= 0)
			{
				DumpChannelState(targetChannel, "Sending data", false);

				#if 0
				Serial.printf("???start???\n");
				Serial.write((uint8_t*)targetChannel->outgoingBuffer, targetChannel->outgoingTotalBytes);
				Serial.printf("\n???end???\n");
				#endif

				serialPort->write((uint8_t*)targetChannel->outgoingBuffer, targetChannel->outgoingTotalBytes);
			}
			else
			{
				DumpChannelState(targetChannel, "Can't send data, no link", true);
				// the connection was closed so just send a 0 to terminate the send
				serialPort->write((uint8_t)0);
			}
			serialPort->flush();

			// Command queue tail is advanced after SEND OK has been received
			sendState = eSendState_WaitingOnRecv;
			serialInputBuffer.Clear();
		}
		// keep collecting input
		else if(inChar == '\r')
		{
			// got a command, process it
			if(serialInputBuffer.GetLength() > 0 && !serialInputBuffer.IsSpace())
			{
				ProcessInputResponse();
			}
			serialInputBuffer.Clear();

			gotCR = true;
		}
		else if(inChar > 0 && isprint(inChar))
		{
			serialInputBuffer.Append(inChar);
			if(serialInputBuffer == "+IPD," && gotReady)
			{
				// We are receiving an IPD command so start special parsing
				ipdInProcess = true;
				ipdTotalBytes = 0;
				ipdCurByte = 0;
				ipdCurChannel = NULL;
				serialInputBuffer.Clear();
			}
		}
	}
}

void
CModule_ESP8266::ProcessSerialInput(
	void)
{
	size_t	bytesAvailable = serialPort->available();
	char	tmpBuffer[1024];

	if(bytesAvailable == 0)
	{
		return;	// no input to process
	}

	bytesAvailable = MMin(bytesAvailable, sizeof(tmpBuffer) - 1);
	serialPort->readBytes(tmpBuffer, bytesAvailable);

	#if 0
	tmpBuffer[bytesAvailable] = 0;
	Serial.printf("***start***\n");
	for(size_t i = 0; i < bytesAvailable; ++i)
	{
		uint8_t c = (uint8_t)tmpBuffer[i];
		if(c >= 32 && c < 0x7F && isprint(c))
		{
			Serial.write(c);
		}
		else
		{
			Serial.printf("\\x%02x", c);
		}
	}
	Serial.printf("\n***end***\n");
	#endif

	for(size_t i = 0; i < bytesAvailable; ++i)
	{
		ProcessSerialChar(tmpBuffer[i]);
	}

	lastSerialInputTimeMS = millis();

	if(ipdCurChannel != NULL)
	{
		ipdCurChannel->lastUseTimeMS = lastSerialInputTimeMS;
		CheckIPDBufferForCommands();
	}

	if(sendState != eSendState_None)
	{
		SPendingCommand*	curCommand = GetCurrentCommand();
		SChannel*			targetChannel = curCommand->targetChannel;
		targetChannel->lastUseTimeMS = lastSerialInputTimeMS;
	}
}

void
CModule_ESP8266::ProcessInputResponse(
	void)
{
	MESPDebugMsg("Received: \"%s\"\n", (char*)serialInputBuffer); 

	if(serialInputBuffer == "ready")
	{
		// got the initial ready message

		gotReady = true;

		if(simpleCommandInProcess)
		{
			++commandTail;
			simpleCommandInProcess = false;
		}
		return;
	}
	
	if(!gotReady)
	{
		// Do not process any input until get have received "ready"
		MESPDebugMsg("Not ready, skipping input\n");
		return;
	}
	
	if(serialInputBuffer == "OK")
	{
		// Command completed successfully
		if(simpleCommandInProcess)
		{
			SPendingCommand*	curCommand = GetCurrentCommand();
			MAssert(curCommand != NULL);

			if(curCommand->command.StartsWith("AT+CWJAP"))
			{
				attemptingConnection = false;
			}

			++commandTail;
			simpleCommandInProcess = false;
		}
	}
	else if(serialInputBuffer.StartsWith("Recv "))
	{
		long	recvSize = strtol((char*)serialInputBuffer + 5, NULL, 10);

		SPendingCommand*	curCommand = GetCurrentCommand();
		SChannel*			targetChannel = curCommand->targetChannel;

		if(sendState == eSendState_None)
		{
			SystemMsg("ESP8266: ERROR: Got Recv with no send pending");
		}
		else if(sendState != eSendState_WaitingOnRecv)
		{
			SystemMsg("ESP8266: ERROR: Got Recv out of order");
		}
		else if(recvSize != targetChannel->outgoingTotalBytes)
		{
			SystemMsg("ESP8266: ERROR: Got wrong recv size got %d expecting %d", recvSize, targetChannel->outgoingTotalBytes);
		}
		sendState = eSendState_WaitingOnSendOK;
	}
	else if(serialInputBuffer == "SEND OK")
	{
		// Send completed successfully
		ProcessSendOkAck();
	}
	else if(serialInputBuffer.Contains(",CONNECT"))
	{
		char*		startStr = strstr((char*)serialInputBuffer, ",CONNECT");
		int			linkIndex = startStr[-1] - '0';
		SChannel*	targetChannel = FindChannel(linkIndex);
		
		if(targetChannel == NULL)
		{
			targetChannel = FindAvailableChannel();
			MReturnOnError(targetChannel == NULL);
			targetChannel->ServerConnection(linkIndex);
			DumpChannelState(targetChannel, "Incoming Server Connect", false);
		}
		else
		{
			DumpChannelState(targetChannel, "Incoming Client Connect", false);
			switch(targetChannel->state)
			{
				case eChannelState_Server:
				case eChannelState_ClientConnected:
					// this is a duplicate connect
					SystemMsg("ESP8266: Ignoring duplicate connect");
					break;

				case eChannelState_ClientStart:
					targetChannel->ClientConnected(linkIndex);
					break;

				default:
					SystemMsg("ESP8266: Invalid channel state on connect state=%d", targetChannel->state);
					break;
			}
		}
		
		#if MESPDebug
		if(logDebugData) DumpState("CONNECT");
		#endif
	}
	else if(serialInputBuffer.Contains(",CLOSED"))
	{
		char*		startStr = strstr((char*)serialInputBuffer, ",CLOSED");
		int			linkIndex = startStr[-1] - '0';
		SChannel*	targetChannel = FindChannel(linkIndex);

		if(targetChannel)
		{
			DumpChannelState(targetChannel, "Received Closed", false);

			if(targetChannel->sendPending)
			{
				SystemMsg("ESP8266: ERROR: Received close while channel send is pending");
				
				// Check to see if this pending send has been submitted
				if(sendState != eSendState_None)
				{
					SPendingCommand*	sendCommand = GetCurrentCommand();
					if(sendCommand->targetChannel == targetChannel)
					{
						SystemMsg("ESP8266: ERROR: Received close while actively sending");
						sendState = eSendState_None;
						++commandTail;
					}
				}
			}

			targetChannel->Reset();
			ClearOutPendingCommandsForChannel(targetChannel);	// Since this channel is now closed clear out any pending sends
		}
		else
		{
			MESPDebugMsg("Received closed with no channel lnk=%d\n", linkIndex);
		}

		#if MESPDebug
		if(logDebugData) DumpState("CLOSED");
		#endif
	}
	else if(serialInputBuffer == "WIFI CONNECTED")
	{
		wifiConnected = true;
	}
	else if(serialInputBuffer == "WIFI DISCONNECT")
	{
		wifiConnected = false;
	}
	else if(serialInputBuffer == "WIFI GOT IP")
	{
		gotIP = true;
	}
	else if(serialInputBuffer.Contains(",CONNECT FAIL"))
	{
		char*	startStr = strstr((char*)serialInputBuffer, ",CONNECT FAIL");
		int		linkIndex = startStr[-1] - '0';
		ProcessError(eErrorType_ConnectFailed, linkIndex);
	}
	else if(serialInputBuffer == "ERROR")
	{
		ProcessError(eErrorType_ERROR);
	}
	else if(serialInputBuffer == "SEND FAIL")
	{
		ProcessError(eErrorType_SendFail);
	}
	else if(serialInputBuffer == "UNLINK")
	{
		// The device has done thermonuclear on us, return the favor
		deviceIsHorked = true;
	}
	else if(serialInputBuffer.GetLength() > 0)
	{
		MESPDebugMsg("  COMMAND NOT RECOGNIZED\n"); 
	}
}

void
CModule_ESP8266::ProcessTimeouts(
	void)
{
	if(ipdInProcess)
	{
		if(millis() - lastSerialInputTimeMS > eIPDTimeout)
		{
			ProcessError(eErrorType_IPDTimeout);
		}
	}
	
	if(sendState != eSendState_None)
	{
		if(millis() - lastSerialInputTimeMS > eSendTimeout)
		{
			ProcessError(eErrorType_SendTimeout);
		}
	}
	
	if(simpleCommandInProcess)
	{
		SPendingCommand*	curCommand = GetCurrentCommand();

		if(curCommand != NULL && (millis() - lastSerialInputTimeMS >= curCommand->timeoutMS))
		{
			// command has timed out
			ProcessError(eErrorType_CommandTimeout);
		}
	}

	// Look for channel timeouts
	SChannel*	curChannel = channelArray;
	for(int i = 0; i < eChannelCount; ++i, ++curChannel)
	{
		if(curChannel->state == eChannelState_Server && !curChannel->sendPending)
		{
			if((millis() - curChannel->lastUseTimeMS) > eServerConnectionTimeoutMS)
			{
				// A server port has timed out so close it
				DumpChannelState(curChannel, "server timeout", true);
				
				if(curChannel->linkIndex >= 0)
				{
					curChannel->state = eChannelState_ClosePending;
					IssueCommand("AT+CIPCLOSE=%d", curChannel, eCommandTimeoutMS, curChannel->linkIndex);
				}
				else
				{
					curChannel->Reset();
				}
			}
		}
	}
}

void
CModule_ESP8266::Update(
	uint32_t	inDeltaTimeUS)
{
	if(deviceIsHorked)
	{
		return;
	}

	ProcessPendingCommand();
	ProcessSerialInput();
	ProcessTimeouts();
}

void
CModule_ESP8266::DumpDebugInfo(
	IOutputDirector*	inOutput)
{
	inOutput->printf("wifiConnected=%d\n", wifiConnected);
	inOutput->printf("gotIP=%d\n", gotIP);
	DumpState("DumpDebugInfo", inOutput);
}

void
CModule_ESP8266::ConnectToAP(
	char const*		inSSID,
	char const*		inPassword,
	EWirelessPWEnc	inPasswordEncryption)
{
	ssid = inSSID;
	pw = inPassword;

	if(deviceIsHorked)
	{
		return;
	}

	InitiateConnectionAttempt();
}

void
CModule_ESP8266::SetIPAddr(
	uint32_t	inIPAddr,
	uint32_t	inGatewayAddr,
	uint32_t	inSubnetAddr)
{
	ipAddr = inIPAddr;
	gatewayAddr = inGatewayAddr;
	subnetAddr = inSubnetAddr;

	if(deviceIsHorked)
	{
		return;
	}

	if(HasBeenSetup())
	{
		IssueCommand("AT+CIPSTA_CUR=\"%d.%d.%d.%d\",\"%d.%d.%d.%d\",\"%d.%d.%d.%d\"", 
			NULL, 
			eCommandTimeoutMS, 
			(ipAddr >> 24) & 0xFF,
			(ipAddr >> 16) & 0xFF,
			(ipAddr >> 8) & 0xFF,
			(ipAddr >> 0) & 0xFF,
			(gatewayAddr >> 24) & 0xFF,
			(gatewayAddr >> 16) & 0xFF,
			(gatewayAddr >> 8) & 0xFF,
			(gatewayAddr >> 0) & 0xFF,
			(subnetAddr >> 24) & 0xFF,
			(subnetAddr >> 16) & 0xFF,
			(subnetAddr >> 8) & 0xFF,
			(subnetAddr >> 0) & 0xFF
			);
	}
}

bool
CModule_ESP8266::Server_Open(
	uint16_t	inServerPort)
{
	if(deviceIsHorked)
	{
		return false;
	}

	MReturnOnError(serverPort != 0, false);

	serverPort = inServerPort;
	IssueCommand("AT+CIPSERVER=1,%d", NULL, eCommandTimeoutMS, inServerPort);
	return true;
}

void
CModule_ESP8266::Server_Close(
	uint16_t	inServerPort)
{
	if(deviceIsHorked)
	{
		return;
	}

	serverPort = 0;
	IssueCommand("AT+CIPSERVER=0,%d", NULL, eCommandTimeoutMS, inServerPort);
}

int
CModule_ESP8266::TCPRequestOpen(
	uint16_t	inRemoteServerPort,	
	char const*	inRemoteServerAddress)
{
	if(deviceIsHorked)
	{
		return -1;
	}

	SChannel*	targetChannel = FindAvailableChannel();

	MReturnOnError(targetChannel == NULL, -1);

	MESPDebugMsg("TCPRequestOpen chn=%d\n", targetChannel->channelIndex);

	targetChannel->ClientStart();

	IssueCommand("AT+CIPSTART=0,\"TCP\",\"%s\",%d", targetChannel, eCommandTimeoutMS, inRemoteServerAddress, inRemoteServerPort);

	return targetChannel->channelIndex;
}

bool
CModule_ESP8266::TCPCheckOpenCompleted(
	int			inOpenRef,
	bool&		outSuccess,
	uint16_t&	outLocalPort)
{
	outSuccess = false;
	outLocalPort = 0;

	if(deviceIsHorked)
	{
		return true;
	}

	if(inOpenRef < 0 || inOpenRef >= eChannelCount)
	{
		SystemMsg("ESP8266: ERROR: Client_OpenCompleted %d is not valid", inOpenRef);
		return true;	// Return true since we are saying the open has completed unsuccessfully
	}

	SChannel*	targetChannel = channelArray + inOpenRef;

	if(targetChannel->state != eChannelState_ClientStart)
	{
		// The state has changed from open start so we have either failed or succeeded

		if(targetChannel->state == eChannelState_ClientConnected)
		{
			MESPDebugMsg("Client_OpenCompleted success chn=%d\n", inOpenRef);
			outLocalPort = inOpenRef;
			outSuccess = true;
		}
		else
		{
			SystemMsg("ESP8266: ERROR: Client_OpenCompleted failed chn=%d\n", inOpenRef);
			if(targetChannel->state != eChannelState_ClosePending)
			{
				// Only reset the channel if we are not closing the channel - if we are closing channel will be reset when close completes
				targetChannel->Reset();
			}
		}

		#if MESPDebug
		if(logDebugData) DumpState("CheckOpenCompleted");
		#endif

		return true;
	}

	return false;
}

void
CModule_ESP8266::TCPGetData(
	uint16_t&	outPort,
	uint16_t&	outReplyPort,	
	size_t&		ioBufferSize,		
	char*		outBuffer)
{
	if(deviceIsHorked)
	{
		return;
	}

	SChannel*	curChannel = channelArray;
	for(int i = 0; i < eChannelCount; ++i, ++curChannel)
	{
		//MESPDebugMsg("chn=%d inuse=%d lnk=%d bytes=%d", i, curChannel->inUse, curChannel->linkIndex, curChannel->incomingTotalBytes);
		if((curChannel->state == eChannelState_ClientConnected || curChannel->state == eChannelState_Server) && curChannel->incomingTotalBytes > 0)
		{
			DumpChannelState(curChannel, "Got Data", false);

			ioBufferSize = MMin(ioBufferSize, curChannel->incomingTotalBytes);
			memcpy(outBuffer, curChannel->incomingBuffer, ioBufferSize);
			outReplyPort = i;
			curChannel->incomingTotalBytes = 0;
			outPort = curChannel->state == eChannelState_Server ? serverPort : i;
			return;
		}
	}

	ioBufferSize = 0;
}

bool
CModule_ESP8266::TCPSendData(
	uint16_t	inPort,	
	size_t		inBufferSize,
	char const*	inBuffer,
	bool		inFlush)
{
	MReturnOnError(inPort >= eChannelCount, false);

	if(deviceIsHorked)
	{
		return false;
	}

	SChannel*	targetChannel = channelArray + inPort;

	MReturnOnError(targetChannel->linkIndex < 0, false);
	MReturnOnError(targetChannel->state != eChannelState_Server && targetChannel->state != eChannelState_ClientConnected, false);

	while(inBufferSize > 0)
	{
		// We need to wait for the last transmit to complete before putting new data in the outgoing buffer
		MReturnOnError(WaitPendingTransmitComplete(targetChannel) == false, false);

		// Now it is safe to copy new data into the outgoing buffer
		size_t	bytesToCopy = MMin(inBufferSize, sizeof(targetChannel->outgoingBuffer) - targetChannel->outgoingTotalBytes);
		memcpy(targetChannel->outgoingBuffer + targetChannel->outgoingTotalBytes, inBuffer, bytesToCopy);
		targetChannel->outgoingTotalBytes += (uint16_t)bytesToCopy;
		inBufferSize -= bytesToCopy;
		inBuffer += bytesToCopy;

		// If the buffer is full then transmit the pending data
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
CModule_ESP8266::TCPGetPortState(
	uint16_t	inPort)
{
	MReturnOnError(inPort >= eChannelCount, 0);

	if(deviceIsHorked)
	{
		return ePortState_Failure;
	}

	SChannel*	targetChannel = channelArray + inPort;
		
	if(targetChannel->state == eChannelState_Unused)
	{
		return 0;
	}

	if(targetChannel->state == eChannelState_Failed)
	{
		return ePortState_Failure;
	}

	uint32_t	result = 0;

	if(targetChannel->state == eChannelState_ClientConnected || targetChannel->state == eChannelState_Server)
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

	return result;
}

void
CModule_ESP8266::TCPCloseConnection(
	uint16_t	inPort)
{
	MESPDebugMsg("CloseConnection chn=%d\n", inPort);
	MReturnOnError(inPort >= eChannelCount);

	SChannel*	targetChannel = channelArray + inPort;

	// Sometimes the channel can be closed before the client closes it so handle that case
	if(targetChannel->linkIndex >= 0)
	{
		if(!targetChannel->sendPending)
		{
			TransmitPendingData(targetChannel);
		}

		targetChannel->state = eChannelState_ClosePending;

		// inUse will be marked false when this command is processed
		IssueCommand("AT+CIPCLOSE=%d", targetChannel, eCommandTimeoutMS, targetChannel->linkIndex);
	}
	else
	{
		targetChannel->Reset();
	}

	#if MESPDebug
	if(logDebugData) DumpState("CloseConnection");
	#endif
}

bool
CModule_ESP8266::UDPGetData(
	uint16_t&	outRemotePort,
	uint16_t&	outLocalPort,
	size_t&		ioBufferSize,
	uint8_t*	outBuffer)
{

	return false;
}

bool
CModule_ESP8266::UDPSendData(
	uint16_t&	outLocalPort,
	uint16_t	inRemotePort,
	size_t		inBufferSize,
	uint8_t*	inBuffer)
{
	
	return false;
}

bool
CModule_ESP8266::ConnectedToInternet(
	void)
{
	return wifiConnected && gotIP;
}

bool
CModule_ESP8266::IsDeviceTotallyFd(
	void)
{
	return deviceIsHorked;
}

void
CModule_ESP8266::ResetDevice(
	void)
{
	commandHead = commandTail = 0;
	serverPort = 0;
	simpleCommandInProcess = false;
	ipdInProcess = false;
	ipdTotalBytes = 0;
	ipdCurByte = 0;
	ipdCurChannel = 0;
	sendState = eSendState_None;
	lastSerialInputTimeMS = 0;
	wifiConnected = false;
	gotIP = false;
	gotReady = false;
	gotCR = false;
	deviceIsHorked = false;
	memset(commandQueue, 0, sizeof(commandQueue));
	for(int i = 0; i < eChannelCount; ++i)
	{
		channelArray[i].Reset();
	}
	CModule_ESP8266::Setup();
}

void
CModule_ESP8266::TransmitPendingData(
	SChannel*	inChannel)
{
	if(inChannel->outgoingTotalBytes == 0)
	{
		return;
	}

	DumpChannelState(inChannel, "Initiating Transmit", false);

	MReturnOnError((inChannel->state != eChannelState_Server && inChannel->state != eChannelState_ClientConnected) || inChannel->linkIndex < 0);

	inChannel->sendPending = true;
	IssueCommand("AT+CIPSENDEX=%d,%d", inChannel, eCommandTimeoutMS, inChannel->linkIndex, inChannel->outgoingTotalBytes);
}
	
// return true on success, false if there was a channel failure or a timeout
bool
CModule_ESP8266::WaitPendingTransmitComplete(
	SChannel*	inChannel)
{
	bool		waitingOn = false;

	while(inChannel->sendPending)
	{
		MAssert(inChannel->state == eChannelState_Server || inChannel->state == eChannelState_ClientConnected);

		if(!waitingOn)
		{
			MESPDebugMsg("Waiting on transmit\n");
			waitingOn = true;
		}

		// Allow time for the serial port
		delay(1);

		// Check serial input to monitor the send command
		Update(0);
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
	uint32_t	inTimeoutMS,
	...)
{
	bool	waitingOn = false;
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
	va_start(varArgs, inTimeoutMS);
	targetCommand->command.SetVA(inCommand, varArgs);
	va_end(varArgs);

	MESPDebugMsg("Queuing Command (%d): %s\n", commandHead - 1, (char*)targetCommand->command);
	targetCommand->timeoutMS = inTimeoutMS;
	targetCommand->targetChannel = inChannel;
}

void
CModule_ESP8266::ClearOutPendingCommandsForChannel(
	SChannel*	inChannel)
{
	uint16_t	startingCommandIndex = commandTail;

	if(simpleCommandInProcess || sendState != eSendState_None)
	{
		// don't cancel a command that is in progress so advance the starting command index
		++startingCommandIndex;
	}

	if(startingCommandIndex == commandHead)
	{
		return;
	}

	for(uint16_t i = startingCommandIndex; i != commandHead; ++i)
	{
		SPendingCommand*	targetCommand = commandQueue + (i % eMaxPendingCommands);

		if(targetCommand->targetChannel == inChannel)
		{
			if(targetCommand->command.Contains("AT+CIPCLOSE") || targetCommand->command.Contains("AT+CIPSENDEX"))
			{
				MESPDebugMsg("Canceling (%d): %s\n", i, (char*)targetCommand->command);
				targetCommand->command.Clear();
			}
		}
	}
}

void
CModule_ESP8266::ProcessSendOkAck(
	void)
{
	if(sendState != eSendState_WaitingOnSendOK)
	{
		SystemMsg("ESP8266: ERROR: Got SEND OK out of order");
	}

	if(sendState != eSendState_None)
	{
		SPendingCommand*	curCommand = GetCurrentCommand();
		MAssert(curCommand != NULL);

		SChannel*	targetChannel = curCommand->targetChannel;

		targetChannel->outgoingTotalBytes = 0;
		targetChannel->sendPending = false;
		sendState = eSendState_None;
		++commandTail;

		targetChannel->lastUseTimeMS = millis();
	}
}
		
void
CModule_ESP8266::CheckIPDBufferForCommands(
	void)
{
	char const*	lowestPtr = NULL;
	char const*	checkPtr;

	ipdBuffer[ipdCurByte] = 0;
	checkPtr = strstr(ipdBuffer, "\r\nRecv ");
	if(checkPtr != NULL && ipdCurByte - (checkPtr - ipdBuffer + 7) > 0 && isdigit(checkPtr[7]) && lowestPtr < checkPtr)
	{
		lowestPtr = checkPtr;
	}

	checkPtr = strstr(ipdBuffer, "\r\nSEND OK\r\n");
	if(checkPtr != NULL && lowestPtr < checkPtr)
	{
		lowestPtr = checkPtr;
	}

	checkPtr = strstr(ipdBuffer, ",CONNECT\r\n");
	if(checkPtr != NULL && checkPtr > ipdBuffer && isdigit(checkPtr[-1]) && lowestPtr < checkPtr)
	{
		lowestPtr = checkPtr - 1;
	}

	checkPtr = strstr(ipdBuffer, ",CLOSED\r\n");
	if(checkPtr != NULL && checkPtr > ipdBuffer && isdigit(checkPtr[-1]) && lowestPtr < checkPtr)
	{
		lowestPtr = checkPtr - 1;
	}

	checkPtr = strstr(ipdBuffer, "\r\nWIFI CONNECTED\r\n");
	if(checkPtr != NULL && lowestPtr < checkPtr)
	{
		lowestPtr = checkPtr;
	}

	checkPtr = strstr(ipdBuffer, "\r\nWIFI DISCONNECT\r\n");
	if(checkPtr != NULL && lowestPtr < checkPtr)
	{
		lowestPtr = checkPtr;
	}

	checkPtr = strstr(ipdBuffer, "\r\nWIFI GOT IP\r\n");
	if(checkPtr != NULL && lowestPtr < checkPtr)
	{
		lowestPtr = checkPtr;
	}

	checkPtr = strstr(ipdBuffer, ",CONNECT FAIL\r\n");
	if(checkPtr != NULL && checkPtr > ipdBuffer && isdigit(checkPtr[-1]) && lowestPtr < checkPtr)
	{
		lowestPtr = checkPtr;
	}

	checkPtr = strstr(ipdBuffer, "\r\nSEND FAIL\r\n");
	if(checkPtr != NULL && lowestPtr < checkPtr)
	{
		lowestPtr = checkPtr;
	}

	checkPtr = strstr(ipdBuffer, "\r\nUNLINK\r\n");
	if(checkPtr != NULL && lowestPtr < checkPtr)
	{
		lowestPtr = checkPtr;
	}

	if(lowestPtr != NULL)
	{
		// We found a command in the ipd input data this means the P.O.S. ESP8266 firmware did not send us all the ipd data it said it would
		// cancel the ipd and feed the chars after back into the serial input processing loop
		size_t	startIndex = lowestPtr - ipdBuffer;
		size_t	endIndex = ipdCurByte;

		ipdInProcess = false;
		ipdTotalBytes = 0;
		ipdCurByte = 0;
		ipdCurChannel = NULL;

		for(size_t	i = startIndex; i < endIndex; ++i)
		{
			ProcessSerialChar(ipdBuffer[i]);
		}
	}

}

void
CModule_ESP8266::ProcessError(
	uint8_t	inError,
	int		inLinkIndex)
{
	static char const* gErrorStrs[] = {"None", "ERROR", "IPDTimeout", "SendTimeout", "SendFail", "CommandTimeout", "ConnectFailed"};	// See eErrorType_*

	SPendingCommand*	curCommand = GetCurrentCommand();
	SChannel*			targetChannel = inLinkIndex >= 0 ? FindChannel(inLinkIndex) : curCommand != NULL ? curCommand->targetChannel : NULL;
	
	if(curCommand != NULL)
	{
		SystemMsg("ESP8266: ERROR: %s cmd=\"%s\"\n", gErrorStrs[inError], (char*)curCommand->command); 
	}

	if(targetChannel != NULL)
	{
		DumpChannelState(targetChannel, gErrorStrs[inError], true);
	}

	DumpState(gErrorStrs[inError]);

	switch(inError)
	{
		case eErrorType_ERROR:
			if(simpleCommandInProcess)
			{
				if(curCommand->command.StartsWith("AT+CWJAP"))
				{
					attemptingConnection = false;
				}

				if(targetChannel != NULL)
				{
					if(targetChannel->state == eChannelState_ClientStart)
					{
						if(targetChannel->linkIndex >= 0)
						{
							targetChannel->state = eChannelState_ClosePending;
							IssueCommand("AT+CIPCLOSE=%d", targetChannel, eCommandTimeoutMS, targetChannel->linkIndex);	// Issue a close here in case this failed because it was already open
						}
						else
						{
							targetChannel->state = eChannelState_Failed;
						}
					}
					else if(targetChannel->state == eChannelState_ClosePending)
					{
						targetChannel->Reset();
					}
				}

				++commandTail;
			}
			else
			{
				SystemMsg("ESP8266: ERROR: Got ERROR without pending cmd\n");
			}
			break;

		case eErrorType_IPDTimeout:
			SystemMsg("ESP8266: ERROR: ipd timedout this probably means serial buffer overrun, increase RX_BUFFER_SIZE in serial1.c\n");
			if(ipdCurChannel != NULL)
			{
				DumpChannelState(ipdCurChannel, "IPDTimeout", true);
			}
			
			if(sendState != eSendState_None)
			{
				// Assume the SEND OK went into the incoming buffer and just move on
				ProcessSendOkAck();
			}

			ipdInProcess = false;
			ipdTotalBytes = 0;
			ipdCurByte = 0;
			ipdCurChannel = NULL;
			break;

		case eErrorType_SendTimeout:
		case eErrorType_SendFail:
			if(sendState == eSendState_None)
			{
				SystemMsg("ESP8266: ERROR: Send failure without pending cmd\n");
			}

			if(targetChannel->sendPending == false)
			{
				SystemMsg("ESP8266: ERROR: Send failure without channel send pending\n");
			}

			if(targetChannel != NULL)
			{
				targetChannel->sendPending = false;
				targetChannel->outgoingTotalBytes = 0;
			}
			else
			{
				SystemMsg("ESP8266: ERROR: Send failure with no send channel\n");
			}
			
			sendState = eSendState_None;
			++commandTail;	// Skip to the next command
			break;

		case eErrorType_CommandTimeout:
			if(simpleCommandInProcess)
			{
				if(curCommand->command.StartsWith("AT+CWJAP"))
				{
					attemptingConnection = false;
				}

				if(targetChannel != NULL)
				{
					if(targetChannel->state == eChannelState_ClientStart)
					{
						targetChannel->state = eChannelState_ClientStartFailed;
					}
					else if(targetChannel->state == eChannelState_ClosePending)
					{
						targetChannel->Reset();
					}
				}

				simpleCommandInProcess = false;
				++commandTail;
			}
			else
			{
				SystemMsg("ESP8266: ERROR: Got command timeout without pending cmd\n");
			}
			break;

		case eErrorType_ConnectFailed:
			if(targetChannel != NULL)
			{
				if(targetChannel->state == eChannelState_Server)
				{
					// This should not happen but handle it just in case
					targetChannel->Reset();
				}
				else if(targetChannel->state == eChannelState_ClientStart)
				{
					targetChannel->state = eChannelState_ClientStartFailed;
				}
				else if(targetChannel->state != eChannelState_Unused)
				{
					// Only set to the failed state if it was being used
					targetChannel->state = eChannelState_Failed;
				}
			}
	}
}

int
CModule_ESP8266::FindHighestAvailableLink(
	void)
{
	int	usedLinks = 0;
	for(int i = 0; i < eChannelCount; ++i)
	{
		if(channelArray[i].state != eChannelState_Unused || channelArray[i].linkIndex >= 0)
		{
			usedLinks |= 1 << channelArray[i].linkIndex;
		}
	}

	for(int i = eChannelCount - 1; i >= 0; --i)
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
	if(commandTail == commandHead)
	{
		return NULL;
	}

	return commandQueue + (commandTail % eMaxPendingCommands);
}

CModule_ESP8266::SChannel*
CModule_ESP8266::FindAvailableChannel(
	void)
{
	for(int i = 0; i < eChannelCount; ++i)
	{
		if(channelArray[i].state == eChannelState_Unused && channelArray[i].linkIndex < 0)
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
	for(int i = 0; i < eChannelCount; ++i)
	{
		if(channelArray[i].linkIndex == inLinkIndex)
		{
			return channelArray + i;
		}
	}

	return NULL;
}

void
CModule_ESP8266::DumpChannelState(
	SChannel*	inChannel,
	char const*	inMsg,
	bool		inError)
{
	if(inError)
	{
		SystemMsg(
			"ESP8266: ERROR: %s (chn=%d lnk=%d state=%d sp=%d otb=%d itb=%d)\n", 
			inMsg,
			inChannel->channelIndex, 
			inChannel->linkIndex, 
			inChannel->state, 
			inChannel->sendPending, 
			inChannel->outgoingTotalBytes, 
			inChannel->incomingTotalBytes);
	}
	else
	{
		MESPDebugMsg(
			"%s (chn=%d lnk=%d state=%d sp=%d otb=%d itb=%d)\n", 
			inMsg,
			inChannel->channelIndex, 
			inChannel->linkIndex, 
			inChannel->state, 
			inChannel->sendPending, 
			inChannel->outgoingTotalBytes, 
			inChannel->incomingTotalBytes);
	}
}

void
CModule_ESP8266::DumpState(
	char const*			inMsg,
	IOutputDirector*	inOutput)
{
	SChannel*	curChannel = channelArray;
	
	MOutputDirectorOrSerial(
		inOutput,
		"%s (simpleCommandInProcess=%d ipdInProcess=%d sendState=%d ipdTotalBytes=%d ipdCurByte=%d)\n",
		inMsg,
		simpleCommandInProcess, 
		ipdInProcess, 
		sendState,
		ipdTotalBytes,
		ipdCurByte);
	
	for(int i = 0; i < eChannelCount; ++i, ++curChannel)
	{
		MOutputDirectorOrSerial(
			inOutput, 
			"    chn=%d lnk=%d state=%d sp=%d otb=%d itb=%d\n", 
			curChannel->channelIndex, 
			curChannel->linkIndex, 
			curChannel->state, 
			curChannel->sendPending, 
			curChannel->outgoingTotalBytes, 
			curChannel->incomingTotalBytes);
	}
}

void
CModule_ESP8266::CheckConnectionStatus(
	TRealTimeEventRef	inEventRef,
	void*				inRefCon)
{
	if(!wifiConnected)
	{
		InitiateConnectionAttempt();
	}
}
	
void
CModule_ESP8266::InitiateConnectionAttempt(
	void)
{
	if(attemptingConnection || !HasBeenSetup())
	{
		return;
	}

	attemptingConnection = true;
	if(ssid != NULL)
	{
		IssueCommand("AT+CWJAP=\"%s\",\"%s\"", NULL, eCommandTimeoutMS, (char*)ssid, (char*)pw);
	}
}

