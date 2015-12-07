// 
// 
// 

#include <string.h>

#include "ELModule.h"
#include "ELSerial.h"
#include "ELAssert.h"


CModule_SerialCmd::CModule_SerialCmd(
	)
	:
	CModule("sril", 0, 1000, 0, 4, false)
{
	handlerCount = 0;
}

void
CModule_SerialCmd::Update(
	uint32_t	inDeltaTimeUS)
{
	int	bytesAvailable = Serial.available();
	char	tmpBuffer[256];

	if(bytesAvailable == 0)
	{
		return;
	}

	Serial.readBytes(tmpBuffer, bytesAvailable);

	for(int i = 0; i < bytesAvailable; ++i)
	{
		char c = tmpBuffer[i];

		if(c == '\n')
		{
			charBuffer[curIndex] = 0;
			curIndex = 0;

			if(ProcessSerialMsg(charBuffer) == false)
			{
				Serial.write("Command Failed\n");
			}
		}
		else
		{
			charBuffer[curIndex++] = c;
		}
	}
}

void
CModule_SerialCmd::RegisterCommand(
	char const*			inCmdName, 
	ISerialCmdHandler*	inCmdHandler,
	TSerialCmdMethod	inMethod)
{
	MReturnOnError(handlerCount >= eSerial_MaxCommands || strlen(inCmdName) + 1 >= eSerial_MaxNameLen);

	commandList[handlerCount].handler = inCmdHandler;
	commandList[handlerCount].method = inMethod;
	strncpy(commandList[handlerCount].name, inCmdName, sizeof(commandList[handlerCount].name));
}

bool
CModule_SerialCmd::ProcessSerialMsg(
	char*	inStr)
{
	int strLen = strlen(inStr);
	char*	components[eSerial_MaxCommandArgs];
	char*	cp = inStr;
	char*	ep = inStr + strLen;
	int		curCompIndex = 0;

	DebugMsg(eDbgLevel_Verbose, "Serial: Received %s\n", inStr);

	while(cp < ep)
	{
		if(curCompIndex >= eSerial_MaxCommandArgs)
		{
			break;
		}

		components[curCompIndex++] = cp;
		cp = strchr(cp, ' ');
		if(cp == NULL)
		{
			break;
		}
		*cp = 0;
		++cp;
	}

	if(curCompIndex == 0)
	{
		return false;
	}

	components[curCompIndex] = NULL;

	for(int itr = 0; itr < handlerCount; ++itr)
	{
		if(strcmp(commandList[itr].name, components[0]) == 0)
		{
			return (commandList[itr].handler->*commandList[itr].method)(curCompIndex, components);
		}
	}

	return false;
}

CModule_SerialCmd	gSerialCmd;
