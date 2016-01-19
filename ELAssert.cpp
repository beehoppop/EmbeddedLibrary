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

#define MBufferMsgs 1

struct SOutputEntry
{
	IOutputDirector*	outputDirector;
	int					refCount;
};

static SOutputEntry	gEntries[eMaxDebugMsgHandlers];

IOutputDirector*	gSerialOut;

#if MBufferMsgs
char		gMsgBuffer[1024 * 8];
uint32_t	gMsgBufferIndex;
#endif

static void
SerialOutEarly_vprintf(
	char const*	inMsg,
	va_list		inVAList)
{
	char	vabuffer[256];
	vsnprintf(vabuffer, sizeof(vabuffer) - 1, inMsg, inVAList);
	vabuffer[sizeof(vabuffer) - 1] = 0;
	SerialOutEarly_write(vabuffer);
}

void
SerialOutEarly_write(
	char const*	inMsg)
{
	SerialOutEarly_write(inMsg, strlen(inMsg));
}

void
SerialOutEarly_write(
	char const*	inMsg,
	size_t		inBytes)
{
	Serial.write(inMsg, inBytes);
	#if MBufferMsgs
	for(size_t i = 0; i < inBytes; ++i)
	{
		gMsgBuffer[gMsgBufferIndex++ % sizeof(gMsgBuffer)] = inMsg[i];
	}
	#endif
}

void
SerialOutEarly_printf(
	char const*	inMsg,
	...)
{
	va_list	varArgs;
	va_start(varArgs, inMsg);
	SerialOutEarly_vprintf(inMsg, varArgs);
	va_end(varArgs);
}

class CDebugSerialHanlder : public CModule, public ICmdHandler, public IOutputDirector
{
public:
	
	CDebugSerialHanlder(
		)
		:
		CModule(
			"bdgh",
			0,
			0,
			NULL,
			0,
			1)
	{
		gSerialOut = this;
	}

	virtual void
	Setup(
		void)
	{
		AddDebugMsgHandler(this);
		gCommand->RegisterCommand("debug_dump", this, static_cast<TCmdHandlerMethod>(&CDebugSerialHanlder::DbgLogDump));
	}

	uint8_t
	DbgLogDump(
		IOutputDirector*	inOutputDirector,
		int					inArgC,
		char const*			inArgV[])
	{
		#if MBufferMsgs
		inOutputDirector->write("*****\n");
		if(gMsgBufferIndex <= sizeof(gMsgBuffer))
		{
			inOutputDirector->write(gMsgBuffer, gMsgBufferIndex);
		}
		else
		{
			inOutputDirector->write(gMsgBuffer + (gMsgBufferIndex % sizeof(gMsgBuffer)), sizeof(gMsgBuffer) - (gMsgBufferIndex % sizeof(gMsgBuffer)));
			inOutputDirector->write(gMsgBuffer, (gMsgBufferIndex % sizeof(gMsgBuffer)));
		}
		inOutputDirector->write("\n*****\n");
		#endif

		return eCmd_Succeeded;
	}

	virtual void
	write(
		char const*	inMsg,
		size_t		inBytes)
	{
		SerialOutEarly_write(inMsg, inBytes);
	}

	static CDebugSerialHanlder	module;
};

CDebugSerialHanlder	CDebugSerialHanlder::module;

void
AssertFailed(
	char const*	inMsg,
	char const*	inFile,
	int			inLine)
{
	for(;;)
	{
		DebugMsg(0, "ASSERT: %s %d %s\n", inFile, inLine, inMsg);
		delay(500);
	}
}

void
DebugMsg(
	uint8_t		inLevel,
	char const*	inMsg,
	...)
{
	if(gConfig != NULL && inLevel > gConfig->GetVal(gConfig->debugLevelIndex))
		return;

	va_list	varArgs;
	va_start(varArgs, inMsg);
	char	vabuffer[256];
	vsnprintf(vabuffer, sizeof(vabuffer), inMsg, varArgs);
	va_end(varArgs);

	char	timestamp[32];
	uint32_t	remaining = uint32_t(gCurLocalMS / 1000);
	uint32_t	hours = remaining / (60 * 60);
	remaining -= hours * 60 * 60;
	uint32_t	mins = remaining / 60;
	remaining -= mins * 60;
	uint32_t	secs = remaining;

	snprintf(timestamp, sizeof(timestamp), "%02lu:%02lu:%02lu:%03lu", hours, mins, secs, uint32_t(gCurLocalMS) % 1000);

	char finalBuffer[256];
	snprintf(finalBuffer, sizeof(finalBuffer) - 1, "[%s] %s", timestamp, vabuffer);
	finalBuffer[sizeof(finalBuffer) - 1] = 0;	// Ensure valid string
	size_t	strLen = strlen(finalBuffer);

	// Ensure enough room for both a newline and a zero byte
	if(strLen > sizeof(finalBuffer) - 2)
	{
		strLen = sizeof(finalBuffer) - 2;
	}

	// If the message does not end with a newline add one
	if(finalBuffer[strLen - 1] != '\n')
	{
		finalBuffer[strLen] = '\n';
		finalBuffer[strLen + 1] = '0';
	}

	// Share it with the world
	bool	sent = false;
	for(int itr = 0; itr < eMaxDebugMsgHandlers; ++itr)
	{
		if(gEntries[itr].outputDirector != NULL)
		{
			gEntries[itr].outputDirector->write(finalBuffer);
			sent = true;
		}
	}

	if(!sent)
	{
		// We must be in early init
		SerialOutEarly_write(finalBuffer);
	}
}

void
AddDebugMsgHandler(
	IOutputDirector*	inOutputDirector)
{
	int	targetIndex = eMaxDebugMsgHandlers;

	for(int i = 0; i < eMaxDebugMsgHandlers; ++i)
	{
		if(gEntries[i].outputDirector == inOutputDirector)
		{
			targetIndex = i;
			break;
		}
		if(targetIndex == eMaxDebugMsgHandlers && gEntries[i].outputDirector == NULL)
		{
			gEntries[i].refCount = 0;
			targetIndex = i;
		}
	}

	MReturnOnError(targetIndex == eMaxDebugMsgHandlers);

	gEntries[targetIndex].outputDirector = inOutputDirector;
	++gEntries[targetIndex].refCount;
}

void
RemoveDebugMsgHandler(
	IOutputDirector*	inOutputDirector)
{
	for(int i = 0; i < eMaxDebugMsgHandlers; ++i)
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
