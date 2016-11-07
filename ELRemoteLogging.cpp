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

*/

#include "ELRemoteLogging.h"
#include "ELAssert.h"
#include "ELCommand.h"

MModuleImplementation_Start(
	CModule_Loggly,
	char const*	inGlobalTags)
MModuleImplementation_Finish(CModule_Loggly, inGlobalTags)

CModule_Loggly::CModule_Loggly(
	char const*	inGlobalTags)
	:
	CModule(sizeof(SSettings), 1, &settings, 50000)
{
	head = tail = 0;
	globalTags = inGlobalTags;
	requestInProgress = false;

	CModule_Internet::Include();
}

void
CModule_Loggly::write(
	char const*	inMsg,
	size_t		inBytes)
{
	while(inBytes-- > 0)
	{
		buffer[head++ % sizeof(buffer)] = *inMsg++;
	}
	buffer[head++ % sizeof(buffer)] = 0;
}

void
CModule_Loggly::SendLog(
	char const*	inTags,
	char const*	inFormat,
	...)
{
	TString<256>	tmpBuffer;

	va_list	varArgs;
	va_start(varArgs, inFormat);
	tmpBuffer.Append(inFormat, varArgs);
	va_end(varArgs);

	for(;;)
	{
		char c = *inTags++;
		if(c == 0)
		{
			break;
		}

		buffer[head++ % sizeof(buffer)] = c;
	}
	buffer[head++ % sizeof(buffer)] = ':';

	int bufLen = tmpBuffer.GetLength();
	char*	cp = tmpBuffer;
	while(bufLen-- > 0)
	{
		buffer[head++ % sizeof(buffer)] = *cp++;
	}
	buffer[head++ % sizeof(buffer)] = 0;
}
	
void
CModule_Loggly::Setup(
	void)
{
	UpdateServerData();
	MCommandRegister("remotelogging_set", CModule_Loggly::Command_SetUUID, "[serveraddress] [uuid] : Set the Loggly UUID");
	MCommandRegister("remotelogging_get", CModule_Loggly::Command_GetUUID, ": Get the Loggly UUID");
}

uint16_t
CModule_Loggly::GetQueueLength(
	void)
{
	return (uint16_t)(head - tail);
}

uint16_t
CModule_Loggly::GetQueueLengthFromGivenTail(
	uint16_t	inTail)
{
	return (uint16_t)(head - inTail);
}

void
CModule_Loggly::Update(
	uint32_t	inDeltaUS)
{
	if(gInternetModule->ConnectedToInternet() && connection != NULL && requestInProgress == false && GetQueueLength() > 0)
	{
		TString<128>	tagsBuffer;
		TString<512>	msgBuffer;

		connection->StartRequest("POST", url);
		connection->SendHeaders(1, "content-type", "text/plain");

		if(buffer[tail % sizeof(buffer)] == '[')
		{
			// This means there is a timestamp so add the timestamp characters up until the next space directly to the message buffer
			while(GetQueueLength() > 0)
			{
				char c = buffer[tail++ % sizeof(buffer)];
				if(c == 0)
				{
					--tail;	// Ensure that 0 gets written out when we copy the rest of the message buffer
					break;
				}

				msgBuffer.Append(c);
			
				if(c == ' ')
				{
					break;
				}
			}
		}

		// Look ahead to see if there are tags
		bool		hasTags = false;
		uint16_t	tmpTail = tail;
		while(GetQueueLengthFromGivenTail(tmpTail) > 0)
		{
			char c = buffer[tmpTail++ % sizeof(buffer)];
			if(c == ':')
			{
				hasTags = true;
				break;
			}

			if(!isalnum(c) && c != '_')
			{
				break;
			}
		}

		#if WIN32
			tagsBuffer.Append("WIN32");
		#endif

		if(gVersionStr[0] != 0)
		{
			if(tagsBuffer.GetLength() > 0)
			{
				tagsBuffer.Append(',');
			}
			tagsBuffer.Append(gVersionStr);
		}

		if(globalTags[0] != 0)
		{
			if(tagsBuffer.GetLength() > 0)
			{
				tagsBuffer.Append(',');
			}
			tagsBuffer.Append(globalTags);
		}

		if(hasTags)
		{
			if(tagsBuffer.GetLength() > 0)
			{
				tagsBuffer.Append(',');
			}
			for(;;)
			{
				char c = buffer[tail++ % sizeof(buffer)];
				if(c == ':')
				{
					++tail;
					break;
				}

				tagsBuffer.Append(c);
			}
		}

		connection->SendHeaders(1, "X-LOGGLY-TAG", (char*)tagsBuffer);

		// Copy the rest of the message over
		while(GetQueueLength() > 0)
		{
			char c = buffer[tail++ % sizeof(buffer)];
			if(c == 0)
			{
				break;
			}

			msgBuffer.Append(c);
		}

		connection->SendBody(msgBuffer);

		requestInProgress = true;
	}
}

void
CModule_Loggly::HTTPResponseHandlerMethod(
	uint16_t			inHTTPReturnCode,
	int					inDataSize,
	char const*			inData)
{
	requestInProgress = false;
}
	
uint8_t
CModule_Loggly::Command_SetUUID(
	IOutputDirector*	inOutput,
	int					inArgC,
	char const*			inArgV[])
{
	if(inArgC != 3)
	{
		return eCmd_Failed;
	}

	memset(settings.serverAddress, 0, sizeof(settings.serverAddress));
	strncpy(settings.serverAddress, inArgV[1], sizeof(settings.serverAddress) - 1);
	memset(settings.uuid, 0, sizeof(settings.uuid));
	strncpy(settings.uuid, inArgV[2], sizeof(settings.uuid) - 1);

	UpdateServerData();

	EEPROMSave();

	return eCmd_Succeeded;
}
	
uint8_t
CModule_Loggly::Command_GetUUID(
	IOutputDirector*	inOutput,
	int					inArgC,
	char const*			inArgV[])
{
	inOutput->printf("serverAddress=%s uuid=%s\n", settings.serverAddress, settings.uuid);

	return eCmd_Succeeded;
}
	
void
CModule_Loggly::UpdateServerData(
	void)
{
	connection = 0;

	if(settings.serverAddress[0] != 0 && settings.uuid[0] != 0)
	{
		connection = MInternetCreateHTTPConnection(settings.serverAddress, 80, CModule_Loggly::HTTPResponseHandlerMethod);
		url.Set("/inputs/%s", settings.uuid);
	}
}

