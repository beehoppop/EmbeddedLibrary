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

	Commonly needed functionality for debugging, verifying program correctness, and handling errors gracefully
*/

#include "EL.h"

// Use this macro to assert that an expression is true - if its not the program will halt execution and repeatedly output a debug message
#define MAssert(x) if(!(x)) AssertFailed(#x, __FILE__, __LINE__)

// Use this macro to return from a function if the given condition is true and notify the user about it
#define MReturnOnError(x, ...) do {if(x) {DebugMsg(0, "ERROR: %s %s %d", #x, __FILE__, __LINE__); return __VA_ARGS__;}} while(0)

enum
{
	eMaxDebugMsgHandlers = 4,

	eDbgLevel_Off = 0,
	eDbgLevel_Basic,
	eDbgLevel_Medium,
	eDbgLevel_Verbose,
};

// Output a debug msg when inLevel <= the current debug level
void
DebugMsg(
	uint8_t		inLevel,
	char const*	inMsg,
	...);

// Inherit from this class to define your our own debug message output mechanism - you must register your class using RegisterDebugMsgHandler
class IDebugMsgHandler
{
public:
	
	virtual char const*
	GetDebugID(
		void) = 0;

	virtual void
	OutputDebugMsg(
		char const*	inMsg) = 0;

};

// Register the given handler so that it can be called on every call to DebugMsg()
void
RegisterDebugMsgHandler(
	IDebugMsgHandler*	inHandler);

// This is used by the MAssert macro to handle a failed assertion
void
AssertFailed(
	char const*	inMsg,
	char const*	inFile,
	int			inLine);
	
#endif /* _ELASSERT_H_ */
