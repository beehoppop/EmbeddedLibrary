
#include <limits.h>

#include <ELModule.h>
#include <ELAssert.h>
#include <ELInternet.h>
#include <ELUtilities.h>
#include <ELRealTime.h>
#include <ELInternetDevice_ESP8266.h>

#if 0
#define MESPDebugMsg(...) DebugMsgLocal(__VA_ARGS__)
#else
#define MESPDebugMsg(...)
#endif

enum
{
	eMaxLinks = 5,

	eMaxCommandLenth = 96,
	eMaxPendingCommands = 8,

	eConnectionTimeoutMS = 10000,
	eTransmitTimeoutMS = 5000,

};

class CModule_ESP8266 : public CModule, public IInternetDevice
{
public:

	struct SChannel
	{
		SChannel(
			)
		{
			linkIndex = -1;
		}

		void
		Initialize(
			bool	inServer)
		{
			inUse = true;
			connectionFailed = false;
			serverPort = inServer;
			sendPending = false;
			isConnected = false;

			linkIndex = -1;
			incomingBufferIndex = 0;
			incomingTotalBytes = 0;
			outgoingTotalBytes = 0;
			lastUseTimeMS = millis();
		}

		void
		ConnectionOpened(
			int		inLinkIndex)
		{
			linkIndex = inLinkIndex;
			isConnected = true;
			lastUseTimeMS = millis();
		}

		void
		ConnectionClosed(
			void)
		{
			if(serverPort)
			{
				inUse = false;
			}
			linkIndex = -1;
			sendPending = false;
			isConnected = false;
			outgoingTotalBytes = 0;
		}

		void
		ConnectionFailed(
			void)
		{
			connectionFailed = true;
			ConnectionClosed();
		}

		uint32_t	lastUseTimeMS;
		char	incomingBuffer[eMaxIncomingPacketSize];
		char	outgoingBuffer[eMaxOutgoingPacketSize];
		size_t	incomingBufferIndex;
		size_t	incomingTotalBytes;
		size_t	outgoingTotalBytes;
		int		linkIndex;
		bool	inUse;
		bool	serverPort;
		bool	connectionFailed;
		bool	sendPending;
		bool	isConnected;
	};

	struct SPendingCommand
	{
		char		command[eMaxCommandLenth];
		uint32_t	commandStartMS;
		uint32_t	timeoutMS;
		SChannel*	targetChannel;
	};

	CModule_ESP8266(
		)
		:
		CModule("8266", 0, 0, NULL, 1000, 2, false)
	{
		serialPort = NULL;
		rstPin = 0;
		chPDPin = 0;
		gpio0Pin = 0;
		gpio2Pin = 0;
		curIPDChannel = NULL;
		ipdParsing = false;
		commandHead = 0;
		commandTail = 0;
		memset(commandQueue, 0, sizeof(commandQueue));
		serialInputBufferLength = 0;
		serverPort = 0;
		memset(channelArray, 0, sizeof(channelArray));
		for(int i = 0; i < eMaxLinks; ++i)
		{
			channelArray[i].linkIndex = -1;
		}
	}

	virtual void
	Setup(
		void)
	{
		MReturnOnError(serialPort == NULL);
		serialPort->begin(115200);

		if(rstPin != 0xFF)
		{
			pinMode(rstPin, OUTPUT);
			delay(10);	// This is a bit of a mystery but 4 hours of experimentation says that the device will not work without this delay right exactly here
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
	ProcessPendingCommand(
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

			MESPDebugMsg("Processing cmd: %s", curCommand->command);

			if(strcmp(curCommand->command, "ready") == 0)
			{
				// Don't issue the ready command - its special for startup
			}
			else if(targetChannel == NULL)
			{
				// Always send out commands not associated with a channel
				MESPDebugMsg("  Sending");
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
					MESPDebugMsg("  Sending Start %s", curCommand->command);
					serialPort->write(curCommand->command);
					serialPort->write("\r\n");
				}
				else
				{
					// no link is available
					MESPDebugMsg("  No link is available");
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
					MESPDebugMsg("  Sending");
					serialPort->write(curCommand->command);
					serialPort->write("\r\n");
				}
				else
				{
					MESPDebugMsg("  Not issuing send data cmd lnk=%d inuse=%d cf=%d", targetChannel->linkIndex, targetChannel->inUse, targetChannel->connectionFailed);
					targetChannel->outgoingTotalBytes = 0;
					++commandTail;
				}
			}
			else if(strstr(curCommand->command, "AT+CIPCLOSE") != NULL)
			{
				// Only close if we have a valid connection
				if(targetChannel->linkIndex >= 0)
				{
					MESPDebugMsg("  Sending");
					serialPort->write(curCommand->command);
					serialPort->write("\r\n");
				}
				else
				{
					MESPDebugMsg("Not closing lnk=%d inuse=%d cf=%d", targetChannel->linkIndex, targetChannel->inUse, targetChannel->connectionFailed);
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
			MESPDebugMsg("Command TIMEOUT \"%s\"", curCommand->command);
			if(targetChannel != NULL)
			{
				targetChannel->connectionFailed = true;
				targetChannel->outgoingTotalBytes = 0;
			}
			++commandTail;
		}
	}

	void
	ProcessSerialInput(
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

		for(size_t i = 0; i < bytesAvailable; ++i)
		{
			char c = tmpBuffer[i];

			if(curIPDChannel != NULL)
			{
				// We are collecting incoming data from the IPD command
				curIPDChannel->incomingBuffer[curIPDChannel->incomingBufferIndex++] = c;
				if(curIPDChannel->incomingBufferIndex >= curIPDChannel->incomingTotalBytes)
				{
					// we have all the ipd data now so stop collecting, hold on to the data until it is collected from the GetData() method
					MESPDebugMsg("Finished ipd lnk=%d bytes=%d", curIPDChannel->linkIndex, curIPDChannel->incomingBufferIndex);
					curIPDChannel->lastUseTimeMS = millis();
					curIPDChannel = NULL;
				}

				continue;
			}

			if(ipdParsing)
			{
				if(c == ':')
				{
					// we are done processing the IPD command
					serialInputBuffer[serialInputBufferLength++] = 0;
					int	curIPDLinkIndex, intPDBBytes;
					sscanf(serialInputBuffer, "%d,%d", &curIPDLinkIndex, &intPDBBytes);
					MESPDebugMsg("Collecting ipd lnk=%d bytes=%d", curIPDLinkIndex, intPDBBytes);
					curIPDChannel = FindChannel(curIPDLinkIndex);
					if(curIPDChannel != NULL)
					{
						curIPDChannel->incomingBufferIndex = 0;
						curIPDChannel->incomingTotalBytes = (size_t)intPDBBytes;
					}
					else
					{
						// We should have received a CONNECT msg from the chip
						MESPDebugMsg("WARNING: IPD lnk=%d not in use", curIPDLinkIndex);
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
					MESPDebugMsg("Sending data lnk=%d bytes=%d", targetChannel->linkIndex, targetChannel->outgoingTotalBytes);
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
	ProcessInputResponse(
		char*	inCmd)
	{
		if(strcmp(inCmd, "ERROR") == 0 || strcmp(inCmd, "SEND FAIL") == 0)
		{
			if(IsCommandPending())
			{
				SPendingCommand*	curCommand = GetCurrentCommand();
				MESPDebugMsg("CMD FAILED \"%s\"", curCommand->command); 

				SChannel*	targetChannel = curCommand->targetChannel;
				if(targetChannel != NULL)
				{
					targetChannel->ConnectionFailed();
				}

				++commandTail;
			}
			else
			{
				MESPDebugMsg("Got ERROR without pending cmd");
			}
		}
		else if(strcmp(inCmd, "ready") == 0)
		{
			// Command completed successfully
			MESPDebugMsg("Got ready");

			if(IsCommandPending())
			{
				++commandTail;
			}
		}
		else if(strcmp(inCmd, "OK") == 0)
		{
			// Command completed successfully
			MESPDebugMsg("Got OK");

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
			MESPDebugMsg("Got SEND OK");

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
		else if(strcmp(inCmd + 1, ",CONNECT") == 0 && isdigit(inCmd[0]))
		{
			int	linkIndex = inCmd[0] - '0';
			SChannel*	targetChannel = FindChannel(linkIndex);
			bool		isServer = targetChannel == NULL;
			MESPDebugMsg("Incoming Connect lnk=%d server=%d", linkIndex, isServer);
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
			MESPDebugMsg("Incoming CONNECT FAIL lnk=%d %s", linkIndex, targetChannel != NULL ? (targetChannel->serverPort ? "server" : "client") : "unknown");
			if(targetChannel != NULL)
			{
				targetChannel->ConnectionFailed();
			}
		}
		else if(strcmp(inCmd + 1, ",CLOSED") == 0 && isdigit(inCmd[0]))
		{
			int	linkIndex = inCmd[0] - '0';
			SChannel*	targetChannel = FindChannel(linkIndex);
			MESPDebugMsg("Incoming Close lnk=%d %s", linkIndex, targetChannel != NULL ? (targetChannel->serverPort ? "server" : "client") : "unknown");
			if(targetChannel)
			{
				targetChannel->ConnectionClosed();
			}
			DumpState();
		}
		else
		{
			MESPDebugMsg("Received: \"%s\"", inCmd); 
		}
	}

	void
	ProcessChannelTimeouts(
		void)
	{
		SChannel*	curChannel = channelArray;
		for(int i = 0; i < eMaxLinks; ++i, ++curChannel)
		{
			if(curChannel->serverPort && curChannel->linkIndex >= 0 && millis() - curChannel->lastUseTimeMS > eConnectionTimeoutMS)
			{
				MESPDebugMsg("chn=%d timedout lnk=%d", i, curChannel->linkIndex);
				curChannel->lastUseTimeMS = millis();	// prevent this from timing out repeatedly
				CloseConnection(i);
				break;
			}
		}
	}

	virtual void
	Update(
		uint32_t	inDeltaTimeUS)
	{
		ProcessPendingCommand();
		ProcessSerialInput();
		ProcessChannelTimeouts();
	}

	virtual void
	ConnectToAP(
		char const*		inSSID,
		char const*		inPassword,
		EWirelessPWEnc	inPasswordEncryption)
	{
		IssueCommand("AT+CWJAP=\"%s\",\"%s\"", NULL, 15, inSSID, inPassword);
	}

	virtual void
	SetIPAddr(
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

	virtual bool
	Server_Open(
		uint16_t	inServerPort)
	{
		MReturnOnError(serverPort != 0, false);

		serverPort = inServerPort;
		IssueCommand("AT+CIPSERVER=1,%d", NULL, 5, inServerPort);
		return true;
	}

	virtual void
	Server_Close(
		uint16_t	inServerPort)
	{
		serverPort = 0;
		IssueCommand("AT+CIPSERVER=0,%d", NULL, 5, inServerPort);
	}

	virtual int
	Client_RequestOpen(
		uint16_t	inRemoteServerPort,	
		char const*	inRemoteServerAddress)
	{
		SChannel*	targetChannel = FindAvailableChannel();

		if(targetChannel == NULL)
		{
			return -1;
		}

		MESPDebugMsg("Client_RequestOpen chn=%d", targetChannel - channelArray);

		targetChannel->Initialize(false);

		IssueCommand("AT+CIPSTART=0,\"TCP\",\"%s\",%d", targetChannel, 5, inRemoteServerAddress, inRemoteServerPort);

		return (int)(targetChannel - channelArray);
	}

	virtual bool
	Client_OpenCompleted(
		int			inOpenRef,
		bool&		outSuccess,
		uint16_t&	outLocalPort)
	{
		outSuccess = false;
		outLocalPort = 0;

		if(inOpenRef < 0 || inOpenRef >= eMaxLinks)
		{
			SystemMsg("Client_OpenCompleted %d is not valid", inOpenRef);
			return true;
		}

		SChannel*	targetChannel = channelArray + inOpenRef;

		if(targetChannel->connectionFailed || !targetChannel->inUse)
		{
			MESPDebugMsg("Client_OpenCompleted failed chn=%d", inOpenRef);
			targetChannel->inUse = false;
			DumpState();

			return true;
		}

		if(targetChannel->isConnected)
		{
			MESPDebugMsg("Client_OpenCompleted success chn=%d", inOpenRef);
			DumpState();

			outLocalPort = inOpenRef;
			outSuccess = true;
			return true;
		}

		return false;
	}

	virtual void
	GetData(
		uint16_t&	outPort,
		uint16_t&	outReplyPort,	
		size_t&		ioBufferSize,		
		char*		outBuffer)
	{
		SChannel*	curChannel = channelArray;

		for(int i = 0; i < eMaxLinks; ++i, ++curChannel)
		{
			//MESPDebugMsg("chn=%d inuse=%d lnk=%d bytes=%d", i, curChannel->inUse, curChannel->linkIndex, curChannel->incomingTotalBytes);
			if(curChannel->inUse && curChannel->incomingTotalBytes > 0 && curChannel->incomingBufferIndex >= curChannel->incomingTotalBytes)
			{
				MESPDebugMsg("Got data chn=%d lnk=%d srp=%d bytes=%d bi=%d", i, curChannel->linkIndex, curChannel->serverPort, curChannel->incomingTotalBytes, curChannel->incomingBufferIndex);

				ioBufferSize = MMin(ioBufferSize, curChannel->incomingTotalBytes);
				memcpy(outBuffer, curChannel->incomingBuffer, ioBufferSize);
				outReplyPort = i;
				curChannel->incomingTotalBytes = 0;
				curChannel->incomingBufferIndex = 0;
				outPort = curChannel->serverPort ? serverPort : i;
				return;
			}
		}

		ioBufferSize = 0;
	}

	virtual bool
	SendData(
		uint16_t	inPort,	
		size_t		inBufferSize,
		char const*	inBuffer,
		bool		inFlush)
	{
		MReturnOnError(inPort >= eMaxLinks, false);

		SChannel*	targetChannel = channelArray + inPort;

		MReturnOnError(!targetChannel->inUse || targetChannel->linkIndex < 0 || targetChannel->connectionFailed, false);

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

	virtual uint32_t
	GetPortState(
		uint16_t	inPort)
	{
		MReturnOnError(inPort >= eMaxLinks, 0);

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

	virtual void
	CloseConnection(
		uint16_t	inPort)
	{
		MESPDebugMsg("CloseConnection chn=%d", inPort);
		MReturnOnError(inPort >= eMaxLinks);

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

	void
	TransmitPendingData(
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
	WaitPendingTransmitComplete(
		SChannel*	inChannel)
	{
		bool		waitingOn = false;
		uint32_t	waitStartTime = millis();

		while(inChannel->sendPending)
		{
			MReturnOnError(inChannel->connectionFailed || inChannel->linkIndex < 0, false);

			if(!waitingOn)
			{
				MESPDebugMsg("Waiting on transmit");
				waitingOn = true;
			}

			// Allow time for the usb serial port
			delay(1);

			// Check serial input to monitor the send command
			Update(0);

			// Check for timeout
			if(millis() - waitStartTime > eTransmitTimeoutMS)
			{
				// we have timed out
				MESPDebugMsg("transmit has timed out");
				inChannel->connectionFailed = true;
				return false;
			}
		}

		if(waitingOn)
		{
			MESPDebugMsg("Done waiting on transmit");
		}

		return true;
	}

	void
	Configure(
		HardwareSerial*	inSerialPort,
		uint8_t	inRstPin,
		uint8_t	inChPDPin,
		uint8_t	inGPIO0,
		uint8_t	inGPIO2)
	{
		serialPort = inSerialPort;
		rstPin = inRstPin;
		chPDPin = inChPDPin;
		gpio0Pin = inGPIO0;
		gpio2Pin = inGPIO2;
	}

	void
	IssueCommand(
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
				MESPDebugMsg("Waiting on command queue");
				waitingOn = true;
			}
			delay(1);
			Update(0);
		}
		if(waitingOn)
		{
			MESPDebugMsg("Done waiting on command queue");
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
	FindHighestAvailableLink(
		void)
	{
		int	usedLinks = 0;
		for(int i = 0; i < eMaxLinks; ++i)
		{
			if(channelArray[i].linkIndex >= 0)
			{
				usedLinks |= 1 << channelArray[i].linkIndex;
			}
		}

		for(int i = eMaxLinks - 1; i >= 0; --i)
		{
			if(!(usedLinks & (1 << i)))
			{
				return i;
			}
		}

		return -1;
	}

	SPendingCommand*
	GetCurrentCommand(
		void)
	{
		return commandQueue + (commandTail % eMaxPendingCommands);
	}

	bool
	IsCommandPending(
		void)
	{
		return commandTail != commandHead;
	}

	SChannel*
	FindAvailableChannel(
		void)
	{
		for(int i = 0; i < eMaxLinks; ++i)
		{
			if(channelArray[i].inUse == false && channelArray[i].linkIndex < 0)
			{
				return channelArray + i;
			}
		}

		return NULL;
	}

	SChannel*
	FindChannel(
		int	inLinkIndex)
	{
		for(int i = 0; i < eMaxLinks; ++i)
		{
			if(channelArray[i].linkIndex == inLinkIndex)
			{
				return channelArray + i;
			}
		}

		return NULL;
	}

	void
	DumpState(
		void)
	{
		MESPDebugMsg("***");

		SChannel*	curChannel = channelArray;
		for(int i = 0; i < eMaxLinks; ++i, ++curChannel)
		{
			MESPDebugMsg("chn=%d lnk=%d inu=%d srp=%d cnf=%d sdp=%d", 
				i,
				curChannel->linkIndex,
				curChannel->inUse,
				curChannel->serverPort,
				curChannel->connectionFailed,
				curChannel->sendPending);
		}

		MESPDebugMsg("***");
	}

	HardwareSerial*	serialPort;
	uint8_t	rstPin;
	uint8_t	chPDPin;
	uint8_t	gpio0Pin;
	uint8_t	gpio2Pin;

	SChannel*	curIPDChannel;
	bool		ipdParsing;

	uint32_t		commandHead;
	uint32_t		commandTail;
	SPendingCommand	commandQueue[eMaxPendingCommands];

	size_t		serialInputBufferLength;
	char		serialInputBuffer[256];

	uint16_t	serverPort;

	SChannel	channelArray[eMaxLinks];
};

IInternetDevice*
GetInternetDevice_ESP8266(
	HardwareSerial*	inSerialPort,
	uint8_t	inRstPin,
	uint8_t	inChPDPin,
	uint8_t	inGPIO0,
	uint8_t	inGPIO2)
{
	static CModule_ESP8266*	esp8266;

	if(esp8266 == NULL)
	{
		esp8266 = new CModule_ESP8266();
		esp8266->Configure(inSerialPort, inRstPin, inChPDPin, inGPIO0, inGPIO2);
		esp8266->SetEnabledState(true);
	}

	return esp8266;
}
