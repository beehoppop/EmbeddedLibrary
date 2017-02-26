#ifndef _EL_INTERNET_H_
#define _EL_INTERNET_H_
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

	Everything internet related
*/

#include <ELModule.h>
#include <ELOutput.h>
#include <ELCommand.h>
#include <ELString.h>
#include <ELRealTime.h>

enum
{
	eMaxServersCount = 4,
	eMaxConnectionsCount = 4,
	eServerMaxAddressLength = 64,

	eWebServerPageHandlerMax = 16,

	eUDPTimeoutSecs = 10,

	eInvalidPort = 0xFFFF,

	eMaxIncomingPacketSize = 1500,
	eMaxOutgoingPacketSize = 1400,

	eLocalPortBase = 40000,
	eLocalPortCount = 16,

	ePortState_IsOpen			= 1 << 0,
	ePortState_CanSendData		= 1 << 1,
	ePortState_HasIncommingData	= 1 << 2,
	ePortState_Failure			= 1 << 3,
};

enum EWirelessPWEnc
{
	eWirelessPWEnc_Open,
	eWirelessPWEnc_WEP,
	eWirelessPWEnc_WPA2Personal,
};

enum EConnectionResponse
{
	eConnectionResponse_Opened,
	eConnectionResponse_Closed,
	eConnectionResponse_Data,
	eConnectionResponse_Error,
};

// Utility macro for registering a response handler when opening a connection to a server
#define MInternetOpenConnection(inServerPort, inServerAddress, inMethod) gInternetModule->TCPOpenConnection(inServerPort, inServerAddress, this, static_cast<TTCPResponseHandlerMethod>(&inMethod))
#define MInternetRegisterPage(inPage, inMethod) gInternetModule->WebServer_RegisterPageHandler(inPage, this, static_cast<TInternetServerPageMethod>(&inMethod))

class CModule_Internet;
class CHTTPConnection;

class IInternetHandler
{
public:
};

// This will be called when a client connects to the command server
typedef void
(IInternetHandler::*TInternetServerPageMethod)(
	IOutputDirector*	inOutput,
	int					inParamCount,
	char const**		inParamList);

// This will be called when data arrives to a opened server
typedef void
(IInternetHandler::*TInternetServerHandlerMethod)(
	IOutputDirector*	inOutput,
	int					inDataSize,
	char const*			inData);

// This will be called when a client tcp connection receives response data from a server, if this is called with inDataSize of 0 and inDat is NULL that means an error occurred
typedef void
(IInternetHandler::*TTCPResponseHandlerMethod)(
	EConnectionResponse	inResponse,
	uint16_t			inLocalPort,
	int					inDataSize,
	char const*			inData);

// This will be called when a udp packet is received, if this is called with inDataSize of 0 and inDat is NULL that means an error occurred
typedef void
(IInternetHandler::*TUDPPacketHandlerMethod)(
	EConnectionResponse	inResponse,
	uint16_t			inLocalPort,
	uint16_t			inRemotePort,
	uint32_t			inRemoteIPAddress,
	size_t				inDataSize,
	char const*			inData);

// This will be called when a HTTP connection receives response data from a server
typedef void
(IInternetHandler::*THTTPResponseHandlerMethod)(
	uint16_t			inHTTPReturnCode,
	int					inDataSize,
	char const*			inData);


class IInternetDevice
{
public:
	
	// Connect to a wireless access point if this is wireless
	virtual void
	ConnectToAP(
		char const*		inSSID,
		char const*		inPassword,
		EWirelessPWEnc	inPasswordEncryption) = 0;

	virtual void
	SetIPAddr(
		uint32_t	inIPAddr,
		uint32_t	inGatewayAddr,
		uint32_t	inSubnetAddr) = 0;

	// Start a server on the given port
	virtual bool
	Server_Open(
		uint16_t	inServerPort) = 0;

	// End a server on the given port
	virtual void
	Server_Close(
		uint16_t	inServerPort) = 0;

	// Open a connection to a remote server, the return value can be passed into Connection_OpenCompleted to check if the open request has completed, return -1 on error
	virtual int
	TCPRequestOpen(
		uint16_t	inRemoteServerPort,			// The port on the remote server expecting a connection attempt
		char const*	inRemoteServerAddress) = 0;	// The remote server address

	// Check if a open connection request has completed, returns true process has completed or false if it is still ongoing
	virtual bool
	TCPCheckOpenCompleted(
		int			inOpenRef,
		bool&		outSuccess,
		uint16_t&	outPort) = 0;

	// Do a non blocking check for data arriving on a previously opened port
	virtual void
	TCPGetData(
		uint16_t&	outPort,			// The previously open port that data was found on
		uint16_t&	outReplyPort,		// The port to send response data with
		size_t&		ioBufferSize,		// The size of the provided buffer in bytes on input and the bytes received on output
		char*		outBuffer) = 0;		// The buffer to store data

	// Send data on the previously opened port
	virtual bool
	TCPSendData(
		uint16_t	inPort,				// The open port
		size_t		inBufferSize,		// The number of bytes on the buffer
		char const*	inBuffer,			// The data to send
		bool		inFlush = false) = 0;
	
	// Check if the given port is ready to send data
	virtual uint32_t
	TCPGetPortState(
		uint16_t	inPort) = 0;		// The open port

	// End the transaction
	virtual void
	TCPCloseConnection(
		uint16_t	inPort) = 0;	// The open port

	// Open a udp connection, return -1 on error
	virtual int
	UDPOpenChannel(
		uint16_t	inLocalPort,
		uint16_t	inRemoteServerPort,			// The port on the remote server expecting a connection attempt, 0 if listening for packets
		char const*	inRemoteServerAddress) = 0;	// The remote server address, NULL if listening for packets
	
	// Check if a UDP channel is ready for processing
	virtual bool
	UDPChannelReady(
		int			inChannel) = 0;			// The channel from UDPOpenChannel

	// Check for a udp packet
	virtual bool
	UDPGetData(
		int			inChannel,			// The channel from UDPOpenChannel
		uint32_t&	outRemoteAddress,
		uint16_t&	outRemotePort,
		size_t&		ioBufferSize,		// The size of the provided buffer in bytes on input and the bytes received on output
		char*		outBuffer) = 0;		// The buffer to store data

	virtual bool
	UDPSendData(
		int			inChannel,			// The channel from UDPOpenChannel
		size_t		inBufferSize,
		void*		inBuffer,
		char const*	inRemoteAddress,
		uint16_t	inRemotePort) = 0;
	
	virtual void
	UDPCloseChannel(
		int			inChannel) = 0;	// The channel from UDPOpenChannel

	// Return true if a valid connection exists
	virtual bool
	ConnectedToInternet(
		void) = 0;

	// Returns true if the device is so f'd up it needs to be reset - if this does return true the device will stay in this state until ResetDevice is called
	virtual bool
	IsDeviceTotallyFd(
		void) = 0;

	// Reset the device - normally this only needs to be called if IsDeviceTotallyFd has returned true
	virtual void
	ResetDevice(
		void) = 0;
};

class CModule_Internet : public CModule, public IOutputDirector, public ICmdHandler, public IInternetHandler
{
public:
	
	MModule_Declaration(
		CModule_Internet)
	
	void
	Configure(
		IInternetDevice*	inInternetDevice);

	// Register a server handler
	void
	RegisterServer(
		uint16_t						inPort,				// The port for the server
		IInternetHandler*				inInternetHandler,	// The object of the handler
		TInternetServerHandlerMethod	inMethod);			// The method of the handler

	void
	RemoveServer(
		uint16_t	inPort);

	void
	TCPOpenConnection(
		uint16_t					inServerPort,
		char const*					inServerAddress,
		IInternetHandler*			inHandlerObject,			// The object of the handlers
		TTCPResponseHandlerMethod	inHandlerMethod);			// The response handler
	
	// The OpenConnection completion method must have been called before calling SendData
	bool
	TCPSendData(
		uint16_t	inLocalPort,
		size_t		inDataSize,
		char const*	inData,
		bool		inFlush);

	void
	TCPCloseConnection(
		uint16_t	inLocalPort);
	
	// UDP api is connection based to handle client server mode where the server changes the remote port for further communication after the initial packet is received
	int
	UDPOpenPort(
		char const*				inRemoteIPAddress,	// NULL if listening for packets
		uint16_t				inRemotePort,		// 0 if listening for packets
		IInternetHandler*		inHandlerObject,
		TUDPPacketHandlerMethod	inHandlerMethod,
		uint16_t				inLocalPort = 0,	// If 0 system will choose a local port to use
		uint32_t				inTimeoutSecs = eUDPTimeoutSecs);

	void
	UDPClosePort(
		int	inPortRef);

	bool
	UDPSend(
		int			inPortRef,
		size_t		inBufferSize,
		void*		inBuffer,
		uint32_t	inTimeoutSecs = 0);

	// Configure the internet connection to serve web pages on the given port
	void
	WebServer_Start(
		uint16_t inPort);

	void
	WebServer_RegisterPageHandler(
		char const*					inPage,					// This must be a static const string
		IInternetHandler*			inInternetHandler,		// The object of the handlers
		TInternetServerPageMethod	inMethod);				// This method will be called when a client connects to the server, use the inOutput parameter to send html code back to the client

	// Higher level HTTP service
	CHTTPConnection*
	CreateHTTPConnection(
		char const*					inServer,
		uint16_t					inPort,
		IInternetHandler*			inInternetHandler,	// The object of the handler
		THTTPResponseHandlerMethod	inResponseMethod);	// The method of the response handler
	
	bool
	ConnectedToInternet(
		void);

	// Utility functions
	static void
	TransformURLIntoParameters(
		int&			ioParameterCount,	// in -> max parameters, out -> actual parameter count
		char const**	outParameterList,	// An array of char* to parameter pairs, key then value
		char*&			outPageName,
		char*			inURL);

private:
	
	CModule_Internet(
		);

	virtual void
	Setup(
		void);

	virtual void
	Update(
		uint32_t	inDeltaTimeUS);
	
	uint8_t
	SerialCmd_WirelessSet(
		IOutputDirector*	inOutput,
		int					inArgC,
		char const*			inArgV[]);
	
	uint8_t
	SerialCmd_IPAddrSet(
		IOutputDirector*	inOutput,
		int					inArgC,
		char const*			inArgV[]);
	
	uint8_t
	SerialCmd_WirelessGet(
		IOutputDirector*	inOutput,
		int					inArgC,
		char const*			inArgV[]);
	
	uint8_t
	SerialCmd_IPAddrGet(
		IOutputDirector*	inOutput,
		int					inArgC,
		char const*			inArgV[]);

	virtual void
	write(
		char const* inMsg,
		size_t		inBytes);

	void
	CommandHomePageHandler(
		IOutputDirector*	inOutput,
		int					inParamCount,
		char const**		inParamList);

	void
	CommandProcessPageHandler(
		IOutputDirector*	inOutput,
		int					inParamCount,
		char const**		inParamList);

	virtual void
	EEPROMInitialize(
		void)
	{
		memset(&settings, 0, sizeof(settings));
	}

	struct SServer
	{
		uint16_t						port;
		IInternetHandler*				handlerObject;
		TInternetServerHandlerMethod	handlerMethod;
	};

	struct STCPConnection
	{
		int			openRef;
		uint16_t	localPort;
		uint16_t	serverPort;
		TString<eServerMaxAddressLength>	serverAddress;
		IInternetHandler*					handlerObject;
		TTCPResponseHandlerMethod			handlerResponseMethod;
	};

	struct SUDPConnection
	{
		int			deviceConnection;
		uint16_t	localPort;
		uint16_t	remotePort;
		bool		readyForUse;
		TEpochTime	sendStartTime;
		uint32_t	timeoutSecs;
		TString<eServerMaxAddressLength>	remoteAddress;
		IInternetHandler*					handlerObject;
		TUDPPacketHandlerMethod				handlerMethod;
	};

	struct SWebServerPageHandler
	{
		char const*					pageName;
		IInternetHandler*			object;
		TInternetServerPageMethod	method;
	};

	struct SSettings
	{
		TString<64>	ssid;
		TString<64>	pw;
		uint32_t	ipAddr;
		uint32_t	subnetAddr;
		uint32_t	gatewayAddr;
		uint8_t		securityType;
	};

	IInternetDevice*	internetDevice;
	SServer				serverList[eMaxServersCount];
	STCPConnection		tcpConnectionList[eMaxConnectionsCount];
	SUDPConnection		udpConnectionList[eMaxConnectionsCount];

	SSettings	settings;

	uint16_t	webServerPort;
	
	uint16_t	usedUDPPorts;

	SWebServerPageHandler	webServerPageHandlerList[eWebServerPageHandlerMax];

	bool		respondingServer;
	uint16_t	respondingServerPort;
	uint16_t	respondingReplyPort;
	
};

extern CModule_Internet*	gInternetModule;

#endif /* _EL_INTERNET_H_ */
