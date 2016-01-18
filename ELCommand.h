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
typedef bool
(ICmdHandler::*TCmdHandlerMethod)(
	IOutputDirector*	inOutput,
	int					inArgC,
	char const*			inArgv[]);

class CModule_Command : public CModule
{
public:
	
	// Register a command handler
	void
	RegisterCommand(
		char const*			inCmdName,		// The name of the command
		ICmdHandler*		inCmdHandler,	// The object of the command handler
		TCmdHandlerMethod	inMethod);		// The method of the command handler

	// This will process the given command args
	bool
	ProcessCommand(
		IOutputDirector*	inOutput,
		int					inArgC,
		char const*			inArgv[]);

	// This will process the given command string
	bool
	ProcessCommand(
		IOutputDirector*	inOutput,
		char*				inCmdStr);	// This input command string must be writable in order to break up into discrete arg strings

private:
	
	CModule_Command(
		);
	
	struct SCommand
	{
		char				name[eCmd_MaxNameLen + 1];
		ICmdHandler*		handler;
		TCmdHandlerMethod	method;
	};

	int			handlerCount;
	SCommand	commandList[eCmd_MaxCommands];

	static CModule_Command	module;
};

extern CModule_Command*	gCommand;

#endif /* _ELCOMMAND_H_ */
