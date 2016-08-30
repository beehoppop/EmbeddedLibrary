#ifndef _ELASSERT_H_
#define _ELASSERT_H_
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

	This is used to verify program correctness and aid in debugging.

	One call, SystemMsg(), is used to output messages from the program. The actual output
	is then sent to multiple IOutputDirector objects (serial, internet logging, CAN bus, etc).
	IOutputDirector objects are registered at program start via AddSysMsgHandler()
*/

#include <EL.h>
#include <ELOutput.h>
#include <ELModule.h>
#include <ELCommand.h>

// Use this macro to assert that an expression is true - if its not the program will halt execution and repeatedly output a debug message
#define MAssert(x) if(!(x)) AssertFailed(#x, __FILE__, __LINE__)

// Use this macro to return from a function if the given condition is true and notify the user about it
#define MReturnOnError(x, ...) do {if(x) {SystemMsg("ERROR: %s %s %d", #x, __FILE__, __LINE__); AllowABreakpoint(); return __VA_ARGS__;}} while(0)

enum
{
	eMaxMsgHandlers = 6,

	eMsgLevel_Off = 0,

	eMsgLevel_Always = 0,
	eMsgLevel_Basic,
	eMsgLevel_Medium,
	eMsgLevel_Verbose,

	eMsgBuffer_Size = 2 * 1024,
};

// Inlcude() this module to dump a log of system messages using the "msg_dump" command
class CModule_SysMsgCmdHandler : public CModule, public ICmdHandler, public IOutputDirector
{
public:

	MModule_Declaration(CModule_SysMsgCmdHandler);

private:
	
	CModule_SysMsgCmdHandler(
		);

	virtual void
	Setup(
		void);

	uint8_t
	MsgLogDump(
		IOutputDirector*	inOutputDirector,
		int					inArgC,
		char const*			inArgV[]);

	virtual void
	write(
		char const*	inMsg,
		size_t		inBytes);

	char		msgBuffer[eMsgBuffer_Size];
	uint32_t	msgBufferIndex;
};

// This module is instantiated by the library so that system msgs go out to the serial port
class CModule_SysMsgSerialHandler : public CModule, public IOutputDirector
{
public:

	MModule_Declaration(CModule_SysMsgSerialHandler);

private:
	
	CModule_SysMsgSerialHandler(
		);

	virtual void
	write(
		char const*	inMsg,
		size_t		inBytes);
};

// Output a debug msg when inLevel <= the current debug level
void
SystemMsg(
	uint8_t		inLevel,
	char const*	inMsg,
	...);

// Always output the debug msg
void
SystemMsg(
	char const*	inMsg,
	...);

// Add the given handler so that it can be called on every call to SystemMsg()
void
AddSysMsgHandler(
	IOutputDirector*	inOutputDirector);

// Add the given handler so that it can be called on every call to SystemMsg()
void
RemoveSysMsgHandler(
	IOutputDirector*	inOutputDirector);

// This is used by the MAssert macro to handle a failed assertion
void
AssertFailed(
	char const*	inMsg,
	char const*	inFile,
	int			inLine);

void
AllowABreakpoint(
	void);
	
#endif /* _ELASSERT_H_ */
