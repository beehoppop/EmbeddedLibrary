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
#include <ELInternetHTTP.h>
#include <ELAssert.h>
#include <ELUtilities.h>
#include <ELCommand.h>

CModule_Internet*	gInternetModule;

static char const*	gReplyStringPreOutput = 
	"HTTP/1.1 200 OK\r\n"
	"Content-Type: text/html\r\n"
	"Connection: close\r\n"
	"\r\n"
	"<!DOCTYPE html><html><body>"
	"<form action=\"cmd_data\">Command: <input type=\"text\" name=\"Command\" autocapitalize=\"none\"><br><input type=\"submit\" value=\"Submit\"></form><p>Click the \"Submit\" button and the command will be sent to the server. Return <a href=\"/\">Home</a></p><code>"
	;
static char const*	gReplyStringPostOutput = "</code></body></html>";
	
CHTTPConnection::CHTTPConnection(
	char const*							inServer,
	uint16_t							inPort,
	IInternetHandler*					inInternetHandler,
	THTTPResponseHandlerMethod			inResponseMethod)
{
	internetHandler = inInternetHandler;
	responseMethod = inResponseMethod;
	serverAddress = inServer;
	serverPort = inPort;
	localPort = eInvalidPort;
	openInProgress = false;
	responseState = eResponseState_None;
	flushPending = false;

	eventRef = MRealTimeCreateEvent("HTTP", CHTTPConnection::CheckForTimeoutEvent, NULL);
	gRealTime->ScheduleEvent(eventRef, 1000000, false);
}

CHTTPConnection::~CHTTPConnection(
	)
{
	if(eventRef != NULL)
	{
		gRealTime->DestroyEvent(eventRef);
		eventRef = NULL;
	}
}

void
CHTTPConnection::StartRequest(
	char const*	inVerb,
	char const*	inURL,
	int			inParameterCount,
	...)
{
	MReturnOnError(responseState != eResponseState_None);

	TString<256> buffer;

	isPost = strcmp(inVerb, "POST") == 0;

	if(inParameterCount > 0)
	{
		parameterBuffer.Clear();

		va_list	valist;

		va_start(valist, inParameterCount);
	
		for(int i = 0; i < inParameterCount; ++i)
		{
			char const*	key = va_arg(valist, char const*);
			char const* value = va_arg(valist, char const*);

			parameterBuffer.AppendF("%s=%s", key, value);
			if(i < inParameterCount - 1)
			{
				parameterBuffer.Append('&');
			}
		}

		va_end(valist);

		if(strcmp(inVerb, "GET") == 0)
		{
			buffer.SetF("%s %s?%s HTTP/1.1\r\n", inVerb, inURL, (char*)parameterBuffer);
		}
		else
		{
			buffer.SetF("%s %s HTTP/1.1\r\n", inVerb, inURL);
		}
	}
	else
	{
		buffer.SetF("%s %s HTTP/1.1\r\n", inVerb, inURL);
	}

	SendData(buffer, false);
}

void
CHTTPConnection::SetHeaders(
	int	inHeaderCount,
	...)
{
	TString<256> buffer;

	MReturnOnError(responseState != eResponseState_None);

	va_list	valist;

	va_start(valist, inHeaderCount);
	
	for(int i = 0; i < inHeaderCount; ++i)
	{
		char const*	key = va_arg(valist, char const*);
		char const* value = va_arg(valist, char const*);

		buffer.AppendF("%s: %s\r\n", key, value);
	}

	va_end(valist);

	SendData(buffer, false);
}

void
CHTTPConnection::CompleteRequest(
	char const*	inBody)
{
	MReturnOnError(responseState != eResponseState_None);

	size_t	bodyLen = strlen(inBody);
	char blBuffer[32];

	// Add the Content-Length header
	if(bodyLen > 0)
	{
		SetHeaders(4, "Content-Length", _itoa((int)bodyLen, blBuffer, 10), "User-Agent", "EmbeddedLibrary", "Host", (char*)serverAddress, "Accept", "*/*"); 
	}
	else if(isPost)
	{
		SetHeaders(4, "Content-Length", _itoa((int)parameterBuffer.GetLength(), blBuffer, 10), "User-Agent", "EmbeddedLibrary", "Host", (char*)serverAddress, "Accept", "*/*"); 
	}
	else
	{
		SetHeaders(3, "User-Agent", "EmbeddedLibrary", "Host", (char*)serverAddress, "Accept", "*/*"); 
	}
		
	// Add a blank line
	SendData("\r\n", false);

	if(bodyLen > 0)
	{
		// Send the body
		SendData(inBody, true);
	}
	else if(isPost)
	{
		SendData(parameterBuffer, true);
	}
	else
	{
		SendData("", true);
	}

	responseContentSize = 0;
	responseState = eResponseState_HTTP;
	responseHTTPCode = 0;
}

void
CHTTPConnection::CloseConnection(
	void)
{
	if(localPort != eInvalidPort)
	{
		gInternetModule->TCPCloseConnection(localPort);
	}

	delete this;
}

void
CHTTPConnection::DumpDebugInfo(
	IOutputDirector*	inOutput)
{
	inOutput->printf("millis() = %lu\n", millis());
	inOutput->printf("dataSentTimeMS = %lu\n", dataSentTimeMS);
	inOutput->printf("responseContentSize = %d\n", responseContentSize);
	inOutput->printf("responseHTTPCode = %d\n", responseHTTPCode);
	inOutput->printf("responseState = %d\n", responseState);
	inOutput->printf("openInProgress = %d\n", openInProgress);
	inOutput->printf("flushPending = %d\n", flushPending);
}

void
CHTTPConnection::ResponseHandlerMethod(
	EConnectionResponse	inResponse,
	uint16_t			inLocalPort,
	int					inDataSize,
	char const*			inData)
{
	switch(inResponse)
	{
		case eConnectionResponse_Opened:
			openInProgress = false;
			localPort = inLocalPort;
			if(tempBuffer.GetLength() > 0)
			{
				if(gInternetModule->TCPSendData(localPort, tempBuffer.GetLength(), tempBuffer, flushPending))
				{
					tempBuffer.Clear();
					flushPending = false;
					dataSentTimeMS = millis();
				}
				else
				{
					ReopenConnection();
				}
			}
			break;

		case eConnectionResponse_Closed:
		case eConnectionResponse_Error:
			if(responseState != eResponseState_None)
			{
				if(inResponse == eConnectionResponse_Error)
				{
					responseHTTPCode = 504;
				}
				else
				{
					responseHTTPCode = 500;
				}
				responseContentSize = 0;
				FinishResponse();
			}

			if(localPort != eInvalidPort)
			{
				gInternetModule->TCPCloseConnection(localPort);
				localPort = eInvalidPort;
			}

			openInProgress = false;
			break;

		case eConnectionResponse_Data:
			if(responseState != eResponseState_None)
			{
				ProcessResponseData(inDataSize, inData);
			}
			break;
			
	}
}

void
CHTTPConnection::SendData(
	char const*	inData,
	bool		inFlush)
{
	dataSentTimeMS = millis();
	if(localPort != eInvalidPort)
	{
		if(gInternetModule->TCPSendData(localPort, strlen(inData), inData, inFlush) == false)
		{
			ReopenConnection();
			tempBuffer = inData;
			flushPending |= inFlush;
		}
	}
	else
	{
		if(!openInProgress)
		{
			// Start an open request
			MInternetOpenConnection(serverPort, serverAddress, CHTTPConnection::ResponseHandlerMethod);
			openInProgress = true;
		}

		tempBuffer.Append(inData);
		flushPending |= inFlush;
	}
}

void
CHTTPConnection::ProcessResponseData(
	int			inDataSize,
	char const*	inData)
{
	char const*	cp = inData;
	char const*	ep = cp + inDataSize;

	while(cp < ep)
	{
		char c = *cp++;
		if(responseState == eResponseState_Body)
		{
			tempBuffer.Append(c);
			if(tempBuffer.GetLength() >= responseContentSize)
			{
				FinishResponse();
				return;
			}
		}
		else
		{
			if(c == '\r')
			{
				ProcessResponseLine();
				tempBuffer.Clear();
				if(*cp == '\n')
				{
					++cp;
				}
				continue;
			}

			tempBuffer.Append(c);
		}
	}
}

void
CHTTPConnection::ProcessResponseLine(
	void)
{
	if(tempBuffer.GetLength() == 0)
	{
		// If the response line is empty that means the response body is starting
		if(responseContentSize > 0)
		{
			// We are expecting a body so start saving that data
			responseState = eResponseState_Body;
		}
		else
		{
			// If there is no content size then there is no body data to get so just finish now
			FinishResponse();
		}
		return;
	}

	if(responseState == eResponseState_HTTP)
	{
		if(tempBuffer.StartsWith("HTTP"))
		{
			tempBuffer.TrimUntilNextWord();

			responseHTTPCode = atoi(tempBuffer);

			responseState = eResponseState_Headers;
		}
	}
	else
	{
		tempBuffer.TrimStartingSpace();
		if(tempBuffer.StartsWith("Content-Length"))
		{
			tempBuffer.TrimBeforeChr(':');
			tempBuffer.TrimStartingSpace();
			responseContentSize = atoi(tempBuffer);
		}
	}
}

void
CHTTPConnection::FinishResponse(
	void)
{
	responseState = eResponseState_None;
	(internetHandler->*responseMethod)(responseHTTPCode, responseContentSize, tempBuffer);
	tempBuffer.Clear();
}

void
CHTTPConnection::CheckForTimeoutEvent(
	TRealTimeEventRef	inRef,
	void*				inRefCon)
{
	if(responseState != eResponseState_None && (millis() - dataSentTimeMS >= eHTTPTimeoutMS))
	{
		// we have timed out waiting for a response
		responseHTTPCode = 500;
		FinishResponse();
		openInProgress = false;
		if(localPort != eInvalidPort)
		{
			gInternetModule->TCPCloseConnection(localPort);
			localPort = eInvalidPort;
		}
	}
}

void
CHTTPConnection::ReopenConnection(
	void)
{
	if(localPort != eInvalidPort)
	{
		gInternetModule->TCPCloseConnection(localPort);
		localPort = eInvalidPort;
	}

	// Re-open a connection
	MInternetOpenConnection(serverPort, serverAddress, CHTTPConnection::ResponseHandlerMethod);
	openInProgress = true;
}

MModuleImplementation_Start(CModule_Internet)
MModuleImplementation_FinishGlobal(CModule_Internet, gInternetModule)

CModule_Internet::CModule_Internet(
	)
	:
	CModule(sizeof(settings), 0, &settings, 10000)
{
	internetDevice = NULL;
	memset(serverList, 0, sizeof(serverList));
	memset(tcpConnectionList, 0, sizeof(tcpConnectionList));
	memset(udpConnectionList, 0, sizeof(udpConnectionList));
	memset(webServerPageHandlerList, 0, sizeof(webServerPageHandlerList));
	webServerPort = 0;
	respondingServer = false;
	respondingServerPort = 0;
	respondingReplyPort = 0;
	usedUDPPorts = 0;

	STCPConnection*	curTCP = tcpConnectionList;
	for(int i = 0; i < eMaxConnectionsCount; ++i, ++curTCP)
	{
		curTCP->openRef = -1;
		curTCP->localPort = eInvalidPort;
	}

	SUDPConnection*	curUDP = udpConnectionList;
	for(int i = 0; i < eMaxConnectionsCount; ++i, ++curUDP)
	{
		curUDP->deviceConnection = -1;
		curUDP->localPort = eInvalidPort;
	}

	CModule_RealTime::Include();
}

void
CModule_Internet::Configure(
	IInternetDevice*	inInternetDevice)
{
	internetDevice = inInternetDevice;
}

// Register a server handler
void
CModule_Internet::RegisterServer(
	uint16_t						inPort,
	IInternetHandler*				inInternetHandler,
	TInternetServerHandlerMethod	inMethod)
{
	MReturnOnError(internetDevice == NULL);

	SServer*	target = NULL;
	SServer*	cur = serverList;
	for(int i = 0; i < eMaxServersCount; ++i, ++cur)
	{
		if(cur->port == inPort)
		{
			target = cur;
			break;
		}
		else if(target == NULL && cur->handlerObject == NULL)
		{
			target = cur;
		}
	}

	MReturnOnError(target == NULL);

	bool success = internetDevice->Server_Open(inPort);
	MReturnOnError(success == false);

	target->port = inPort;
	target->handlerObject = inInternetHandler;
	target->handlerMethod = inMethod;
}

void
CModule_Internet::RemoveServer(
	uint16_t	inPort)
{
	SServer*	cur = serverList;
	for(int i = 0; i < eMaxServersCount; ++i, ++cur)
	{
		if(cur->port == inPort)
		{
			cur->port = 0;
			cur->handlerMethod = NULL;
			cur->handlerObject = NULL;
			internetDevice->Server_Close(inPort);
			return;
		}
	}
}

void
CModule_Internet::TCPOpenConnection(
	uint16_t					inServerPort,
	char const*					inServerAddress,
	IInternetHandler*			inInternetHandler,
	TTCPResponseHandlerMethod	inResponseMethod)
{
	MReturnOnError(internetDevice == NULL);
	MReturnOnError(strlen(inServerAddress) > eServerMaxAddressLength - 1);

	STCPConnection*	target = NULL;
	STCPConnection*	cur = tcpConnectionList;
	for(int i = 0; i < eMaxConnectionsCount; ++i, ++cur)
	{
		if(cur->serverPort == inServerPort && cur->serverAddress == inServerAddress)
		{
			target = cur;
			break;
		}
		else if(target == NULL && cur->serverAddress.GetLength() == 0)
		{
			target = cur;
		}
	}

	MReturnOnError(target == NULL);

	target->openRef = internetDevice->TCPRequestOpen(inServerPort, inServerAddress);
	if(target->openRef < 0)
	{
		(inInternetHandler->*inResponseMethod)(eConnectionResponse_Error, 0, 0, NULL);
		return;
	}

	target->handlerObject = inInternetHandler;
	target->handlerResponseMethod = inResponseMethod;
	target->serverPort = inServerPort;
	target->serverAddress = inServerAddress;
	target->localPort = eInvalidPort;
}

bool
CModule_Internet::TCPSendData(
	uint16_t						inLocalPort,
	size_t							inDataSize,
	char const*						inData,
	bool							inFlush)
{
	STCPConnection*	target = NULL;
	STCPConnection*	curConnection = tcpConnectionList;
	for(int i = 0; i < eMaxConnectionsCount; ++i, ++curConnection)
	{
		if(curConnection->localPort == inLocalPort)
		{
			target = curConnection;
			break;
		}
	}

	MReturnOnError(target == NULL || target->openRef > 0, false);

	if(internetDevice->TCPSendData(inLocalPort, inDataSize, inData, inFlush) == false)
	{
		TCPCloseConnection(inLocalPort);
		return false;
	}

	return true;
}

void
CModule_Internet::TCPCloseConnection(
	uint16_t	inLocalPort)
{
	MReturnOnError(inLocalPort == eInvalidPort);

	STCPConnection*	cur = tcpConnectionList;
	for(int i = 0; i < eMaxConnectionsCount; ++i, ++cur)
	{
		if(cur->localPort == inLocalPort)
		{
			cur->localPort = eInvalidPort;
			cur->serverPort = 0;
			cur->serverAddress.Clear();
			cur->handlerResponseMethod = NULL;
			cur->handlerObject = NULL;
			cur->openRef = -1;
			internetDevice->TCPCloseConnection(inLocalPort);
			return;
		}
	}
}
	
int
CModule_Internet::UDPOpenPort(
	char const*				inRemoteAddress,
	uint16_t				inRemotePort,
	IInternetHandler*		inHandlerObject,
	TUDPPacketHandlerMethod	inHandlerMethod,
	uint16_t				inLocalPort,
	uint32_t				inTimeoutSecs)
{
	MReturnOnError(internetDevice == NULL, -1);

	SUDPConnection*	target = NULL;
	SUDPConnection*	cur = udpConnectionList;
	for(int i = 0; i < eMaxConnectionsCount; ++i, ++cur)
	{
		if(cur->deviceConnection < 0)
		{
			target = cur;
			break;
		}
	}

	MReturnOnError(target == NULL, -1);

	if(inLocalPort == 0)
	{
		// find an open port
		for(int i = 0; i < eLocalPortBase; ++i)
		{
			if(!(usedUDPPorts & (1 << i)))
			{
				inLocalPort = eLocalPortBase + i;
				usedUDPPorts |= 1 << i;
				break;
			}
		}

		MReturnOnError(inLocalPort == 0, -1);
	}

	target->deviceConnection = internetDevice->UDPOpenChannel(inLocalPort, inRemotePort, inRemoteAddress);
	MReturnOnError(target->deviceConnection < 0, -1);

	target->handlerObject = inHandlerObject;
	target->handlerMethod = inHandlerMethod;
	target->localPort = inLocalPort;
	target->remoteAddress = inRemoteAddress;
	target->remotePort = inRemotePort;
	target->readyForUse = false;

	return int(target - udpConnectionList);
}

void
CModule_Internet::UDPClosePort(
	int	inConnectionRef)
{
	MReturnOnError(internetDevice == NULL);
	MReturnOnError(inConnectionRef < 0 || inConnectionRef >= eMaxConnectionsCount);

	SUDPConnection*	target = udpConnectionList + inConnectionRef;

	internetDevice->UDPCloseChannel(target->deviceConnection);

	if(target->localPort >= eLocalPortBase && target->localPort < eLocalPortBase + eLocalPortCount)
	{
		usedUDPPorts &= ~(1 << (target->localPort - eLocalPortBase));
	}

	target->deviceConnection = -1;
	target->localPort = eInvalidPort;
	target->handlerMethod = NULL;
	target->handlerObject = NULL;
}

bool
CModule_Internet::UDPSend(
	int			inConnectionRef,
	size_t		inBufferSize,
	void*		inBuffer,
	uint32_t	inTimeoutSecs)
{
	MReturnOnError(internetDevice == NULL, false);
	MReturnOnError(inConnectionRef < 0 || inConnectionRef >= eMaxConnectionsCount, false);

	SUDPConnection*	target = udpConnectionList + inConnectionRef;
	
	target->sendStartTime = gRealTime->GetEpochTime(true);
	target->timeoutSecs = inTimeoutSecs;
	bool	result = internetDevice->UDPSendData(target->deviceConnection, inBufferSize, inBuffer, (char*)target->remoteAddress, target->remotePort);

	return result;
}

void
CModule_Internet::WebServer_Start(
	uint16_t	inPort)
{
	MReturnOnError(internetDevice == NULL);

	MInternetRegisterPage("/", CModule_Internet::CommandHomePageHandler);
	MInternetRegisterPage("/cmd_data", CModule_Internet::CommandProcessPageHandler);

	webServerPort = inPort;
	internetDevice->Server_Open(inPort);
}

void
CModule_Internet::WebServer_RegisterPageHandler(
	char const*					inPage,
	IInternetHandler*			inInternetHandler,
	TInternetServerPageMethod	inMethod)
{
	for(int i = 0; i < eWebServerPageHandlerMax; ++i)
	{
		if(webServerPageHandlerList[i].object == NULL)
		{
			webServerPageHandlerList[i].pageName = inPage;
			webServerPageHandlerList[i].object = inInternetHandler;
			webServerPageHandlerList[i].method = inMethod;
			return;
		}
	}
	
	MReturnOnError(true);
}

CHTTPConnection*
CModule_Internet::CreateHTTPConnection(
	char const*							inServer,
	uint16_t							inPort,
	IInternetHandler*					inInternetHandler,
	THTTPResponseHandlerMethod			inResponseMethod)
{
	MReturnOnError(internetDevice == NULL, NULL);
	return new CHTTPConnection(inServer, inPort, inInternetHandler, inResponseMethod);
}
	
bool
CModule_Internet::ConnectedToInternet(
	void)
{
	return internetDevice && internetDevice->ConnectedToInternet();
}

void
CModule_Internet::Setup(
	void)
{
	MReturnOnError(internetDevice == NULL);	// Internet device needs to be setup before the general internet module

	if(settings.ssid.GetLength() != 0)
	{
		internetDevice->ConnectToAP(settings.ssid, settings.pw, EWirelessPWEnc(settings.securityType));
	}

	if(settings.ipAddr != 0)
	{
		internetDevice->SetIPAddr(settings.ipAddr, settings.gatewayAddr, settings.subnetAddr);
	}

	MCommandRegister("wireless_set", CModule_Internet::SerialCmd_WirelessSet, "[ssid] [pw] [wpa2|wep|open] : Set the wireless configuration");
	MCommandRegister("wireless_get", CModule_Internet::SerialCmd_WirelessGet, ": Get the wireless configuration");
	MCommandRegister("ip_set", CModule_Internet::SerialCmd_IPAddrSet, "[ip addr] [gateway addr] [subnet mask] : Set the ip configuration");
	MCommandRegister("ip_get", CModule_Internet::SerialCmd_IPAddrGet, ": Get the ip configuration");
}

void
CModule_Internet::Update(
	uint32_t	inDeltaTimeUS)
{
	size_t	bufferSize;
	char	buffer[eMaxIncomingPacketSize + 1];

	if(internetDevice == NULL)
	{
		return;
	}

	STCPConnection*	curTCPConnection;

	if(internetDevice->IsDeviceTotallyFd())
	{
		// Close all connections
		curTCPConnection = tcpConnectionList;
		for(int i = 0; i < eMaxConnectionsCount; ++i, ++curTCPConnection)
		{
			if(curTCPConnection->handlerObject != NULL)
			{
				(curTCPConnection->handlerObject->*curTCPConnection->handlerResponseMethod)(eConnectionResponse_Error, 0, 0, NULL);
			}
			curTCPConnection->localPort = eInvalidPort;
			curTCPConnection->serverPort = 0;
			curTCPConnection->serverAddress.Clear();
			curTCPConnection->handlerResponseMethod = NULL;
			curTCPConnection->handlerObject = NULL;
			curTCPConnection->openRef = -1;
		}

		// close udp connections
		// xxx

		internetDevice->ResetDevice();

		// Setup all the servers
		SServer*	curServer = serverList;
		for(int i = 0; i < eMaxServersCount; ++i, ++curServer)
		{
			if(curServer->handlerObject != NULL)
			{
				internetDevice->Server_Open(curServer->port);
			}
		}

		// Setup the webserver
		if(webServerPort != 0)
		{
			internetDevice->Server_Open(webServerPort);
		}
	}

	// Update the current TCP connections
	curTCPConnection = tcpConnectionList;
	for(int i = 0; i < eMaxConnectionsCount; ++i, ++curTCPConnection)
	{
		if(curTCPConnection->handlerObject == NULL)
		{
			continue;
		}

		// If we have a open in process check it's status
		if(curTCPConnection->openRef >= 0)
		{
			bool	successfulyOpened;
			if(internetDevice->TCPCheckOpenCompleted(curTCPConnection->openRef, successfulyOpened, curTCPConnection->localPort))
			{
				// call completion
				curTCPConnection->openRef = -1;
				if(successfulyOpened)
				{
					(curTCPConnection->handlerObject->*curTCPConnection->handlerResponseMethod)(eConnectionResponse_Opened, curTCPConnection->localPort, 0, NULL);
				}
				else
				{
					(curTCPConnection->handlerObject->*curTCPConnection->handlerResponseMethod)(eConnectionResponse_Error, 0, 0, NULL);

					// Empty out the connection since another OpenConnection will be required to use this slot
					curTCPConnection->serverAddress.Clear();
					curTCPConnection->handlerObject = NULL;
				}
			}
			continue;
		}

		MAssert(curTCPConnection->localPort != eInvalidPort);

		uint32_t	portState = internetDevice->TCPGetPortState(curTCPConnection->localPort);

		// If the port has failed or closed report that
		if(portState & ePortState_Failure)
		{
			(curTCPConnection->handlerObject->*curTCPConnection->handlerResponseMethod)(eConnectionResponse_Error, curTCPConnection->localPort, 0, NULL);
			// Since the method above may have already closed the port we need to check for that
			if(curTCPConnection->localPort != eInvalidPort)
			{
				TCPCloseConnection(curTCPConnection->localPort);
			}
		}
		else if(!(portState & ePortState_IsOpen))
		{
			(curTCPConnection->handlerObject->*curTCPConnection->handlerResponseMethod)(eConnectionResponse_Closed, curTCPConnection->localPort, 0, NULL);
			// Since the method above may have already closed the port we need to check for that
			if(curTCPConnection->localPort != eInvalidPort)
			{
				TCPCloseConnection(curTCPConnection->localPort);
			}
		}
	}

	// Get any incoming UDP packets

	// Update current UDP listeners
	SUDPConnection*	curUDPConnection = udpConnectionList;
	for(int i = 0; i < eMaxConnectionsCount; ++i, ++curUDPConnection)
	{
		if(curUDPConnection->handlerObject == NULL)
		{
			continue;
		}

		if(!curUDPConnection->readyForUse)
		{
			if(internetDevice->UDPChannelReady(curUDPConnection->deviceConnection))
			{
				curUDPConnection->readyForUse = true;
				(curUDPConnection->handlerObject->*curUDPConnection->handlerMethod)(eConnectionResponse_Opened, 0, 0, 0, 0, NULL);
			}
			continue;
		}

		uint32_t	remoteAddress;
		uint16_t	remotePort;

		if(internetDevice->UDPGetData(curUDPConnection->deviceConnection, remoteAddress, remotePort, bufferSize, buffer))
		{
			curUDPConnection->timeoutSecs = 0;
			curUDPConnection->remotePort = remotePort;
			curUDPConnection->remoteAddress.SetF("%d.%d.%d.%d", (remoteAddress >> 24) & 0xFF, (remoteAddress >> 16) & 0xFF, (remoteAddress >> 8) & 0xFF, (remoteAddress >> 0) & 0xFF);
			(curUDPConnection->handlerObject->*curUDPConnection->handlerMethod)(eConnectionResponse_Data, curUDPConnection->localPort, remotePort, remoteAddress, bufferSize, buffer);
		}

		if(curUDPConnection->timeoutSecs > 0 && (gRealTime->GetEpochTime(true) - curUDPConnection->sendStartTime) > curUDPConnection->timeoutSecs)
		{
			SystemMsg("ERROR: UDP timeout");
			(curUDPConnection->handlerObject->*curUDPConnection->handlerMethod)(eConnectionResponse_Error, 0, 0, 0, 0, 0);
			curUDPConnection->timeoutSecs = 0;
		}
	}

	// Check for incoming data
	bufferSize = sizeof(buffer) - 1;
	uint16_t	localPort;
	uint16_t	replyPort;
	internetDevice->TCPGetData(localPort, replyPort, bufferSize, buffer);

	if(bufferSize == 0)
	{
		return;
	}

	buffer[bufferSize] = 0;
	
	if(webServerPort > 0 && localPort == webServerPort)
	{
		//Serial.printf("Got cmd server data chn=%d\n", replyPort);

		char*	verb = NULL;
		char*	url = NULL;
		char*	nextSpace;

		verb = buffer;
		nextSpace = strchr(verb, ' ');
		if(nextSpace != NULL)
		{
			*nextSpace = 0;
			url = nextSpace + 1;

			nextSpace = strchr(url, ' ');
			if(nextSpace != NULL)
			{
				*nextSpace = 0;
			}
		}

		if(verb != NULL && strcmp(verb, "GET") == 0 && url != NULL)
		{
			char const*	paramList[32];	
			int		paramCount = 32;
			char*	pageName;

			TransformURLIntoParameters(paramCount, paramList, pageName, url);

			internetDevice->TCPSendData(replyPort, strlen(gReplyStringPreOutput), gReplyStringPreOutput);

			respondingServer = true;
			respondingServerPort = webServerPort;
			respondingReplyPort = replyPort;

			for(int i = 0; i < eWebServerPageHandlerMax; ++i)
			{
				if(webServerPageHandlerList[i].object != NULL && strcmp(pageName, webServerPageHandlerList[i].pageName) == 0)
				{
					(webServerPageHandlerList[i].object->*(webServerPageHandlerList[i].method))(this, paramCount, paramList);
				}
			}

			internetDevice->TCPSendData(replyPort, strlen(gReplyStringPostOutput), gReplyStringPostOutput);
			internetDevice->TCPCloseConnection(replyPort);
		}
		else
		{
			Serial.printf("  unknown cmd request");
			internetDevice->TCPCloseConnection(replyPort);
		}
	}
	else
	{
		// look for the server connection the data came back on
		SServer*	curServer = serverList;
		bool		dataProcessed = false;
		for(int i = 0; i < eMaxServersCount; ++i, ++curServer)
		{
			if(curServer->handlerObject != NULL && curServer->port == localPort)
			{
				// we got data, now call the handler
				respondingServer = true;
				respondingServerPort = curServer->port;
				respondingReplyPort = replyPort;
				(curServer->handlerObject->*curServer->handlerMethod)(this, (int)bufferSize, buffer);
				internetDevice->TCPCloseConnection(replyPort);
				dataProcessed = true;
				break;
			}
		}

		if(!dataProcessed)
		{
			// Look for a client connection
			curTCPConnection = tcpConnectionList;
			for(int i = 0; i < eMaxConnectionsCount; ++i, ++curTCPConnection)
			{
				if(curTCPConnection->handlerObject != NULL && curTCPConnection->localPort == localPort)
				{
					// Call the response handler with the data
					(curTCPConnection->handlerObject->*curTCPConnection->handlerResponseMethod)(eConnectionResponse_Data, curTCPConnection->localPort, (int)bufferSize, buffer);
				}
			}
		}
	}
}

void
CModule_Internet::write(
	char const* inMsg,
	size_t		inBytes)
{
	char const*	csp = inMsg;
	char const*	cep = inMsg + inBytes;
	char const*	lineStart = csp;

	uint32_t	portState = internetDevice->TCPGetPortState(respondingReplyPort);
	if(!(portState & ePortState_IsOpen) || (portState & ePortState_Failure))
	{
		return;
	}

	while(csp < cep)
	{
		char c = *csp++;

		if(c == '\n' || c == '\r')
		{
			char const*	lineEnd = csp - 1;
			char c2 = *csp;

			if((c == '\n' && c2 == '\r') || (c == '\r' && c2 == '\n'))
			{
				++csp;
			}

			if(respondingServer)
			{
				if(internetDevice->TCPSendData(respondingReplyPort, lineEnd - lineStart, lineStart) == false)
				{
					return;
				}

				if(internetDevice->TCPSendData(respondingReplyPort, 5, "</br>") == false)
				{
					return;
				}
			}

			lineStart = csp;
		}
	}

	if(lineStart < cep)
	{
		if(respondingServer)
		{
			if(internetDevice->TCPSendData(respondingReplyPort, cep - lineStart, lineStart) == false)
			{
				return;
			}
		}
	}
}

uint8_t
CModule_Internet::SerialCmd_WirelessSet(
	IOutputDirector*	inOutput,
	int					inArgC,
	char const*			inArgV[])
{
	MReturnOnError(inArgC != 4, eCmd_Failed);
	MReturnOnError(strlen(inArgV[1]) > settings.ssid.GetSize(), eCmd_Failed);
	MReturnOnError(strlen(inArgV[2]) > settings.pw.GetSize(), eCmd_Failed);

	if(strcmp(inArgV[3], "wpa2") == 0)
	{
		settings.securityType = eWirelessPWEnc_WPA2Personal;
	}
	else if(strcmp(inArgV[3], "wep") == 0)
	{
		settings.securityType = eWirelessPWEnc_WEP;
	}
	else if(strcmp(inArgV[3], "open") == 0)
	{
		settings.securityType = eWirelessPWEnc_Open;
	}
	else
	{
		inOutput->printf("unknown security type %s\n", inArgV[3]);
		return eCmd_Failed;
	}

	settings.ssid = inArgV[1];
	settings.pw = inArgV[2];

	EEPROMSave();

	if(internetDevice != NULL)
	{
		internetDevice->ConnectToAP(settings.ssid, settings.pw, EWirelessPWEnc(settings.securityType));
	}

	return eCmd_Succeeded;
}
	
uint8_t
CModule_Internet::SerialCmd_WirelessGet(
	IOutputDirector*	inOutput,
	int					inArgC,
	char const*			inArgV[])
{
	static char const*	gEncList[] = {"open", "wep", "wpa2personal"};

	inOutput->printf("ssid: %s\n", (char*)settings.ssid);
	inOutput->printf("pw: %s\n", (char*)settings.pw);
	if(settings.securityType <= eWirelessPWEnc_WPA2Personal)
	{
		inOutput->printf("enc: %s\n", gEncList[settings.securityType]);
	}
	else
	{
		inOutput->printf("enc: NA\n");
	}

	return eCmd_Succeeded;
}

uint8_t
CModule_Internet::SerialCmd_IPAddrSet(
	IOutputDirector*	inOutput,
	int					inArgC,
	char const*			inArgV[])
{
	MReturnOnError(inArgC != 4, eCmd_Failed);

	int	addrByte3;
	int	addrByte2;
	int	addrByte1;
	int	addrByte0;

	sscanf(inArgV[1], "%d.%d.%d.%d", &addrByte3, &addrByte2, &addrByte1, &addrByte0);

	settings.ipAddr = (addrByte3 << 24) | (addrByte2 << 16) | (addrByte1 << 8) | addrByte0;

	sscanf(inArgV[2], "%d.%d.%d.%d", &addrByte3, &addrByte2, &addrByte1, &addrByte0);

	settings.gatewayAddr = (addrByte3 << 24) | (addrByte2 << 16) | (addrByte1 << 8) | addrByte0;

	sscanf(inArgV[3], "%d.%d.%d.%d", &addrByte3, &addrByte2, &addrByte1, &addrByte0);

	settings.subnetAddr = (addrByte3 << 24) | (addrByte2 << 16) | (addrByte1 << 8) | addrByte0;

	EEPROMSave();

	if(internetDevice != NULL)
	{
		internetDevice->SetIPAddr(settings.ipAddr, settings.gatewayAddr, settings.subnetAddr);
	}

	return eCmd_Succeeded;
}

uint8_t
CModule_Internet::SerialCmd_IPAddrGet(
	IOutputDirector*	inOutput,
	int					inArgC,
	char const*			inArgV[])
{
	inOutput->printf("ip: %d.%d.%d.%d\n", ((settings.ipAddr >> 24) & 0xFF), ((settings.ipAddr >> 16) & 0xFF), ((settings.ipAddr >> 8) & 0xFF), ((settings.ipAddr >> 0) & 0xFF));
	inOutput->printf("gateway: %d.%d.%d.%d\n", ((settings.gatewayAddr >> 24) & 0xFF), ((settings.gatewayAddr >> 16) & 0xFF), ((settings.gatewayAddr >> 8) & 0xFF), ((settings.gatewayAddr >> 0) & 0xFF));
	inOutput->printf("subnet: %d.%d.%d.%d\n", ((settings.subnetAddr >> 24) & 0xFF), ((settings.subnetAddr >> 16) & 0xFF), ((settings.subnetAddr >> 8) & 0xFF), ((settings.subnetAddr >> 0) & 0xFF));

	return eCmd_Succeeded;
}

void
CModule_Internet::CommandHomePageHandler(
	IOutputDirector*	inOutput,
	int					inParamCount,
	char const**		inParamList)
{
	// Nothing to do here
}

void
CModule_Internet::CommandProcessPageHandler(
	IOutputDirector*	inOutput,
	int					inParamCount,
	char const**		inParamList)
{
	if(inParamCount != 2 || strcmp(inParamList[0], "Command") != 0)
	{
		return;
	}

	char*		cdp = (char*)inParamList[1];
	char const*	argList[64];
	int			argIndex = 0;
	
	argList[argIndex++] = cdp;
	for(;;)
	{
		char c = *cdp++;

		if(c == 0)
		{
			break;
		}

		if(c == ' ')
		{
			cdp[-1] = 0;
			argList[argIndex++] = cdp;
			if(argIndex >= 64)
			{
				break;
			}
		}
	}

	gCommandModule->ProcessCommand(inOutput, argIndex, argList);
}

void
CModule_Internet::TransformURLIntoParameters(
	int&			ioParameterCount,
	char const**	outParameterList,
	char*&			outPageName,
	char*			inURL)
{
	int		maxCount = ioParameterCount;

	char*	dp = strchr(inURL, '?');

	outPageName = inURL;
	ioParameterCount = 0;
	if(dp == NULL)
	{
		return;
	}

	*dp++ = 0;
	inURL = dp;
	outParameterList[ioParameterCount++] = dp;

	for(;;)
	{
		char c = *inURL++;
		if(c == 0)
		{
			break;
		}

		if(c == '=' || c == '&')
		{
			*dp++ = 0;
			if(ioParameterCount >= maxCount)
			{
				break;
			}
			outParameterList[ioParameterCount++] = dp;
		}
		else if(c == '+')
		{
			*dp++ = ' ';
		}
		else if(c == '%')
		{
			char hexData[3];
			hexData[0] = *inURL++;
			if(hexData[0] == 0)
			{
				break;
			}
			hexData[1] = *inURL++;
			if(hexData[2] == 0)
			{
				break;
			}
			hexData[2] = 0;
			*dp++ = (char)strtol(hexData, NULL, 16);
		}
		else
		{
			*dp++ = c;
		}
	}

	if(ioParameterCount < maxCount)
	{
		*dp++ = 0;
	}
}
