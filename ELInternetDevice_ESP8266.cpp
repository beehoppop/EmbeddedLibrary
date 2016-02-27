
#include <limits.h>

#include <ELModule.h>
#include <ELAssert.h>
#include <ELInternet.h>
#include <ELUtilities.h>
#include <ELInternetDevice_ESP8266.h>

enum
{
	eMaxLinks = 5
};

class CModule_ESP8266 : public CModule, public IInternetDevice
{
public:

	CModule_ESP8266(
		)
		:
		CModule("8266", 0, 0, NULL, 1000, 2, false)
	{
	}

	virtual void
	Setup(
		void)
	{
		MReturnOnError(serialPort == NULL);
		serialPort->begin(115200);

		if(rstPin != 0xFF)
		{
			pinMode(rstPin, OUTPUT);
			delay(10);	// This is a bit of a mystery but 4 hours of experimentation says that the device will not work without this delay right exactly here
			digitalWriteFast(rstPin, 1);
		}

		if(chPDPin != 0xFF)
		{
			pinMode(chPDPin, OUTPUT);
			digitalWriteFast(chPDPin, 1);
		}

		if(gpio0Pin != 0xFF)
		{
			pinMode(gpio0Pin, OUTPUT);
			digitalWriteFast(gpio0Pin, 1);
		}
		if(gpio2Pin != 0xFF)
		{
			pinMode(gpio2Pin, OUTPUT);
			digitalWriteFast(gpio2Pin, 1);
		}

		while(ready == false)
		{
			Update(0);
		}

		memset(links, 0, sizeof(links));

		IssueCommand("ATE0", 5000);
		WaitCommandCompleted();

		IssueCommand("AT+CWMODE=3", 5000);
		WaitCommandCompleted();

		IssueCommand("AT+CIPMUX=1", 5000);
		WaitCommandCompleted();
	}

	virtual void
	Update(
		uint32_t	inDeltaTimeUS)
	{
		size_t	bytesAvailable = serialPort->available();
		char	tmpBuffer[256];
		SLink*	targetLink;

		if(bytesAvailable == 0)
		{
			return;
		}

		bytesAvailable = MMin(bytesAvailable, sizeof(tmpBuffer));
		serialPort->readBytes(tmpBuffer, bytesAvailable);

		for(size_t i = 0; i < bytesAvailable; ++i)
		{
			char c = tmpBuffer[i];

			if(curIDPLink >= 0)
			{
				targetLink = links + curIDPLink;
				targetLink->buffer[targetLink->bufferIndex++] = c;
				if(targetLink->bufferIndex >= targetLink->totalBytes)
				{
					//DebugMsg(eDbgLevel_Always, "got all data %d %d", curIDPLink, targetLink->totalBytes);
					// we have all the ipd data now
					curIDPLink = -1;
				}
				continue;
			}

			if(ipdParsing)
			{
				if(c == ':')
				{
					commandResultBuffer[commandResultLength++] = 0;
					int	intPDBBytes;
					sscanf(commandResultBuffer, "%d,%d", &curIDPLink, &intPDBBytes);

					//DebugMsg(eDbgLevel_Always, "IPD %d %d", curIDPLink, intPDBBytes);

					targetLink = links + curIDPLink;
					targetLink->bufferIndex = 0;
					targetLink->totalBytes = (size_t)intPDBBytes;

					if(targetLink->inUse)
					{
						// This must be a client connection
						MAssert(targetLink->isServer == false);
					}
					else
					{
						// this is a new packet coming in for the server
						targetLink->isServer = true;
						targetLink->inUse = true;
					}

					ipdParsing = false;
					commandResultLength = 0;
				}
				else
				{
					commandResultBuffer[commandResultLength++] = c;
				}

				continue;
			}

			if(c == '\n' || c == '\r')
			{
				if(commandResultLength > 0)
				{
					commandResultBuffer[commandResultLength] = 0;
					commandResultLength = 0;

					ProcessInputResponse(commandResultBuffer);
				}
			}
			else
			{
				if(commandResultLength < sizeof(commandResultBuffer) - 1)
				{
					commandResultBuffer[commandResultLength++] = c;

					if(commandResultLength == 5 && strncmp(commandResultBuffer, "+IPD,", 5) == 0)
					{
						ipdParsing = true;
						commandResultLength = 0;
					}
				}
			}
		}
	}

	void
	ProcessInputResponse(
		char*	inCmd)
	{
		//DebugMsgLocal(eDbgLevel_Always, "Response: %s", inCmd);

		if(strcmp(inCmd, "OK") == 0 || strcmp(inCmd, "SEND OK") == 0)
		{
			commandSucceeded = true;
			commandPending = false;
		}
		else if(strcmp(inCmd, "ERROR") == 0)
		{
			commandSucceeded = false;
			commandPending = false;
		}
		else if(strcmp(inCmd, "ready") == 0)
		{
			ready = true;
		}
	}

	virtual void
	ConnectToAP(
		char const*		inSSID,
		char const*		inPassword,
		EWirelessPWEnc	inPasswordEncryption)
	{
		IssueCommand("AT+CWJAP=\"%s\",\"%s\"", 15000, inSSID, inPassword);
		WaitCommandCompleted();
	}

	virtual void
	SetIPAddr(
		uint32_t	inIPAddr,
		uint32_t	inSubnetAddr,
		uint32_t	inGatewayAddr)
	{
		IssueCommand("AT+CIPSTA_CUR=\"%d.%d.%d.%d\",\"%d.%d.%d.%d\",\"%d.%d.%d.%d\"", 15000, 
			(inIPAddr >> 24) & 0xFF,
			(inIPAddr >> 16) & 0xFF,
			(inIPAddr >> 8) & 0xFF,
			(inIPAddr >> 0) & 0xFF,
			(inGatewayAddr >> 24) & 0xFF,
			(inGatewayAddr >> 16) & 0xFF,
			(inGatewayAddr >> 8) & 0xFF,
			(inGatewayAddr >> 0) & 0xFF,
			(inSubnetAddr >> 24) & 0xFF,
			(inSubnetAddr >> 16) & 0xFF,
			(inSubnetAddr >> 8) & 0xFF,
			(inSubnetAddr >> 0) & 0xFF
			);
		WaitCommandCompleted();
	}

	virtual bool
	Server_Open(
		uint16_t	inServerPort)
	{
		IssueCommand("AT+CIPSERVER=1,%d", 10000, inServerPort);
		WaitCommandCompleted();
		return true;
	}

	virtual void
	Server_Close(
		uint16_t	inServerPort)
	{
		IssueCommand("AT+CIPSERVER=0,%d", 10000, inServerPort);
		WaitCommandCompleted();
	}

	virtual void
	Server_GetData(
		uint16_t	inServerPort,	
		uint16_t&	outTransactionPort,	
		size_t&		ioBufferSize,		
		char*		outBuffer)
	{
		SLink*	curLink = links;

		for(int i = 0; i < eMaxLinks; ++i, ++curLink)
		{
			if(!curLink->inUse || !curLink->isServer || curLink->totalBytes == 0 || curLink->bufferIndex < curLink->totalBytes)
			{
				continue;
			}

			//DebugMsg(eDbgLevel_Always, "GetData %d %d", i, curLink->totalBytes);

			ioBufferSize = MMin(ioBufferSize, curLink->totalBytes);
			memcpy(outBuffer, curLink->buffer, ioBufferSize);
			outTransactionPort = i;
			curLink->totalBytes = 0;
			curLink->bufferIndex = 0;
			curLink->inUse = false;
			curLink->isServer = false;
			return;
		}

		ioBufferSize = 0;
	}

	virtual void
	Server_SendData(
		uint16_t	inServerPort,	
		uint16_t	inTransactionPort,	
		size_t		inBufferSize,
		char const*	inBuffer)
	{
		SLink*	targetLink = links + inTransactionPort;
		
		while(inBufferSize > 0)
		{
			int	bytesToCopy = MMin(inBufferSize, sizeof(targetLink->buffer) - targetLink->bufferIndex);
			memcpy(targetLink->buffer + targetLink->bufferIndex, inBuffer, bytesToCopy);
			targetLink->bufferIndex += bytesToCopy;
			if(targetLink->bufferIndex >= (int)sizeof(targetLink->buffer))
			{
				TransmitPendingData(inTransactionPort);
			}
			inBufferSize -= bytesToCopy;
			inBuffer += bytesToCopy;
		}
	}

	virtual void
	Server_CloseConnection(
		uint16_t	inServerPort,
		uint16_t	inTransactionPort)
	{
		SLink*	targetLink = links + inTransactionPort;
		targetLink->inUse = false;

		TransmitPendingData(inTransactionPort);
		IssueCommand("AT+CIPCLOSE=%d", 5000, inTransactionPort);
		WaitCommandCompleted();
	}

	void
	TransmitPendingData(
		uint16_t	inTransactionPort)
	{
		SLink*	targetLink = links + inTransactionPort;
		if(targetLink->bufferIndex == 0)
		{
			return;
		}

		IssueCommand("AT+CIPSEND=%d,%d", 5000, inTransactionPort, targetLink->bufferIndex);
		while(serialPort->available() == 0 || serialPort->read() != '>')
		{
			delay(1);
		}
		serialPort->write((uint8_t*)targetLink->buffer, targetLink->bufferIndex);
		WaitCommandCompleted();
		targetLink->bufferIndex = 0;
	}

	virtual bool
	Connection_Open(
		uint16_t&	outLocalPort,
		uint16_t	inRemoteServerPort,	
		char const*	inRemoteServerAddress)
	{
		SLink*	curLink = links;

		for(int i = 0; i < eMaxLinks; ++i, ++curLink)
		{
			if(curLink->inUse)
			{
				continue;
			}

			outLocalPort = i;

			IssueCommand("AT+CIPSTART=%d,\"TCP\",\"%s\",%d", 10000, i, inRemoteServerAddress, inRemoteServerPort);
			WaitCommandCompleted();

			curLink->inUse = true;
			curLink->isServer = false;

			return true;
		}

		return false;
	}

	virtual void
	Connection_Close(
		uint16_t	inLocalPort)
	{
		SLink*	targetLink = links + inLocalPort;

		targetLink->inUse = false;
		IssueCommand("AT+CIPCLOSE=%d", 5000, inLocalPort);
		WaitCommandCompleted();

	}
	
	virtual void
	Connection_InitiateRequest(
		uint16_t	inLocalPort,
		size_t		inDataSize,
		char const*	inData)
	{
		IssueCommand("AT+CIPSEND=%d,%d", 5000, inLocalPort, inDataSize);
		while(serialPort->available() == 0 || serialPort->read() != '>')
		{
			delay(1);
		}
		serialPort->write((uint8_t*)inData, inDataSize);
		WaitCommandCompleted();
	}

	virtual void
	Connection_GetData(
		uint16_t	inPort,
		size_t&		ioBufferSize,
		char*		outBuffer)
	{
		SLink*	targetLink = links + inPort;

		if(targetLink->inUse && !targetLink->isServer && targetLink->totalBytes > 0 && targetLink->bufferIndex >= targetLink->totalBytes)
		{
			ioBufferSize = MMin(ioBufferSize, targetLink->totalBytes);
			memcpy(outBuffer, targetLink->buffer, ioBufferSize);
			targetLink->totalBytes = 0;
			targetLink->bufferIndex = 0;
			return;
		}

		ioBufferSize = 0;
	}
	
	void
	Configure(
		HardwareSerial*	inSerialPort,
		uint8_t	inRstPin,
		uint8_t	inChPDPin,
		uint8_t	inGPIO0,
		uint8_t	inGPIO2)
	{
		serialPort = inSerialPort;
		rstPin = inRstPin;
		chPDPin = inChPDPin;
		gpio0Pin = inGPIO0;
		gpio2Pin = inGPIO2;
	}

	void
	IssueCommand(
		char const*	inCommand,
		uint32_t	inTimeoutMS,
		...)
	{
		char vabuffer[256];
		va_list	varArgs;

		va_start(varArgs, inTimeoutMS);
		vsnprintf(vabuffer, sizeof(vabuffer) - 1, inCommand, varArgs);
		vabuffer[sizeof(vabuffer) - 1] = 0;	// Ensure a valid string
		va_end(varArgs);

		//DebugMsgLocal(eDbgLevel_Always, "Command: %s\n", vabuffer);
		serialPort->write(vabuffer);
		serialPort->write("\r\n");
		commandPending = true;
		commandTimeoutMS = millis() + inTimeoutMS;
	}

	void
	WaitCommandCompleted(
		void)
	{
		while(commandPending && millis() < commandTimeoutMS)
		{
			delay(1);
			Update(0);
		}

		if(commandPending)
		{
			DebugMsg(eDbgLevel_Always, "Cmd FAIL");
			commandPending = false;
			commandSucceeded = false;
			commandResultBuffer[0] = 0;
		}
	}

	struct SLink
	{
		bool	inUse;
		bool	isServer;
		char	buffer[eMaxPacketSize];
		size_t	bufferIndex;
		size_t	totalBytes;
	};

	HardwareSerial*	serialPort;
	uint8_t	rstPin;
	uint8_t	chPDPin;
	uint8_t	gpio0Pin;
	uint8_t	gpio2Pin;

	int			curIDPLink;
	bool		ipdParsing;

	bool		ready;

	bool		commandPending;
	bool		commandSucceeded;
	uint64_t	commandTimeoutMS;
	size_t		commandResultLength;
	char		commandResultBuffer[256];

	SLink	links[eMaxLinks];

	static CModule_ESP8266	module;
};
CModule_ESP8266	CModule_ESP8266::module;

IInternetDevice*
GetInternetDevice_ESP8266(
	HardwareSerial*	inSerialPort,
	uint8_t	inRstPin,
	uint8_t	inChPDPin,
	uint8_t	inGPIO0,
	uint8_t	inGPIO2)
{
	CModule_ESP8266::module.Configure(inSerialPort, inRstPin, inChPDPin, inGPIO0, inGPIO2);
	CModule_ESP8266::module.SetEnabledState(true);
	return &CModule_ESP8266::module;
}
