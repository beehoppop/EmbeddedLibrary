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

class CModule_ESP8266 : public CModule, public IInternetDevice, public IRealTimeHandler
{
public:
	
	MModule_Declaration(
		CModule_ESP8266,
		HardwareSerial*	inSerialPort,
		uint8_t			inRstPin,
		uint8_t			inChPDPin = 0xFF,
		uint8_t			inGPIO0 = 0xFF,
		uint8_t			inGPIO2 = 0xFF)

private:

	CModule_ESP8266(
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
	DumpDebugInfo(
		IOutputDirector*	inOutput);

	virtual void
	ConnectToAP(
		char const*		inSSID,
		char const*		inPassword,
		EWirelessPWEnc	inPasswordEncryption);

	virtual void
	SetIPAddr(
		uint32_t	inIPAddr,
		uint32_t	inGatewayAddr,
		uint32_t	inSubnetAddr);

	virtual bool
	Server_Open(
		uint16_t	inServerPort);
	
	virtual void
	Server_Close(
		uint16_t	inServerPort);

	virtual int
	TCPRequestOpen(
		uint16_t	inRemoteServerPort,	
		char const*	inRemoteServerAddress);

	virtual bool
	TCPCheckOpenCompleted(
		int			inOpenRef,
		bool&		outSuccess,
		uint16_t&	outLocalPort);

	virtual void
	TCPGetData(
		uint16_t&	outPort,
		uint16_t&	outReplyPort,	
		size_t&		ioBufferSize,		
		char*		outBuffer);

	virtual bool
	TCPSendData(
		uint16_t	inPort,	
		size_t		inBufferSize,
		char const*	inBuffer,
		bool		inFlush);

	virtual uint32_t
	TCPGetPortState(
		uint16_t	inPort);

	virtual void
	TCPCloseConnection(
		uint16_t	inPort);

	virtual int
	UDPOpenChannel(
		uint16_t	inLocalPort,
		uint16_t	inRemoteServerPort,
		char const*	inRemoteServerAddress);
	
	virtual bool
	UDPChannelReady(
		int			inChannel);
	
	virtual bool
	UDPGetData(
		int			inChannel,
		uint32_t&	outRemoteAddress,
		uint16_t&	outRemotePort,
		size_t&		ioBufferSize,
		char*	outBuffer);

	virtual bool
	UDPSendData(
		int			inChannel,
		size_t		inBufferSize,
		void*		inBuffer,
		char const*	inRemoteAddress,
		uint16_t	inRemotePort);
	
	virtual void
	UDPCloseChannel(
		int			inChannel);

	virtual bool
	ConnectedToInternet(
		void);

	virtual bool
	IsDeviceTotallyFd(
		void);

	virtual void
	ResetDevice(
		void);

	enum
	{
		eMaxCommandLength = 96,
		eMaxPendingCommands = 8,

		eServerConnectionTimeoutMS = 25000,
		eClientOpenTimeoutMS = 20000,
		eIPDTimeout = 5000,
		eSendTimeout = 15000,
		eCommandTimeoutMS = 15000,
		eSimpleCommandTimeoutMS = 5000,
		eCommandPauseTimeMS = 20,

		eChannelCount = 5,

		eChannelState_Unused = 0,			// The channel is not being used 
		eChannelState_Server,				// The channel is a incoming server connection from a remote client
		eChannelState_ClientStart,			// The channel has a start command pending
		eChannelState_ClientConnected,		// The channel is connected to a remote server
		eChannelState_ClosePending,			// A close command has been submitted
		eChannelState_ClientStartFailed,	// The start command failed, this needs to be reported differently then a general channel failure
		eChannelState_Failed,				// The esp8266 reported a connection failed or a timeout while trying to open a connection
	
		eIPDCheck_NotLooking = 0,
		eIPDCheck_LookingRecvOrSendOK,
		eIPDCheck_LookingNum,
		eIPDCheck_LookingBytes,
		eIPDCheck_LookingSendOK,

		eSendState_None = 0,
		eSendState_WaitingOnReady,
		eSendState_WaitingOnRecv,
		eSendState_WaitingOnSendOK,

		eErrorType_None = 0,
		eErrorType_ERROR,
		eErrorType_IPDTimeout,
		eErrorType_SendTimeout,
		eErrorType_SendFail,
		eErrorType_CommandTimeout,
		eErrorType_ConnectFailed
	};

	struct SChannel
	{
		SChannel(
			)
		{
			Reset();
		}

		void
		Reset(
			void)
		{
			remoteAddress = 0;
			remotePort = 0;
			incomingTotalBytes = 0;
			outgoingTotalBytes = 0;
			linkIndex = -1;
			state = eChannelState_Unused;
			sendPending = false;
			lastUseTimeMS = millis();
		}

		void
		ServerConnection(
			int	inLinkIndex)
		{
			Reset();
			linkIndex = inLinkIndex;
			state = eChannelState_Server;
		}

		void
		ClientStart(
			bool	inIsTCPConnection)
		{
			Reset();
			state = eChannelState_ClientStart;
			tcpConnection = inIsTCPConnection;
		}

		void
		ClientConnected(
			int	inLinkIndex)
		{
			Reset();
			linkIndex = inLinkIndex;
			state = eChannelState_ClientConnected;
		}

		char		incomingBuffer[eMaxIncomingPacketSize];
		char		outgoingBuffer[eMaxOutgoingPacketSize];
		uint32_t	remoteAddress;
		uint16_t	remotePort;
		uint16_t	outgoingTotalBytes;
		uint16_t	incomingTotalBytes;
		uint32_t	lastUseTimeMS;			// For server connections, the last time this channel was used, for client the time the start command was issued
		int			linkIndex;				// The esp8266 link number for this channel
		uint8_t		channelIndex;			// Our index in the the channelArray list
		uint8_t		state;					// The state of the channel
		bool		sendPending;			// True if a send is pending
		bool		tcpConnection;			// True if tcp connection, false if UDP
	};

	struct SPendingCommand
	{
		TString<64>	command;
		uint32_t	timeoutMS;
		SChannel*	targetChannel;
	};

	void
	ProcessPendingCommand(
		void);

	void
	ProcessSerialChar(
		char	inChar);

	void
	ProcessSerialInput(
		void);

	void
	ProcessInputResponse(
		void);
	
	void
	ProcessTimeouts(
		void);

	void
	TCPTransmitPendingData(
		SChannel*	inChannel);

	void
	UDPTransmitPendingData(
		SChannel*	inChannel,
		char const*	inRemoteAddress,
		uint16_t	inRemotePort);

	bool
	WaitPendingTransmitComplete(
		SChannel*	inChannel);

	void
	IssueCommand(
		char const*	inCommand,
		SChannel*	inChannel,
		uint32_t	inTimeoutMS,
		...);

	void
	ClearOutPendingCommandsForChannel(
		SChannel*	inChannel);

	void
	ProcessSendOkAck(
		void);
	
	void
	CheckIPDBufferForCommands(
		void);

	void
	ProcessError(
		uint8_t	inError,
		int		inLinkIndex = -1);

	int
	FindHighestAvailableLink(
		void);

	// Returns NULL if no command is pending
	SPendingCommand*
	GetCurrentCommand(
		void);

	SChannel*
	FindAvailableChannel(
		void);

	SChannel*
	FindChannel(
		int	inLinkIndex);

	void
	DumpChannelState(
		SChannel*	inChannel,
		char const*	inMsg,
		bool		inError);

	void
	DumpState(
		char const*			inMsg,
		IOutputDirector*	inOutput = NULL);

	void
	CheckConnectionStatus(
		TRealTimeEventRef	inEventRef,
		void*				inRefCon);
	
	void
	InitiateConnectionAttempt(
		void);

	TRealTimeEventRef	connectionStatusEvent;

	HardwareSerial*	serialPort;
	uint8_t	rstPin;
	uint8_t	chPDPin;
	uint8_t	gpio0Pin;
	uint8_t	gpio2Pin;

	uint16_t		commandHead;
	uint16_t		commandTail;
	SPendingCommand	commandQueue[eMaxPendingCommands];

	TString<128>	serialInputBuffer;

	uint16_t	serverPort;

	SChannel	channelArray[eChannelCount];

	bool		simpleCommandInProcess;

	bool		ipdInProcess;
	int			ipdTotalBytes;
	int			ipdCurByte;
	SChannel*	ipdCurChannel;
	char		ipdBuffer[eMaxIncomingPacketSize + 1];	// This needs to be null terminated for string processing

	uint8_t		sendState;

	uint32_t	lastSerialInputTimeMS;

	char const*	ssid;
	char const*	pw;
	uint32_t	ipAddr;
	uint32_t	subnetAddr;
	uint32_t	gatewayAddr;

	bool	wifiConnected;
	bool	gotIP;
	bool	gotReady;
	bool	gotCR;
	bool	deviceIsHorked;
	bool	attemptingConnection;
};

#endif /* _ELINTERNETDEVICE_ESP8266_H_ */
