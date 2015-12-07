/*
	ELAssert.h
*/

#ifndef _ELASSERT_H_
#define _ELASSERT_H_

#include "EL.h"

// Use this macro to assert that an expression is true - if its not the program will halt execution and repeatedly output a debug message
#define MAssert(x) if(!(x)) AssertFailed(#x, __FILE__, __LINE__)

#define MReturnOnError(x, ...) do {if(x) {DebugMsg(0, "ERROR: %s %s %d", #x, __FILE__, __LINE__); return __VA_ARGS__;}} while(0)

enum
{
	eMaxDebugMsgHandlers = 4,

	eDbgLevel_Off = 0,
	eDbgLevel_Basic,
	eDbgLevel_Medium,
	eDbgLevel_Verbose,
};

// Set the debugging level - the higher the level the more detailed and verbose the messages will be
void
SetDebugLevel(
	int	inLevel);

// Output a debug msg when inLevel <= the current debug level
void
DebugMsg(
	uint8_t		inLevel,
	char const*	inMsg,
	...);

// Inheret from this class to define your our own debug message outputter - you must register your class using RegisterDebugMsgHandler
class IDebugMsgHandler
{
public:

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

