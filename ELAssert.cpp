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

#include "ELAssert.h"
#include "ELModule.h"
#include "ELConfig.h"
#include "ELOutput.h"
#include "ELRealtime.h"
#include "ELString.h"

struct SOutputEntry
{
	IOutputDirector*	outputDirector;
	int					refCount;
};

static SOutputEntry	gEntries[eMaxMsgHandlers];

MModuleImplementation_Start(CModule_SysMsgCmdHandler)
MModuleImplementation_Finish(CModule_SysMsgCmdHandler)

CModule_SysMsgCmdHandler::CModule_SysMsgCmdHandler(
	)
	:
	CModule()
{
	msgBufferIndex = 0;

	AddSysMsgHandler(this);

	// Begin including other dependent modules
	CModule_Command::Include();
}

void
CModule_SysMsgCmdHandler::Setup(
	void)
{
	MAssert(gCommandModule != NULL);

	MCommandRegister("msg_dump", CModule_SysMsgCmdHandler::MsgLogDump, ": Dump a brief history of the system messages");
}

uint8_t
CModule_SysMsgCmdHandler::MsgLogDump(
	IOutputDirector*	inOutputDirector,
	int					inArgC,
	char const*			inArgV[])
{
	inOutputDirector->write("*****\n");
	if(msgBufferIndex <= sizeof(msgBuffer))
	{
		inOutputDirector->write(msgBuffer, msgBufferIndex);
	}
	else
	{
		inOutputDirector->write(msgBuffer + (msgBufferIndex % sizeof(msgBuffer)), sizeof(msgBuffer) - (msgBufferIndex % sizeof(msgBuffer)));
		inOutputDirector->write(msgBuffer, (msgBufferIndex % sizeof(msgBuffer)));
	}
	inOutputDirector->write("\n*****\n");

	return eCmd_Succeeded;
}

void
CModule_SysMsgCmdHandler::write(
	char const*	inMsg,
	size_t		inBytes)
{
	for(size_t i = 0; i < inBytes; ++i)
	{
		msgBuffer[msgBufferIndex++ % sizeof(msgBuffer)] = inMsg[i];
	}
}

void
AssertFailed(
	char const*	inMsg,
	char const*	inFile,
	int			inLine)
{
	for(;;)
	{
		Serial.printf("ASSERT: %s %d %s\n", inFile, inLine, inMsg);
		delay(500);
	}
}

static void
DebugMsgVA(
	uint8_t		inLevel,
	char const*	inMsg,
	va_list		inVAList)
{
	if(gConfigModule != NULL && gConfigModule->debugLevelIndex < eConfigVar_Max && inLevel > gConfigModule->GetVal(gConfigModule->debugLevelIndex))
		return;

	TString<512>	vabuffer;
	vabuffer.SetVA(inMsg, inVAList);
	TString<32>		timestamp;

	int	year = 0;
	int	month = 0;
	int	day = 0;
	int	dow = 0;
	int	hour = 0;
	int	minute = 0;
	int	sec = 0;
	int	ms = 0;
	if(gRealTime != NULL)
	{
		gRealTime->GetDateAndTimeMS(year, month, day, dow, hour, minute, sec, ms);
	}
	else
	{
		ms = millis();
	}

	timestamp.SetF("%02d:%02d:%02d:%02d:%03d", day, hour, minute, sec, ms);

	TString<512>	finalBuffer;
	finalBuffer.SetF("[%s] %s", (char*)timestamp, (char*)vabuffer);

	// If the message does not end with a newline add one
	if(!finalBuffer.EndsWith('\n'))
	{
		finalBuffer.Append('\n');
	}

	// Share it with the world
	bool	sent = false;
	for(int itr = 0; itr < eMaxMsgHandlers; ++itr)
	{
		IOutputDirector*	curOD = gEntries[itr].outputDirector;
		if(curOD != NULL)
		{
			// Never recurse to the same output director
			static int				tos;
			static IOutputDirector*	outputDirectorStack[eMaxMsgHandlers];

			bool	onStack = false;
			for(int i = 0; i < tos; ++i)
			{
				if(outputDirectorStack[i] == curOD)
				{
					onStack = true;
					break;
				}
			}

			if(!onStack)
			{
				outputDirectorStack[tos++] = curOD;
				curOD->write(finalBuffer);
				--tos;
			}

			sent = true;
		}
	}

	if(!sent)
	{
		// We must be in early init
		Serial.write(finalBuffer);
	}
}	

void
SystemMsg(
	uint8_t		inLevel,
	char const*	inMsg,
	...)
{
	va_list	varArgs;
	va_start(varArgs, inMsg);
	DebugMsgVA(inLevel, inMsg, varArgs);
	va_end(varArgs);
}

void
SystemMsg(
	char const*	inMsg,
	...)
{
	va_list	varArgs;
	va_start(varArgs, inMsg);
	DebugMsgVA(eMsgLevel_Always, inMsg, varArgs);
	va_end(varArgs);
}

void
AddSysMsgHandler(
	IOutputDirector*	inOutputDirector)
{
	int	targetIndex = eMaxMsgHandlers;

	for(int i = 0; i < eMaxMsgHandlers; ++i)
	{
		if(gEntries[i].outputDirector == inOutputDirector)
		{
			targetIndex = i;
			break;
		}
		if(targetIndex == eMaxMsgHandlers && gEntries[i].outputDirector == NULL)
		{
			gEntries[i].refCount = 0;
			targetIndex = i;
		}
	}

	MReturnOnError(targetIndex == eMaxMsgHandlers);

	gEntries[targetIndex].outputDirector = inOutputDirector;
	++gEntries[targetIndex].refCount;
}

void
RemoveSysMsgHandler(
	IOutputDirector*	inOutputDirector)
{
	for(int i = 0; i < eMaxMsgHandlers; ++i)
	{
		if(gEntries[i].outputDirector == inOutputDirector)
		{
			if(--gEntries[i].refCount <= 0)
			{
				gEntries[i].outputDirector = NULL;
				gEntries[i].refCount = 0;
			}
			break;
		}
	}
}

void
AllowABreakpoint(
	void)
{
	// Allow a breakpoint to be set on this line if MReturnOnError encounters an error
	#if WIN32
	printf("AllowABreakpoint\n");
	#endif
}
