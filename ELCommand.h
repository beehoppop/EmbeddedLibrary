#ifndef _ELCOMMAND_H_
#define _ELCOMMAND_H_
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

	This module provides a common mechanism to register commands coming over a generalized port in a similar fashion to command line programs
*/

#include "ELModule.h"
#include "ELOutput.h"

enum
{
	eCmd_Failed = 0,
	eCmd_Succeeded,
	eCmd_Pending,

	eCmd_MaxNameLen = 15,
	eCmd_MaxCommands = 64,
	eCmd_MaxCommandArgs = 64,
};

// A dummy class for the command handler method object
class ICmdHandler
{
public:
};

// The typedef for the command handler method
typedef uint8_t
(ICmdHandler::*TCmdHandlerMethod)(
	IOutputDirector*	inOutput,
	int					inArgC,
	char const*			inArgV[]);

class CModule_SerialCmdHandler : public CModule
{
public:

	MModuleSingleton_Declaration(CModule_SerialCmdHandler)

private:

	CModule_SerialCmdHandler(
		);

	void
	Update(
		uint32_t	inDeltaTimeUS);

	char	charBuffer[256];
	int		curIndex;
};


class CModule_Command : public CModule, public ICmdHandler
{
public:
	
	MModuleSingleton_Declaration(CModule_Command)
	
	// Register a command handler
	void
	RegisterCommand(
		char const*			inCmdName,				// The name of the command, must be a static string
		ICmdHandler*		inCmdHandler,			// The object of the command handler
		TCmdHandlerMethod	inMethod,				// The method of the command handler
		char const*			inDescription = NULL);	// An optional description, must be a static string

	// This will process the given command args
	uint8_t
	ProcessCommand(
		IOutputDirector*	inOutput,
		int					inArgC,
		char const*			inArgV[]);

	// This will process the given command string
	uint8_t
	ProcessCommand(
		IOutputDirector*	inOutput,
		char*				inCmdStr);	// This input command string must be writable in order to break up into discrete arg strings

private:
	
	CModule_Command(
		);

	uint8_t
	HelpCommand(
		IOutputDirector*	inOutput,
		int					inArgC,
		char const*			inArgV[]);
	
	struct SCommand
	{
		char const*			name;
		char const*			description;
		ICmdHandler*		handler;
		TCmdHandlerMethod	method;
	};

	int			handlerCount;
	SCommand	commandList[eCmd_MaxCommands];
};

extern CModule_Command*	gCommandModule;

#endif /* _ELCOMMAND_H_ */
