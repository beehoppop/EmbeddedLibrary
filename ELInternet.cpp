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
	localPort = 0xFF;
	openInProgress = false;
	waitingOnResponse = false;
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
	char const*	inURL)
{
	TString<256> buffer("%s %s HTTP/1.1\r\n", inVerb, inURL);

	MReturnOnError(waitingOnResponse);

	SendData(buffer, false);
}

void
CHTTPConnection::SendHeaders(
	int	inHeaderCount,
	...)
{
	TString<256> buffer;

	MReturnOnError(waitingOnResponse);

	va_list	valist;

	va_start(valist, inHeaderCount);
	
	for(int i = 0; i < inHeaderCount; ++i)
	{
		char const*	key = va_arg(valist, char const*);
		char const* value = va_arg(valist, char const*);

		buffer.Append("%s: %s\r\n", key, value);
	}

	va_end(valist);

	SendData(buffer, false);
}

void
CHTTPConnection::SendParameters(
	int	inParameterCount,
	...)
{
	TString<256> buffer;

	MReturnOnError(waitingOnResponse);

	va_list	valist;

	va_start(valist, inParameterCount);
	
	for(int i = 0; i < inParameterCount; ++i)
	{
		char const*	key = va_arg(valist, char const*);
		char const* value = va_arg(valist, char const*);

		buffer.Append("%s: %s\r\n", key, value);
	}

	va_end(valist);

	SendBody(buffer);
}

void
CHTTPConnection::SendBody(
	char const*	inBody)
{
	MReturnOnError(waitingOnResponse);

	int	bodyLen = strlen(inBody);
	char blBuffer[32];

	// Add the Content-Length header
	SendHeaders(4, "Content-Length", itoa(bodyLen, blBuffer, 10), "User-Agent", "EmbeddedLibrary", "Host", (char*)serverAddress, "Accept", "*/*"); 

	// Add a blank line
	SendData("\r\n", false);

	// Send the body
	SendData(inBody, true);

	waitingOnResponse = true;
	responseContentSize = 0;
	responseState = eResponseState_HTTP;
	responseHTTPCode = 0;
	dataSentTimeMS = millis();
}

void
CHTTPConnection::Close(
	void)
{
	if(localPort != 0)
	{
		gInternetModule->CloseConnection(localPort);
	}

	delete this;
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
				gInternetModule->SendData(localPort, tempBuffer.GetLength(), tempBuffer, flushPending);
				tempBuffer.Clear();
				flushPending = false;
				dataSentTimeMS = millis();
			}
			break;

		case eConnectionResponse_Closed:
		case eConnectionResponse_Error:
			if(waitingOnResponse)
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

			gInternetModule->CloseConnection(localPort);
			openInProgress = false;
			waitingOnResponse = false;
			localPort = 0xFF;
			break;

		case eConnectionResponse_Data:
			if(waitingOnResponse)
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
	if(localPort != 0xFF)
	{
		gInternetModule->SendData(localPort, strlen(inData), inData, inFlush);
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
		// If the response line is empty that means that response body is starting
		if(responseContentSize > 0)
		{
			responseState = eResponseState_Body;
		}
		else
		{
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
	responseState = eResponseState_Done;
	waitingOnResponse = false;
	(internetHandler->*responseMethod)(responseHTTPCode, responseContentSize, tempBuffer);
	tempBuffer.Clear();
}

void
CHTTPConnection::CheckForTimeoutEvent(
	TRealTimeEventRef	inRef,
	void*				inRefCon)
{
	if(waitingOnResponse && (millis() - dataSentTimeMS >= eHTTPTimeoutMS))
	{
		// we have timed out waiting for a response
		responseHTTPCode = 500;
		FinishResponse();
	}

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
	memset(connectionList, 0, sizeof(connectionList));
	memset(webServerPageHandlerList, 0, sizeof(webServerPageHandlerList));
	webServerPort = 0;
	respondingServer = false;
	respondingServerPort = 0;
	respondingReplyPort = 0;

	SConnection*	cur = connectionList;
	for(int i = 0; i < eMaxConnectionsCount; ++i, ++cur)
	{
		cur->openRef = -1;
		cur->localPort = 0xFF;
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
CModule_Internet::OpenConnection(
	uint16_t								inServerPort,
	char const*								inServerAddress,
	IInternetHandler*						inInternetHandler,
	TInternetResponseHandlerMethod			inResponseMethod)
{
	MReturnOnError(internetDevice == NULL);
	MReturnOnError(strlen(inServerAddress) > eServerMaxAddressLength - 1);

	SConnection*	target = NULL;
	SConnection*	cur = connectionList;
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

	target->openRef = internetDevice->Client_RequestOpen(inServerPort, inServerAddress);
	if(target->openRef < 0)
	{
		(inInternetHandler->*inResponseMethod)(eConnectionResponse_Error, 0, 0, NULL);
		return;
	}

	target->handlerObject = inInternetHandler;
	target->handlerResponseMethod = inResponseMethod;
	target->serverPort = inServerPort;
	target->serverAddress = inServerAddress;
	target->localPort = 0xFF;
}

void
CModule_Internet::CloseConnection(
	uint16_t	inLocalPort)
{
	SConnection*	cur = connectionList;
	for(int i = 0; i < eMaxConnectionsCount; ++i, ++cur)
	{
		if(cur->localPort == inLocalPort)
		{
			cur->localPort = 0xFF;
			cur->serverPort = 0;
			cur->serverAddress.Clear();
			cur->handlerResponseMethod = NULL;
			cur->handlerObject = NULL;
			cur->openRef = -1;
			internetDevice->CloseConnection(inLocalPort);
			return;
		}
	}
}

bool
CModule_Internet::SendData(
	uint16_t						inLocalPort,
	size_t							inDataSize,
	char const*						inData,
	bool							inFlush)
{
	SConnection*	target = NULL;
	SConnection*	curConnection = connectionList;
	for(int i = 0; i < eMaxConnectionsCount; ++i, ++curConnection)
	{
		if(curConnection->localPort == inLocalPort)
		{
			target = curConnection;
			break;
		}
	}

	MReturnOnError(target == NULL || target->openRef > 0, false);

	if(internetDevice->SendData(inLocalPort, inDataSize, inData, inFlush) == false)
	{
		CloseConnection(inLocalPort);
		return false;
	}

	return true;
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
	char	buffer[eMaxIncomingPacketSize];

	if(internetDevice == NULL)
	{
		return;
	}

	SConnection*	curConnection;

	// Update the current connections
	curConnection = connectionList;
	for(int i = 0; i < eMaxConnectionsCount; ++i, ++curConnection)
	{
		if(curConnection->handlerObject == NULL)
		{
			continue;
		}

		if(curConnection->openRef >= 0)
		{
			bool	successfulyOpened;
			if(internetDevice->Client_OpenCompleted(curConnection->openRef, successfulyOpened, curConnection->localPort))
			{
				// call completion
				curConnection->openRef = -1;
				if(successfulyOpened)
				{
					(curConnection->handlerObject->*curConnection->handlerResponseMethod)(eConnectionResponse_Opened, curConnection->localPort, 0, NULL);
				}
				else
				{
					(curConnection->handlerObject->*curConnection->handlerResponseMethod)(eConnectionResponse_Error, 0, 0, NULL);

					// Empty out the connection since another OpenConnection will be required to use this slot
					curConnection->serverAddress.Clear();
					curConnection->handlerObject = NULL;
				}
			}
			continue;
		}

		MAssert(curConnection->localPort != 0xFF);

		uint32_t	portState = internetDevice->GetPortState(curConnection->localPort);

		// If the port has failed or closed report that
		if(portState & ePortState_Failure)
		{
			(curConnection->handlerObject->*curConnection->handlerResponseMethod)(eConnectionResponse_Error, curConnection->localPort, 0, NULL);
			// Since the method above may have already closed the port we need to check for that
			if(curConnection->localPort != 0xFF)
			{
				CloseConnection(curConnection->localPort);
			}
		}
		else if(!(portState & ePortState_IsOpen))
		{
			(curConnection->handlerObject->*curConnection->handlerResponseMethod)(eConnectionResponse_Closed, curConnection->localPort, 0, NULL);
			// Since the method above may have already closed the port we need to check for that
			if(curConnection->localPort != 0xFF)
			{
				CloseConnection(curConnection->localPort);
			}
		}
	}

	// Check for incoming data
	bufferSize = sizeof(buffer) - 1;
	uint16_t	localPort;
	uint16_t	replyPort;
	internetDevice->GetData(localPort, replyPort, bufferSize, buffer);
	buffer[bufferSize] = 0;
	
	if(bufferSize > 0)
	{
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

				internetDevice->SendData(replyPort, strlen(gReplyStringPreOutput), gReplyStringPreOutput);

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

				internetDevice->SendData(replyPort, strlen(gReplyStringPostOutput), gReplyStringPostOutput);
				internetDevice->CloseConnection(replyPort);
			}
			else
			{
				Serial.printf("  unknown cmd request");
				internetDevice->CloseConnection(replyPort);
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
					(curServer->handlerObject->*curServer->handlerMethod)(this, bufferSize, buffer);
					internetDevice->CloseConnection(replyPort);
					dataProcessed = true;
					break;
				}
			}

			if(!dataProcessed)
			{
				// Look for a client connection
				curConnection = connectionList;
				for(int i = 0; i < eMaxConnectionsCount; ++i, ++curConnection)
				{
					if(curConnection->handlerObject != NULL && curConnection->localPort == localPort)
					{
						// Call the response handler with the data
						(curConnection->handlerObject->*curConnection->handlerResponseMethod)(eConnectionResponse_Data, curConnection->localPort, bufferSize, buffer);
					}

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

	uint32_t	portState = internetDevice->GetPortState(respondingReplyPort);
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
				if(internetDevice->SendData(respondingReplyPort, lineEnd - lineStart, lineStart) == false)
				{
					return;
				}

				if(internetDevice->SendData(respondingReplyPort, 5, "</br>") == false)
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
			if(internetDevice->SendData(respondingReplyPort, cep - lineStart, lineStart) == false)
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
	MReturnOnError(strlen(inArgV[1]) > sizeof(settings.ssid) - 1, eCmd_Failed);
	MReturnOnError(strlen(inArgV[2]) > sizeof(settings.pw) - 1, eCmd_Failed);

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
