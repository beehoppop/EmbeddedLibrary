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

#include <string.h>

#include "ELModule.h"
#include "ELSerial.h"
#include "ELAssert.h"

CModule_SerialCmd	CModule_SerialCmd::module;
CModule_SerialCmd*	gSerialCmd;

CModule_SerialCmd::CModule_SerialCmd(
	)
	:
	CModule("sril", 0, 0, NULL, 1000, 254)
{
	gSerialCmd = this;
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

			if(ProcessCommand(charBuffer) == false)
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
	MReturnOnError(handlerCount >= eSerial_MaxCommands);
	MReturnOnError(strlen(inCmdName) > eSerial_MaxNameLen);

	SCommand*	newCommand = commandList + handlerCount++;

	newCommand->handler = inCmdHandler;
	newCommand->method = inMethod;
	strncpy(newCommand->name, inCmdName, sizeof(newCommand->name));
}

bool
CModule_SerialCmd::ProcessCommand(
	int			inArgC,
	char const*	inArgv[])
{
	for(int itr = 0; itr < handlerCount; ++itr)
	{
		if(strcmp(commandList[itr].name, inArgv[0]) == 0)
		{
			return (commandList[itr].handler->*commandList[itr].method)(inArgC, inArgv);
		}
	}

	DebugMsg(eDbgLevel_Basic, "Could not find cmd %s\n", inArgv[0]);

	return false;
}

bool
CModule_SerialCmd::ProcessCommand(
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
		DebugMsg(eDbgLevel_Basic, "no command\n");
		return false;
	}

	components[curCompIndex] = NULL;

	return ProcessCommand(curCompIndex, (char const**)components);
}
