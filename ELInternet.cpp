
#include <ELInternet.h>
#include <ELAssert.h>
#include <ELUtilities.h>
#include <ELCommand.h>

CModule_Internet	CModule_Internet::module;
CModule_Internet*	gInternet;

static char const*	gCmdHomePageGet = "GET / HTTP";
static char const*	gCmdProcessPageGet = "GET /cmd_data.asp?Command=";
static char const*	gReplyStringPreOutput = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<!DOCTYPE html><html><body><form action=\"cmd_data.asp\">Command: <input type=\"text\" name=\"Command\"<br><input type=\"submit\" value=\"Submit\"></form><p>Click the \"Submit\" button and the command will be sent to the server.</p><code>";
static char const*	gReplyStringPostOutput = "</code></body></html>";

CModule_Internet::CModule_Internet(
	)
	:
	CModule("intn", sizeof(settings), 0, &settings, 10000)
{
	gInternet = this;

	SConnection*	cur = connectionList;
	for(int i = 0; i < eMaxConnectionsCount; ++i, ++cur)
	{
		cur->openRef = -1;
	}
}

void
CModule_Internet::SetInternetDevice(
	IInternetDevice*	inInternetDevice)
{
	internetDevice = inInternetDevice;

	if(settings.ssid[0] != 0)
	{
		internetDevice->ConnectToAP(settings.ssid, settings.pw, EWirelessPWEnc(settings.securityType));
	}

	if(settings.ipAddr != 0)
	{
		internetDevice->SetIPAddr(settings.ipAddr, settings.subnetAddr, settings.gatewayAddr);
	}
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
	TInternetOpenConnectionHandlerMethod	inMethod)
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
	if(target->openRef >= 0)
	{
		target->handlerObject = inInternetHandler;
		target->handlerConnectionMethod = inMethod;
		target->serverPort = inServerPort;
		strcpy(target->serverAddress, inServerAddress);
	}
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
			cur->localPort = 0;
			cur->serverPort = 0;
			cur->serverAddress[0] = 0;
			cur->handlerConnectionMethod = NULL;
			cur->handlerResponseMethod = NULL;
			cur->handlerObject = NULL;
			cur->openRef = -1;
			internetDevice->CloseConnection(inLocalPort);
			return;
		}
	}
}

bool
CModule_Internet::InitiateRequest(
	uint16_t						inLocalPort,
	size_t							inDataSize,
	char const*						inData,
	IInternetHandler*				inInternetHandler,
	TInternetResponseHandlerMethod	inMethod)
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

	MReturnOnError(target == NULL || target->openRef < 0, false);

	target->handlerObject = inInternetHandler;
	target->handlerResponseMethod = inMethod;

	if(internetDevice->SendData(inLocalPort, inDataSize, inData, true) == false)
	{
		CloseConnection(inLocalPort);
		return false;
	}

	return true;
}

void
CModule_Internet::ServeCommands(
	uint16_t	inPort)
{
	MReturnOnError(internetDevice == NULL);
	commandServerPort = inPort;
	internetDevice->Server_Open(inPort);
}

void
CModule_Internet::Setup(
	void)
{
	gCommand->RegisterCommand("wireless_set", this, static_cast<TCmdHandlerMethod>(&CModule_Internet::SerialCmd_WirelessSet));
	gCommand->RegisterCommand("wireless_get", this, static_cast<TCmdHandlerMethod>(&CModule_Internet::SerialCmd_WirelessGet));
	gCommand->RegisterCommand("ipaddr_set", this, static_cast<TCmdHandlerMethod>(&CModule_Internet::SerialCmd_IPAddrSet));
	gCommand->RegisterCommand("ipaddr_get", this, static_cast<TCmdHandlerMethod>(&CModule_Internet::SerialCmd_IPAddrGet));
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
				(curConnection->handlerObject->*curConnection->handlerConnectionMethod)(successfulyOpened, curConnection->localPort);
				curConnection->openRef = -1;
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
				//DebugMsg(eDbgLevel_Always, "Parsing Command: %s", csp);

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

				// Call command
				gCommand->ProcessCommand(this, argIndex, argList);

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

			if((portState & ePortState_Failure))
			{
				(curConnection->handlerObject->*curConnection->handlerResponseMethod)(curConnection->localPort, 0, NULL);
				internetDevice->CloseConnection(curConnection->localPort);
				curConnection->handlerObject = NULL;
			}
			else if(curConnection->localPort == localPort && bufferSize > 0)
			{
				(curConnection->handlerObject->*curConnection->handlerResponseMethod)(curConnection->localPort, bufferSize, buffer);
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
