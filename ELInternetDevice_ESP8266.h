#ifndef _ELINTERNETDEVICE_ESP8266_H_
#define _ELINTERNETDEVICE_ESP8266_H_
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
#include <ELInternet.h>

class CModule_ESP8266 : public CModule, public IInternetDevice
{
public:
	
	MModule_Declaration(
		CModule_ESP8266,
		uint8_t			inChannelCount,
		HardwareSerial*	inSerialPort,
		uint8_t			inRstPin,
		uint8_t			inChPDPin = 0xFF,
		uint8_t			inGPIO0 = 0xFF,
		uint8_t			inGPIO2 = 0xFF)

private:

	CModule_ESP8266(
		uint8_t			inChannelCount,
		HardwareSerial*	inSerialPort,
		uint8_t			inRstPin,
		uint8_t			inChPDPin,
		uint8_t			inGPIO0,
		uint8_t			inGPIO2);

	virtual void
	Setup(
		void);

	virtual void
	Update(
		uint32_t	inDeltaTimeUS);

	virtual void
	ConnectToAP(
		char const*		inSSID,
		char const*		inPassword,
		EWirelessPWEnc	inPasswordEncryption);

	virtual void
	SetIPAddr(
		uint32_t	inIPAddr,
		uint32_t	inSubnetAddr,
		uint32_t	inGatewayAddr);

	virtual bool
	Server_Open(
		uint16_t	inServerPort);
	
	virtual void
	Server_Close(
		uint16_t	inServerPort);

	virtual int
	Client_RequestOpen(
		uint16_t	inRemoteServerPort,	
		char const*	inRemoteServerAddress);

	virtual bool
	Client_OpenCompleted(
		int			inOpenRef,
		bool&		outSuccess,
		uint16_t&	outLocalPort);

	virtual void
	GetData(
		uint16_t&	outPort,
		uint16_t&	outReplyPort,	
		size_t&		ioBufferSize,		
		char*		outBuffer);

	virtual bool
	SendData(
		uint16_t	inPort,	
		size_t		inBufferSize,
		char const*	inBuffer,
		bool		inFlush);

	virtual uint32_t
	GetPortState(
		uint16_t	inPort);

	virtual void
	CloseConnection(
		uint16_t	inPort);

		
	enum
	{
		eMaxCommandLenth = 96,
		eMaxPendingCommands = 8,

		eConnectionTimeoutMS = 10000,
		eTransmitTimeoutMS = 5000,
		eIPDTimeout = 2000,
	};

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
			incomingBufferIndex = 0;
			incomingTotalBytes = 0;
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

	void
	ProcessPendingCommand(
		void);

	void
	ProcessSerialInput(
		void);

	void
	ProcessInputResponse(
		char*	inCmd);
	
	void
	ProcessChannelTimeouts(
		void);

	void
	TransmitPendingData(
		SChannel*	inChannel);

	bool
	WaitPendingTransmitComplete(
		SChannel*	inChannel);

	void
	IssueCommand(
		char const*	inCommand,
		SChannel*	inChannel,
		uint32_t	inTimeoutSeconds,
		...);

	int
	FindHighestAvailableLink(
		void);

	SPendingCommand*
	GetCurrentCommand(
		void);

	bool
	IsCommandPending(
		void);

	SChannel*
	FindAvailableChannel(
		void);

	SChannel*
	FindChannel(
		int	inLinkIndex);

	void
	DumpState(
		void);

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

	uint8_t		channelCount;
	SChannel*	channelArray;
};

#endif /* _ELINTERNETDEVICE_ESP8266_H_ */
