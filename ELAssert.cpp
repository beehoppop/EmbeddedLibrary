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

#define MBufferMsgs 1

static IDebugMsgHandler*	gHandlers[eMaxDebugMsgHandlers];
static int					gHandlerCount;

#if MBufferMsgs
static char		gMsgBuffer[2048];
static uint32_t	gMsgBufferIndex = 0;

class CDebugSerialHanlder : public CModule, public ISerialCmdHandler
{
public:
	
	CDebugSerialHanlder(
		)
		:
		CModule(
			"asrt",
			0,
			0,
			0,
			1)
	{
	}

	virtual void
	Setup(
		void)
	{
		gSerialCmd->RegisterCommand("dump_dbg", this, static_cast<TSerialCmdMethod>(&CDebugSerialHanlder::DbgLogDump));
	}

	bool
	DbgLogDump(
		int			inArgC,
		char const*	inArgV[])
	{
		#if MBufferMsgs
		Serial.write("*****\n");
		if(gMsgBufferIndex <= sizeof(gMsgBuffer))
		{
			Serial.write(gMsgBuffer, gMsgBufferIndex);
		}
		else
		{
			Serial.write(gMsgBuffer + (gMsgBufferIndex % sizeof(gMsgBuffer)), sizeof(gMsgBuffer) - (gMsgBufferIndex % sizeof(gMsgBuffer)));
			Serial.write(gMsgBuffer, (gMsgBufferIndex % sizeof(gMsgBuffer)));
		}
		Serial.write("\n*****\n");
		#endif

		return true;
	}

};

CDebugSerialHanlder	gDebugSerialHandler;
#endif


void
AssertFailed(
	char const*	inMsg,
	char const*	inFile,
	int			inLine)
{
	for(;;)
	{
		DebugMsg(0, "ASSERT: %s %d %s\n", inFile, inLine, inMsg);
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

	snprintf(finalBuffer, sizeof(finalBuffer), "[%s] %s", timestamp, vabuffer);
	Serial.print(finalBuffer);
	int	strLen = strlen(finalBuffer);

	#if MBufferMsgs
	for(int i = 0; i < strLen; ++i)
	{
		gMsgBuffer[gMsgBufferIndex++ % sizeof(gMsgBuffer)] = finalBuffer[i];
	}
	#endif

	if(finalBuffer[strLen - 1] != '\n')
	{
		Serial.print('\n');
		#if MBufferMsgs
		gMsgBuffer[gMsgBufferIndex++ % sizeof(gMsgBuffer)] = '\n';
		#endif
	}

	for(int itr = 0; itr < gHandlerCount; ++itr)
	{
		gHandlers[itr]->OutputDebugMsg(finalBuffer);
	}
}

void
RegisterDebugMsgHandler(
	IDebugMsgHandler*	inHandler)
{
	if(gHandlerCount >= eMaxDebugMsgHandlers)
	{
		return;
	}

	gHandlers[gHandlerCount++] = inHandler;
}
