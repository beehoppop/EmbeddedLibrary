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

static char const*	gCmdHomePageGet = "GET / HTTP";
static char const*	gCmdProcessPageGet = "GET /cmd_data.asp?Command=";
static char const*	gReplyStringPreOutput = 
	"HTTP/1.1 200 OK\r\n"
	"Content-Type: text/html\r\n\r\n"
	"<!DOCTYPE html><html><body>"
	"<form action=\"cmd_data.asp\">Command: <input type=\"text\" name=\"Command\"<br><input type=\"submit\" value=\"Submit\"></form><p>Click the \"Submit\" button and the command will be sent to the server.</p><code>"
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
	strncpy(serverAddress, inServer, sizeof(serverAddress) - 1);
	serverAddress[sizeof(serverAddress) - 1] = 0;
	serverPort = inPort;
	localPort = 0xFF;
	openInProgress = false;
	waitingOnResponse = false;
	flushPending = false;
	requestIndex = 0;
}

void
CHTTPConnection::StartRequest(
	char const*	inVerb,
	char const*	inURL)
{
	char buffer[128];

	MReturnOnError(waitingOnResponse);

	snprintf(buffer, sizeof(buffer), "%s %s HTTP/1.1\r\n", inVerb, inURL);
	buffer[sizeof(buffer) - 1] = 0;

	SendData(buffer, false);
}

void
CHTTPConnection::SendHeaders(
	int	inHeaderCount,
	...)
{
	char buffer[256];
	char*	cp = buffer;
	char*	ep = cp + sizeof(buffer);

	MReturnOnError(waitingOnResponse);

	va_list	valist;

	va_start(valist, inHeaderCount);
	
	for(int i = 0; i < inHeaderCount && cp < ep; ++i)
	{
		char const*	key = va_arg(valist, char const*);
		char const* value = va_arg(valist, char const*);

		strncpy(cp, key, ep - cp);
		cp += strlen(key);
		if(ep - cp > 2)
		{
			*cp++ = ':';
			*cp++ = ' ';
		}
		strncpy(cp, value, ep - cp);
		cp += strlen(value);
		if(ep - cp > 3)
		{
			*cp++ = '\r';
			*cp++ = '\n';
		}
	}
	*cp++ = 0;

	va_end(valist);

	SendData(buffer, false);
}

void
CHTTPConnection::SendParameters(
	int	inParameterCount,
	...)
{
	char buffer[256];
	char*	cp = buffer;
	char*	ep = cp + sizeof(buffer);

	MReturnOnError(waitingOnResponse);

	va_list	valist;

	va_start(valist, inParameterCount);
	
	for(int i = 0; i < inParameterCount && cp < ep; ++i)
	{
		char const*	key = va_arg(valist, char const*);
		char const* value = va_arg(valist, char const*);

		strncpy(cp, key, ep - cp);
		cp += strlen(key);
		if(ep - cp > 2)
		{
			*cp++ = ':';
			*cp++ = ' ';
		}
		strncpy(cp, value, ep - cp);
		cp += strlen(value);
		if(ep - cp > 2)
		{
			*cp++ = '\r';
			*cp++ = '\n';
		}
	}
	buffer[sizeof(buffer) - 1] = 0;

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
	SendHeaders(4, "Content-Length", itoa(bodyLen, blBuffer, 10), "User-Agent", "EmbeddedLibrary", "Host", serverAddress, "Accept", "*/*"); 

	// Add a blank line
	SendData("\r\n", false);
	SendData(inBody, true);

	waitingOnResponse = true;
	responseContentSize = 0;
	responseState = eResponseState_HTTP;
	responseHTTPCode = 0;
	responseContentIndex = 0;
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
			if(requestIndex > 0)
			{
				gInternetModule->SendData(localPort, requestIndex, buffer, flushPending);
				requestIndex = 0;
				flushPending = false;
			}
			break;

		case eConnectionResponse_Closed:
		case eConnectionResponse_Error:
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

		int	dataLen = strlen(inData);
		if(requestIndex + dataLen > (int)sizeof(buffer))
		{
			dataLen = sizeof(buffer) - requestIndex;
		}

		memcpy(buffer + requestIndex, inData, dataLen);
		requestIndex += dataLen;
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
			buffer[responseContentIndex++] = c;
			if(responseContentIndex >= responseContentSize)
			{
				FinishResponse();
				return;
			}
		}
		else
		{
			if(c == '\r' || responseContentIndex >= sizeof(buffer) - 1)
			{
				buffer[responseContentIndex++] = 0;
				ProcessResponseLine();
				responseContentIndex = 0;
				if(*cp == '\n')
				{
					++cp;
				}
				continue;
			}

			buffer[responseContentIndex++] = c;
		}
	}
}
	
void
CHTTPConnection::ProcessResponseLine(
	void)
{
	if(buffer[0] == 0)
	{
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
		if(strncmp(buffer, "HTTP", 4) == 0)
		{
			char*	codeStr = buffer;
			while(!isspace(*codeStr))
			{
				++codeStr;
			}

			while(!isdigit(*codeStr))
			{
				++codeStr;
			}
			char* endCodeStr = codeStr;
			while(isdigit(*endCodeStr))
			{
				++endCodeStr;
			}
			*endCodeStr = 0;

			responseHTTPCode = atoi(codeStr);

			responseState = eResponseState_Headers;
		}
	}
	else
	{
		char*	valuePtr = strchr(buffer, ':');
		if(valuePtr == NULL)
		{
			return;
		}
		*valuePtr = 0;
		++valuePtr;
		while(isspace(*valuePtr))
		{
			++valuePtr;
		} 

		if(strcmp(buffer, "Content-Length") == 0)
		{
			responseContentSize = atoi(valuePtr);
		}
		else
		{
			// Don't parse any other headers for now
		}
	}
}

void
CHTTPConnection::FinishResponse(
	void)
{
	responseState = eResponseState_Done;
	waitingOnResponse = false;
	(internetHandler->*responseMethod)(responseHTTPCode, responseContentSize, buffer);
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
	memset(&settings, 0, sizeof(settings));
	memset(commandServerFrontPageHandlerList, 0, sizeof(commandServerFrontPageHandlerList));
	commandServerPort = 0;
	respondingServer = false;
	respondingServerPort = 0;
	respondingReplyPort = 0;

	SConnection*	cur = connectionList;
	for(int i = 0; i < eMaxConnectionsCount; ++i, ++cur)
	{
		cur->openRef = -1;
		cur->localPort = 0xFF;
	}
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
	MReturnOnError(strlen(inServerAddress) > eServerMaxAddressLength);

	SConnection*	target = NULL;
	SConnection*	cur = connectionList;
	for(int i = 0; i < eMaxConnectionsCount; ++i, ++cur)
	{
		if(cur->serverPort == inServerPort && strcmp(cur->serverAddress, inServerAddress) == 0)
		{
			target = cur;
			break;
		}
		else if(target == NULL && cur->serverAddress[0] == 0)
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
	strcpy(target->serverAddress, inServerAddress);
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
			cur->serverAddress[0] = 0;
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
CModule_Internet::CommandServer_Start(
	uint16_t	inPort)
{
	MReturnOnError(internetDevice == NULL);
	commandServerPort = inPort;
	internetDevice->Server_Open(inPort);
}

void
CModule_Internet::CommandServer_RegisterFrontPage(
	IInternetHandler*			inInternetHandler,
	TInternetServerPageMethod	inMethod)
{
	for(int i = 0; i < eCommandServerFrontPageHandlerMax; ++i)
	{
		if(commandServerFrontPageHandlerList[i].commandServerObject == NULL)
		{
			commandServerFrontPageHandlerList[i].commandServerObject = inInternetHandler;
			commandServerFrontPageHandlerList[i].commandServerMethod = inMethod;
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

void
CModule_Internet::Setup(
	void)
{
	MReturnOnError(internetDevice == NULL);	// Internet device needs to be setup before the general internet module

	if(settings.ssid[0] != 0)
	{
		internetDevice->ConnectToAP(settings.ssid, settings.pw, EWirelessPWEnc(settings.securityType));
	}

	if(settings.ipAddr != 0)
	{
		internetDevice->SetIPAddr(settings.ipAddr, settings.subnetAddr, settings.gatewayAddr);
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

	// Check for connections that are waiting for an open completion
	SConnection*	curConnection = connectionList;
	for(int i = 0; i < eMaxConnectionsCount; ++i, ++curConnection)
	{
		if(curConnection->openRef >= 0)
		{
			bool	successfulyOpened;
			if(internetDevice->Client_OpenCompleted(curConnection->openRef, successfulyOpened, curConnection->localPort))
			{
				// call completion
				curConnection->openRef = -1;
				(curConnection->handlerObject->*curConnection->handlerResponseMethod)(eConnectionResponse_Opened, curConnection->localPort, 0, NULL);
			}
		}
	}

	bufferSize = sizeof(buffer) - 1;
	uint16_t	localPort;
	uint16_t	replyPort;
	internetDevice->GetData(localPort, replyPort, bufferSize, buffer);
	buffer[bufferSize] = 0;
	
	if(bufferSize > 0)
	{
		if(localPort == commandServerPort)
		{
			//DebugMsgLocal("Got cmd server data chn=%d", replyPort);
			if(strncmp(buffer, gCmdHomePageGet, strlen(gCmdHomePageGet)) == 0)
			{
				internetDevice->SendData(replyPort, strlen(gReplyStringPreOutput), gReplyStringPreOutput);

				respondingServer = true;
				respondingServerPort = commandServerPort;
				respondingReplyPort = replyPort;
				for(int i = 0; i < eCommandServerFrontPageHandlerMax; ++i)
				{
					if(commandServerFrontPageHandlerList[i].commandServerObject != NULL)
					{
						(commandServerFrontPageHandlerList[i].commandServerObject->*(commandServerFrontPageHandlerList[i].commandServerMethod))(this);
					}
				}

				internetDevice->SendData(replyPort, strlen(gReplyStringPostOutput), gReplyStringPostOutput);
				internetDevice->CloseConnection(replyPort);
			}
			else if(strncmp(buffer, gCmdProcessPageGet, strlen(gCmdProcessPageGet)) == 0)
			{
				char*	httpStr = strrstr(buffer, "HTTP/1.1");
				MReturnOnError(httpStr == NULL);
				--httpStr;
				*httpStr = 0;

				char*	csp = buffer + strlen(gCmdProcessPageGet);
				//SystemMsg(eMsgLevel_Always, "Parsing Command: %s", csp);

				char*		cdp = buffer;
				char const*	argList[64];
				int			argIndex = 0;

				argList[argIndex++] = cdp;
				while(csp < httpStr)
				{
					char c = *csp++;

					if(c == '+')
					{
						*cdp++ = 0;
						argList[argIndex++] = cdp;
					}
					else if(c == '%')
					{
						char numBuffer[3];
						numBuffer[0] = csp[0];
						numBuffer[1] = csp[1];
						numBuffer[2] = 0;
						*cdp++ = (char)strtol(numBuffer, NULL, 16);
						csp += 2;
					}
					else
					{
						*cdp++ = c;
					}
				}
				*cdp++ = 0;

				internetDevice->SendData(replyPort, strlen(gReplyStringPreOutput), gReplyStringPreOutput);

				respondingServer = true;
				respondingServerPort = commandServerPort;
				respondingReplyPort = replyPort;

				// Call the front page handlers to provide html
				gCommandModule->ProcessCommand(this, argIndex, argList);

				for(int i = 0; i < eCommandServerFrontPageHandlerMax; ++i)
				{
					if(commandServerFrontPageHandlerList[i].commandServerObject != NULL)
					{
						(commandServerFrontPageHandlerList[i].commandServerObject->*(commandServerFrontPageHandlerList[i].commandServerMethod))(this);
					}
				}

				internetDevice->SendData(replyPort, strlen(gReplyStringPostOutput), gReplyStringPostOutput);
				internetDevice->CloseConnection(replyPort);
			}
			else
			{
				//DebugMsgLocal("  unknown cmd request");
				internetDevice->CloseConnection(replyPort);
			}
		}
		else
		{
			// look for the server or client connection the data came back on
			SServer*	curServer = serverList;
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
					break;
				}
			}
		}
	}

	curConnection = connectionList;
	for(int i = 0; i < eMaxConnectionsCount; ++i, ++curConnection)
	{
		if(curConnection->handlerObject != NULL)
		{
			uint32_t	portState = internetDevice->GetPortState(curConnection->localPort);

			if(bufferSize > 0 && curConnection->localPort == localPort)
			{
				(curConnection->handlerObject->*curConnection->handlerResponseMethod)(eConnectionResponse_Data, curConnection->localPort, bufferSize, buffer);
			}
			else if(curConnection->localPort != 0xFF)
			{
				if(portState & ePortState_Failure)
				{
					(curConnection->handlerObject->*curConnection->handlerResponseMethod)(eConnectionResponse_Error, curConnection->localPort, 0, NULL);
					if(curConnection->localPort != 0xFF)
					{
						internetDevice->CloseConnection(curConnection->localPort);
						curConnection->handlerObject = NULL;
					}
				}

				if(curConnection->openRef < 0 && !(portState & ePortState_IsOpen))
				{
					(curConnection->handlerObject->*curConnection->handlerResponseMethod)(eConnectionResponse_Closed, curConnection->localPort, 0, NULL);
					if(curConnection->localPort != 0xFF)
					{
						internetDevice->CloseConnection(curConnection->localPort);
						curConnection->handlerObject = NULL;
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
			char const*	lineEnd = csp;
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
			//internetDevice->Server_SendData(respondingServerPort, respondingTransactionPort, 5, "</br>");
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

	strcpy(settings.ssid, inArgV[1]);
	strcpy(settings.pw, inArgV[2]);

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

	inOutput->printf("ssid: %s\n", settings.ssid);
	inOutput->printf("pw: %s\n", settings.pw);
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
		internetDevice->SetIPAddr(settings.ipAddr, settings.subnetAddr, settings.gatewayAddr);
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
