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

	eMaxPacketSize = 1500,
};
	
enum EWirelessPWEnc
{
	eWirelessPWEnc_Open,
	eWirelessPWEnc_WEP,
	eWirelessPWEnc_WPA2Personal,

};

class IInternetHandler
{
public:
};

typedef void
(IInternetHandler::*TInternetServerHandlerMethod)(
	IOutputDirector*	inOutput,
	int					inDataSize,
	char const*			inData);

typedef void
(IInternetHandler::*TInternetResponseHandlerMethod)(
	uint16_t	inLocalPort,
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

	// Do a non blocking check for data arriving on the server port, must be opened
	virtual void
	Server_GetData(
		uint16_t	inServerPort,		// The opened server port
		uint16_t&	outTransactionPort,	// The port to send response data with
		size_t&		ioBufferSize,		// The size of the provided buffer in bytes
		char*		outBuffer) = 0;		// The buffer to store data

	// Send the response to the client
	virtual void
	Server_SendData(
		uint16_t	inServerPort,		// The opened server port
		uint16_t	inTransactionPort,	// The transaction port returned from GetData
		size_t		inBufferSize,		// The number of bytes on the buffer
		char const*	inBuffer) = 0;		// The data to send

	// End the transaction
	virtual void
	Server_CloseConnection(
		uint16_t	inServerPort,
		uint16_t	inTransactionPort) = 0;

	// Open a connection to a remote server
	virtual bool
	Connection_Open(
		uint16_t&	outLocalPort,				// The local port on which to receive a reply
		uint16_t	inRemoteServerPort,			// The port on the remote server expecting a connection attempt
		char const*	inRemoteServerAddress) = 0;	// The remote server address

	// Close a previously opened connection
	virtual void
	Connection_Close(
		uint16_t	inLocalPort) = 0;
	
	// Start a request using the port provided by connection open
	virtual void
	Connection_InitiateRequest(
		uint16_t	inLocalPort,
		size_t		inDataSize,
		char const*	inData) = 0;

	// Do a non blocking check for reply data arriving from the server
	virtual void
	Connection_GetData(
		uint16_t	inPort,
		size_t&		ioBufferSize,
		char*		outBuffer) = 0;
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
		uint16_t&	outLocalPort,
		uint16_t	inServerPort,
		char const*	inServerAddress);

	void
	CloseConnection(
		uint16_t	inLocalPort);

	void
	InitiateRequest(
		uint16_t						inLocalPort,
		size_t							inDataSize,
		char const*						inData,
		IInternetHandler*				inInternetHandler,	// The object of the handler
		TInternetResponseHandlerMethod	inMethod);			// The method of the handler

	void
	ServeCommands(
		uint16_t	inPort);

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
		uint16_t	localPort;
		uint16_t	serverPort;
		char		serverAddress[eServerMaxAddressLength + 1];

		IInternetHandler*				handlerObject;
		TInternetResponseHandlerMethod	handlerMethod;
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

	uint16_t	commandServerPort;

	bool		respondingServer;
	uint16_t	respondingServerPort;
	uint16_t	respondingTransactionPort;
	

	static CModule_Internet	module;

};

extern CModule_Internet*	gInternet;

#endif /* _EL_INTERNET_H_ */
