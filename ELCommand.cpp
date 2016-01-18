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
#include "ELCommand.h"
#include "ELAssert.h"
#include "ELOutput.h"

class CModule_SerialCmdHandler : public CModule
{
	CModule_SerialCmdHandler(
		)
		:
		CModule("srlc", 0, 0, NULL, 100000, 254)
	{
	}

	virtual void
	Setup(
		void)
	{

	}

	void
	Update(
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

				if(gCommand->ProcessCommand(gSerialOut, charBuffer) == false)
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

	char	charBuffer[256];
	int		curIndex;

	static CModule_SerialCmdHandler	module;
};
CModule_SerialCmdHandler	CModule_SerialCmdHandler::module;

CModule_Command	CModule_Command::module;
CModule_Command*	gCommand;

CModule_Command::CModule_Command(
	)
	:
	CModule("cmnd", 0, 0, NULL, 0, 254)
{
	gCommand = this;
	handlerCount = 0;
}

void
CModule_Command::RegisterCommand(
	char const*			inCmdName, 
	ICmdHandler*		inCmdHandler,
	TCmdHandlerMethod	inMethod)
{
	MReturnOnError(handlerCount >= eCmd_MaxCommands);
	MReturnOnError(strlen(inCmdName) > eCmd_MaxNameLen);

	SCommand*	newCommand = commandList + handlerCount++;

	newCommand->handler = inCmdHandler;
	newCommand->method = inMethod;
	strncpy(newCommand->name, inCmdName, sizeof(newCommand->name));
}

bool
CModule_Command::ProcessCommand(
	IOutputDirector*	inOutput,
	int					inArgC,
	char const*			inArgv[])
{
	AddDebugMsgHandler(inOutput);	// Add the output source to the list of debug msg handlers so it will get all output

	bool	result = false;
	bool	cmdFound = false;
	for(int itr = 0; itr < handlerCount; ++itr)
	{
		if(strcmp(commandList[itr].name, inArgv[0]) == 0)
		{
			cmdFound = true;
			result = (commandList[itr].handler->*commandList[itr].method)(inOutput, inArgC, inArgv);
			break;
		}
	}

	if(cmdFound == false)
	{
		inOutput->printf("Could not find cmd %s\n", inArgv[0]);
	}
	else if(result == false)
	{
		inOutput->printf("%s: FAILED\n", inArgv[0]);
	}
	else
	{
		inOutput->printf("%s: SUCCESS\n", inArgv[0]);
	}

	RemoveDebugMsgHandler(inOutput);	// Now that the command is done remove it's output handler

	return result;
}

bool
CModule_Command::ProcessCommand(
	IOutputDirector*	inOutput,
	char*				inStr)
{
	int strLen = strlen(inStr);
	char*	components[eCmd_MaxCommandArgs];
	char*	cp = inStr;
	char*	ep = inStr + strLen;
	int		curCompIndex = 0;

	if(strLen == 0)
	{
		return false;
	}

	while(cp < ep)
	{
		if(curCompIndex >= eCmd_MaxCommandArgs)
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

	return ProcessCommand(inOutput, curCompIndex, (char const**)components);
}
