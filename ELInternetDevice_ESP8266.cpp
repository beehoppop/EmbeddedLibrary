
#include <limits.h>

#include <ELModule.h>
#include <ELAssert.h>
#include <ELInternet.h>
#include <ELUtilities.h>
#include <ELInternetDevice_ESP8266.h>

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

		IssueCommand("ATE0", 5000);
		WaitCommandCompleted();

		IssueCommand("AT+CWMODE=3", 5000);
		WaitCommandCompleted();
	}

	virtual void
	Update(
		uint32_t	inDeltaTimeUS)
	{
		size_t	bytesAvailable = serialPort->available();
		char	tmpBuffer[256];

		if(bytesAvailable == 0)
		{
			return;
		}

		bytesAvailable = MMin(bytesAvailable, sizeof(tmpBuffer));
		serialPort->readBytes(tmpBuffer, bytesAvailable);

		for(size_t i = 0; i < bytesAvailable; ++i)
		{
			char c = tmpBuffer[i];

			SIPDBuffer*	targetIPDBuffer = ipdBuffer + ipdIndex;
			if(targetIPDBuffer->totalBytes > 0)
			{
				targetIPDBuffer->buffer[targetIPDBuffer->bufferIndex++] = c;
				if(targetIPDBuffer->bufferIndex >= targetIPDBuffer->totalBytes)
				{
					// we have all the ipd data now
					ipdIndex = !ipdIndex;
					ipdBuffer[ipdIndex].bufferIndex = 0;
					ipdBuffer[ipdIndex].totalBytes = 0;

					commandResultLength = 0;
				}
				continue;
			}

			if(ipdParsing)
			{
				if(c == ':')
				{
					commandResultBuffer[commandResultLength++] = 0;
					int	channel, intPDBBytes;
					sscanf(commandResultBuffer, "%d,%d", &channel, &intPDBBytes);

					ipdBuffer[ipdIndex].bufferIndex = 0;
					ipdBuffer[ipdIndex].totalBytes = (size_t)intPDBBytes;
					ipdBuffer[ipdIndex].channel = channel;

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
		IssueCommand("AT+CIPMUX=1", 5000);
		WaitCommandCompleted();
		IssueCommand("AT+CIPSERVER=1,%d", 10000, inServerPort);
		WaitCommandCompleted();
		return true;
	}

	virtual void
	Server_Close(
		uint16_t	inServerPort)
	{

	}

	virtual void
	Server_GetData(
		uint16_t	inServerPort,	
		uint16_t&	outTransactionPort,	
		size_t&		ioBufferSize,		
		char*		outBuffer)
	{
		SIPDBuffer*	targetIPDBuffer = ipdBuffer + !ipdIndex;

		if(targetIPDBuffer->totalBytes > 0 && targetIPDBuffer->totalBytes == targetIPDBuffer->bufferIndex)
		{
			ioBufferSize = MMin(ioBufferSize, targetIPDBuffer->totalBytes);
			memcpy(outBuffer, targetIPDBuffer->buffer, ioBufferSize);
			outTransactionPort = targetIPDBuffer->channel;
			targetIPDBuffer->totalBytes = 0;
			targetIPDBuffer->bufferIndex = 0;
		}
		else
		{
			ioBufferSize = 0;
		}
	}

	virtual void
	Server_SendData(
		uint16_t	inServerPort,	
		uint16_t	inTransactionPort,	
		size_t		inBufferSize,
		char const*	inBuffer)
	{
		IssueCommand("AT+CIPSEND=%d,%d", 5000, inTransactionPort, inBufferSize);
		while(serialPort->available() == 0 || serialPort->read() != '>')
		{
			delay(1);
		}
		serialPort->write((uint8_t*)inBuffer, inBufferSize);
		WaitCommandCompleted();
	}

	virtual void
	Server_CloseConnection(
		uint16_t	inServerPort,
		uint16_t	inTransactionPort)
	{
		IssueCommand("AT+CIPCLOSE=%d", 5000, inTransactionPort);
		WaitCommandCompleted();
	}

	virtual bool
	Connection_Open(
		uint16_t&	outLocalPort,
		uint16_t	inRemoteServerPort,	
		char const*	inRemoteServerAddress)
	{
		return false;
	}

	virtual void
	Connection_Close(
		uint16_t	inLocalPort)
	{

	}
	
	virtual void
	Connection_InitiateRequest(
		uint16_t	inLocalPort,
		size_t		inDataSize,
		char const*	inData)
	{

	}

	virtual void
	Connection_GetData(
		uint16_t	inPort,
		size_t&		ioBufferSize,
		char*		outBuffer)
	{

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
			Update(0);
		}

		if(commandPending)
		{
			commandPending = false;
			commandSucceeded = false;
			commandResultBuffer[0] = 0;
		}
	}

	struct SIPDBuffer
	{
		char	buffer[1024];
		int		channel;
		size_t	bufferIndex;
		size_t	totalBytes;
	};

	HardwareSerial*	serialPort;
	uint8_t	rstPin;
	uint8_t	chPDPin;
	uint8_t	gpio0Pin;
	uint8_t	gpio2Pin;

	int			ipdIndex;
	SIPDBuffer	ipdBuffer[2];
	bool		ipdParsing;

	bool		ready;

	bool		commandPending;
	bool		commandSucceeded;
	uint64_t	commandTimeoutMS;
	size_t		commandResultLength;
	char		commandResultBuffer[256];

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
