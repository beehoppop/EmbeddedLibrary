
#include <ELInternet.h>
#include <ELAssert.h>

CModule_Internet	CModule_Internet::module;
CModule_Internet*	gInternet;

	
CModule_Internet::CModule_Internet(
	)
	:
	CModule("intn", sizeof(settings), 0, &settings, 10000)
{
	gInternet = this;
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

	cur->port = inPort;
	cur->handlerObject = inInternetHandler;
	cur->handlerMethod = inMethod;
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
	uint16_t&	outLocalPort,
	uint16_t	inServerPort,
	char const*	inServerAddress)
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

	bool success = internetDevice->Connection_Open(target->localPort, inServerPort, inServerAddress);
	MReturnOnError(success == false);

	cur->serverPort = inServerPort;
	strcpy(cur->serverAddress, inServerAddress);
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
			internetDevice->Connection_Close(inLocalPort);
			return;
		}
	}
}

void
CModule_Internet::InitiateRequest(
	uint16_t						inLocalPort,
	int								inDataSize,
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

	MReturnOnError(target == NULL);

	internetDevice->Connection_InitiateRequest(inLocalPort, inDataSize, inData);
}

void
CModule_Internet::ServeCommands(
	uint16_t	inPort)
{
	if(internetDevice == NULL)
	{
		return;
	}
	
	commandServerPort = inPort;

	internetDevice->Server_Open(inPort);
}

void
CModule_Internet::Setup(
	void)
{
	gCommand->RegisterCommand("wireless_set", this, static_cast<TCmdHandlerMethod>(&CModule_Internet::SerialCmd_WirelessSet));
	gCommand->RegisterCommand("ipaddr_set", this, static_cast<TCmdHandlerMethod>(&CModule_Internet::SerialCmd_IPAddrSet));

}

void
CModule_Internet::Update(
	uint32_t	inDeltaTimeUS)
{
	int		bufferSize;
	char	buffer[1024];

	SServer*	curServer = serverList;
	for(int i = 0; i < eMaxServersCount; ++i, ++curServer)
	{
		if(curServer->handlerObject != NULL)
		{
			bufferSize = 0;
			uint16_t	transactionPort;
			internetDevice->Server_GetData(curServer->port, transactionPort, bufferSize, buffer);
			if(bufferSize > 0)
			{
				// we got data, now call the handler
				returnBufferLen = 0;
				(curServer->handlerObject->*curServer->handlerMethod)(this, bufferSize, buffer);
				internetDevice->Server_SendData(curServer->port, transactionPort, returnBufferLen, returnBuffer);
			}
		}
	}

	SConnection*	curConnection = connectionList;
	for(int i = 0; i < eMaxConnectionsCount; ++i, ++curConnection)
	{
		if(curConnection->handlerObject != NULL)
		{
			bufferSize = 0;
			internetDevice->Connection_GetData(curConnection->localPort, bufferSize, buffer);
			if(bufferSize > 0)
			{
				(curConnection->handlerObject->*curConnection->handlerMethod)(bufferSize, buffer);
			}
		}
	}

	if(commandServerPort > 0)
	{

	}
}

void
CModule_Internet::write(
	char const* inMsg,
	size_t inBytes)
{
	if(returnBufferLen + inBytes > sizeof(returnBuffer))
	{
		inBytes = sizeof(returnBuffer) - returnBufferLen;
	}
	memcpy(returnBuffer + returnBufferLen, inMsg, inBytes);
	returnBufferLen += inBytes;
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
CModule_Internet::SerialCmd_IPAddrSet(
	IOutputDirector*	inOutput,
	int					inArgC,
	char const*			inArgV[])
{
	MReturnOnError(inArgC != 13, eCmd_Failed);

	uint8_t	addrByte3 = (uint8_t)atoi(inArgV[1]);
	uint8_t	addrByte2 = (uint8_t)atoi(inArgV[2]);
	uint8_t	addrByte1 = (uint8_t)atoi(inArgV[3]);
	uint8_t	addrByte0 = (uint8_t)atoi(inArgV[4]);

	settings.ipAddr = (addrByte3 << 24) | (addrByte2 << 16) | (addrByte1 << 8) | addrByte0;

	addrByte3 = (uint8_t)atoi(inArgV[5]);
	addrByte2 = (uint8_t)atoi(inArgV[6]);
	addrByte1 = (uint8_t)atoi(inArgV[7]);
	addrByte0 = (uint8_t)atoi(inArgV[8]);

	settings.gatewayAddr = (addrByte3 << 24) | (addrByte2 << 16) | (addrByte1 << 8) | addrByte0;

	addrByte3 = (uint8_t)atoi(inArgV[5]);
	addrByte2 = (uint8_t)atoi(inArgV[6]);
	addrByte1 = (uint8_t)atoi(inArgV[7]);
	addrByte0 = (uint8_t)atoi(inArgV[8]);

	settings.subnetAddr = (addrByte3 << 24) | (addrByte2 << 16) | (addrByte1 << 8) | addrByte0;

	EEPROMSave();

	if(internetDevice != NULL)
	{
		internetDevice->SetIPAddr(settings.ipAddr, settings.subnetAddr, settings.gatewayAddr);
	}

	return eCmd_Succeeded;
}
