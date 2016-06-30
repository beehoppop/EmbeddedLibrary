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

#include <ELModule.h>
#include <ELCommand.h>
#include <ELAssert.h>
#include <ELOutput.h>
#include <ELConfig.h>
#include <ELUtilities.h>

CModule_Command*	gCommandModule;

MModuleImplementation_Start(CModule_SerialCmdHandler)
MModuleImplementation_Finish(CModule_SerialCmdHandler)

CModule_SerialCmdHandler::CModule_SerialCmdHandler(
	)
	:
	CModule(0, 0, NULL, 100000)
{
	curIndex = 0;
}

void
CModule_SerialCmdHandler::Update(
	uint32_t	inDeltaTimeUS)
{
	size_t	bytesAvailable = Serial.available();
	char	tmpBuffer[256];

	if(bytesAvailable == 0)
	{
		return;
	}

	bytesAvailable = MMin(bytesAvailable, sizeof(tmpBuffer));
	Serial.readBytes(tmpBuffer, bytesAvailable);

	for(size_t i = 0; i < bytesAvailable; ++i)
	{
		char c = tmpBuffer[i];

		if(c == '\n' || c == '\r')
		{
			if(curIndex > 0)
			{
				charBuffer[curIndex] = 0;
				curIndex = 0;

				MAssert(gSerialOut != NULL);
				gCommandModule->ProcessCommand(gSerialOut, charBuffer);
			}
		}
		else
		{
			if(curIndex < (int)sizeof(charBuffer) - 1)
			{
				charBuffer[curIndex++] = c;
			}
		}
	}
}

MModuleImplementation_Start(CModule_Command)
MModuleImplementation_FinishGlobal(CModule_Command, gCommandModule)

CModule_Command::CModule_Command(
	)
	:
	CModule()
{
	handlerCount = 0;
	memset(commandList, 0, sizeof(commandList));

	RegisterCommand(
		"help",
		this,
		static_cast<TCmdHandlerMethod>(&CModule_Command::HelpCommand),
		": List the available commands and descriptions");
}

uint8_t
CModule_Command::HelpCommand(
	IOutputDirector*	inOutput,
	int					inArgC,
	char const*			inArgV[])
{
	SCommand*	curCmd = commandList;

	for(int i = 0; i < handlerCount; ++i, ++curCmd)
	{
		inOutput->printf("%s %s\n", curCmd->name, curCmd->description != NULL ? curCmd->description : "(N/A)");
	}

	return eCmd_Succeeded;
}

void
CModule_Command::RegisterCommand(
	char const*			inCmdName, 
	ICmdHandler*		inCmdHandler,
	TCmdHandlerMethod	inMethod,
	char const*			inDescription)
{
	MReturnOnError(inCmdName == NULL || strlen(inCmdName) == 0);
	MReturnOnError(handlerCount >= eCmd_MaxCommands);

	SCommand*	newCommand = commandList + handlerCount++;

	newCommand->name = inCmdName;
	newCommand->description = inDescription;
	newCommand->handler = inCmdHandler;
	newCommand->method = inMethod;
}

uint8_t
CModule_Command::ProcessCommand(
	IOutputDirector*	inOutput,
	int					inArgC,
	char const*			inArgV[])
{
	AddSysMsgHandler(inOutput);	// Add the output source to the list of debug msg handlers so it will get all output

	uint8_t	result = eCmd_Failed;
	bool	cmdFound = false;
	for(int itr = 0; itr < handlerCount; ++itr)
	{
		if(strcmp(commandList[itr].name, inArgV[0]) == 0)
		{
			cmdFound = true;
			result = (commandList[itr].handler->*commandList[itr].method)(inOutput, inArgC, inArgV);
			break;
		}
	}

	if(cmdFound == false)
	{
		inOutput->printf("Could not find cmd %s\n", inArgV[0]);
	}
	
	if(result == eCmd_Failed)
	{
		inOutput->printf("CC:[%03d] %s FAILED\n", gConfigModule->GetVal(gConfigModule->nodeIDIndex), inArgV[0]);
	}
	else if(result == eCmd_Succeeded)
	{
		inOutput->printf("CC:[%03d] %s SUCCEEDED\n", gConfigModule->GetVal(gConfigModule->nodeIDIndex), inArgV[0]);
	}

	RemoveSysMsgHandler(inOutput);	// Now that the command is done remove it's output handler

	return result;
}

uint8_t
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
