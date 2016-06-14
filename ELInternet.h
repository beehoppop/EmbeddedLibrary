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

	This is a common header file for the library which every file includes. This allows me to include the appropriate
	header files based on the platform and if I am running the API simulator on Windows (and someday OSX)
*/
#include <ELModule.h>
#include <ELOutput.h>
#include <ELCommand.h>

enum
{
	eMaxServersCount = 4,
	eMaxConnectionsCount = 4,
	eServerMaxAddressLength = 63,

	eCommandServerFrontPageHandlerMax = 8,

	eMaxIncomingPacketSize = 1500,
	eMaxOutgoingPacketSize = 1400,

	ePortState_IsOpen			= 1 << 0,
	ePortState_CanSendData		= 1 << 1,
	ePortState_HasIncommingData	= 1 << 2,
	ePortState_Failure			= 1 << 3,


};

enum EConnectionResponse
{
	eConnectionResponse_Opened,
	eConnectionResponse_Closed,
	eConnectionResponse_Data,
	eConnectionResponse_Error,
};

enum EWirelessPWEnc
{
	eWirelessPWEnc_Open,
	eWirelessPWEnc_WEP,
	eWirelessPWEnc_WPA2Personal,
};

class CModule_Internet;

class IInternetHandler
{
public:
};

// This will be called when a client connects to the command server
typedef void
(IInternetHandler::*TInternetServerPageMethod)(
	IOutputDirector*	inOutput);

// This will be called when data arrives to a opened server
typedef void
(IInternetHandler::*TInternetServerHandlerMethod)(
	IOutputDirector*	inOutput,
	int					inDataSize,
	char const*			inData);

// This will be called when a client connection receives response data from a server, if this is called with inDataSize of 0 and inDat is NULL that means an error occurred
typedef void
(IInternetHandler::*TInternetResponseHandlerMethod)(
	EConnectionResponse	inResponse,
	uint16_t			inLocalPort,
	int					inDataSize,
	char const*			inData);

// This will be called when a HTTP connection receives response data from a server
typedef void
(IInternetHandler::*THTTPResponseHandlerMethod)(
	uint16_t			inHTTPReturnCode,
	int					inDataSize,
	char const*			inData);

class CHTTPConnection : public IInternetHandler
{
public:
	
	void
	StartRequest(
		char const*	inVerb,
		char const*	inURL);

	void
	SendHeaders(
		int	inHeaderCount,
		...);					// Variable length string headers header name and header value times inHeaderCount

	void
	SendParameters(
		int	inParameterCount,
		...);					// Variable length string parameters param name and param value times inParameterCount

	void
	SendBody(
		char const*	inBody);

	void
	Close(
		void);

private:
	
	enum
	{
		eResponseState_HTTP,
		eResponseState_Headers,
		eResponseState_Body,
		eResponseState_Done,
	};

	CHTTPConnection(
		char const*							inServer,
		uint16_t							inPort,
		IInternetHandler*					inInternetHandler,
		THTTPResponseHandlerMethod			inResponseMethod);

	void
	ResponseHandlerMethod(
		EConnectionResponse	inResponse,
		uint16_t			inLocalPort,
		int					inDataSize,
		char const*			inData);

	void
	SendData(
		char const*	inData);
	
	void
	ProcessResponseData(
		int			inDataSize,
		char const*	inData);
	
	void
	ProcessResponseLine(
		void);
	
	void
	FinishResponse(
		void);

	IInternetHandler*					internetHandler;
	THTTPResponseHandlerMethod			responseMethod;

	char		serverAddress[64];
	uint16_t	serverPort;
	uint16_t	localPort;

	uint16_t	requestIndex;
	char		buffer[512];

	uint16_t	responseContentSize;
	uint16_t	responseContentIndex;
	uint16_t	responseHTTPCode;
	uint8_t		responseState;
	bool		waitingOnResponse;
	bool		openInProgress;

	friend class CModule_Internet;
};

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
		uint32_t	inSubnetAddr,
		uint32_t	inGatewayAddr) = 0;

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
	Client_RequestOpen(
		uint16_t	inRemoteServerPort,			// The port on the remote server expecting a connection attempt
		char const*	inRemoteServerAddress) = 0;	// The remote server address

	// Check if a open connection request has completed
	virtual bool
	Client_OpenCompleted(
		int			inOpenRef,
		bool&		outSuccess,
		uint16_t&	outPort) = 0;

	// Do a non blocking check for data arriving on a previously opened port
	virtual void
	GetData(
		uint16_t&	outPort,			// The previously open port that data was found on
		uint16_t&	outReplyPort,		// The port to send response data with
		size_t&		ioBufferSize,		// The size of the provided buffer in bytes
		char*		outBuffer) = 0;		// The buffer to store data

	// Send data on the previously opened port
	virtual bool
	SendData(
		uint16_t	inPort,				// The open port
		size_t		inBufferSize,		// The number of bytes on the buffer
		char const*	inBuffer,			// The data to send
		bool		inFlush = false) = 0;
	
	// Check if the given port is ready to send data
	virtual uint32_t
	GetPortState(
		uint16_t	inPort) = 0;		// The open port

	// End the transaction
	virtual void
	CloseConnection(
		uint16_t	inPort) = 0;	// The open port
};

class CModule_Internet : public CModule, public IOutputDirector, public ICmdHandler
{
public:
	
	CModule_Internet(
		);

	void
	SetInternetDevice(
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
	OpenConnection(
		uint16_t								inServerPort,
		char const*								inServerAddress,
		IInternetHandler*						inHandlerObject,			// The object of the handlers
		TInternetResponseHandlerMethod			inHandlerMethod);			// The response handler
	
	// The OpenConnection completion method must have been called before calling SendData
	bool
	SendData(
		uint16_t						inLocalPort,
		size_t							inDataSize,
		char const*						inData);

	void
	CloseConnection(
		uint16_t	inLocalPort);

	// Configure the internet connection to serve commands on the given port
	void
	CommandServer_Start(
		uint16_t inPort);

	void
	CommandServer_RegisterFrontPage(
		IInternetHandler*			inInternetHandler,			// The object of the handlers
		TInternetServerPageMethod	inMethod);					// This method will be called when a client connects to the server, use the inOutput parameter to send html code to the client
		

	// Higher level HTTP service
	CHTTPConnection*
	CreateHTTPConnection(
		char const*							inServer,
		uint16_t							inPort,
		IInternetHandler*					inInternetHandler,	// The object of the handler
		THTTPResponseHandlerMethod			inResponseMethod);	// The method of the response handler

private:

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

	struct SServer
	{
		uint16_t						port;
		IInternetHandler*				handlerObject;
		TInternetServerHandlerMethod	handlerMethod;
	};

	struct SConnection
	{
		bool		waitingOnOpen;
		int			openRef;
		uint16_t	localPort;
		uint16_t	serverPort;
		char		serverAddress[eServerMaxAddressLength + 1];

		IInternetHandler*						handlerObject;
		TInternetResponseHandlerMethod			handlerResponseMethod;
	};

	struct SCommandServerFrontPageHandler
	{
		IInternetHandler*			commandServerObject;
		TInternetServerPageMethod	commandServerMethod;
	};

	struct SSettings
	{
		char		ssid[64];
		char		pw[64];
		uint32_t	ipAddr;
		uint32_t	subnetAddr;
		uint32_t	gatewayAddr;
		uint8_t		securityType;
	};

	IInternetDevice*	internetDevice;
	SServer		serverList[eMaxServersCount];
	SConnection	connectionList[eMaxConnectionsCount];

	SSettings	settings;

	uint16_t						commandServerPort;
	
	SCommandServerFrontPageHandler	commandServerFrontPageHandlerList[eCommandServerFrontPageHandlerMax];

	bool		respondingServer;
	uint16_t	respondingServerPort;
	uint16_t	respondingReplyPort;
	
};

extern CModule_Internet*	gInternet;

#endif /* _EL_INTERNET_H_ */
