#ifndef _ELSERIAL_H_
#define _ELSERIAL_H_
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

	This module provides a common mechanism to register commands coming over the serial port in a similar fashion to command line programs
*/

#include "ELModule.h"

enum
{
	eSerial_MaxNameLen = 16,
	eSerial_MaxCommands = 64,
	eSerial_MaxCommandArgs = 64,
};

// A dummy class for the command handler method object
class ISerialCmdHandler
{
public:
};

// The typedef for the command handler method
typedef bool
(ISerialCmdHandler::*TSerialCmdMethod)(
	int		inArgC,
	char*	inArgv[]);

class CModule_SerialCmd : public CModule
{
public:
	
	// Register a command handler
	void
	RegisterCommand(
		char const*			inCmdName,		// The name of the command
		ISerialCmdHandler*	inCmdHandler,	// The object of the command handler
		TSerialCmdMethod	inMethod);		// The method of the command handler

private:
	
	CModule_SerialCmd(
		);

	virtual void
	Update(
		uint32_t	inDeltaTimeUS);
	
	struct SCommand
	{
		char				name[eSerial_MaxNameLen];
		ISerialCmdHandler*	handler;
		TSerialCmdMethod	method;
	};

	bool
	ProcessSerialMsg(
		char*	inMsg);

	int			handlerCount;
	SCommand	commandList[eSerial_MaxCommands];

	char	charBuffer[256];
	int		curIndex;

	static CModule_SerialCmd	module;
};

extern CModule_SerialCmd*	gSerialCmd;

#endif /* _ELSERIAL_H_ */


