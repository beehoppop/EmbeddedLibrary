// 
// 
// 

#include "ELAssert.h"
#include "ELModule.h"

static uint8_t				gLevel = eDbgLevel_Off;
static IDebugMsgHandler*	gHandlers[eMaxDebugMsgHandlers];
static int					gHandlerCount;

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
	if(inLevel > gLevel)
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
	Serial.print('\n');

	for(int itr = 0; itr < gHandlerCount; ++itr)
	{
		gHandlers[itr]->OutputDebugMsg(finalBuffer);
	}
}

void
SetDebugLevel(
	int	inLevel)
{
	gLevel = inLevel;
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
