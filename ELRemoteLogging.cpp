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

MModuleImplementation_Start(
	CModule_Loggly,
	char const*	inGlobalTags,
	char const*	inServerAddress,
	char const*	inURL)
MModuleImplementation_Finish(CModule_Loggly, inGlobalTags, inServerAddress, inURL)

CModule_Loggly::CModule_Loggly(
	char const*	inGlobalTags,
	char const*	inServerAddress,
	char const*	inURL)
	:
	CModule(0, 0, NULL, 50000)
{
	head = tail = 0;
	globalTags = inGlobalTags;
	serverAddress = inServerAddress;
	url = inURL;
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
	char	tmpBuffer[256];

	va_list	varArgs;
	va_start(varArgs, inFormat);
	vsnprintf(tmpBuffer, sizeof(tmpBuffer), inFormat, varArgs);
	tmpBuffer[sizeof(tmpBuffer) - 1] = 0;
	va_end(varArgs);

	int tagsLen = strlen(inTags);
	while(tagsLen-- > 0)
	{
		buffer[head++ % sizeof(buffer)] = *inTags++;
	}
	buffer[head++ % sizeof(buffer)] = ':';

	int bufLen = strlen(tmpBuffer);
	char*	cp = buffer;
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
	connection = MInternetCreateHTTPConnection(serverAddress, 80, CModule_Loggly::HTTPResponseHandlerMethod);
}

void
CModule_Loggly::Update(
	uint32_t	inDeltaUS)
{
	if(gInternetModule->ConnectedToInternet() && connection != NULL && requestInProgress == false && head > tail)
	{
		char	tagsBuffer[128];
		char	msgBuffer[512];

		connection->StartRequest("POST", url);
		connection->SendHeaders(1, "content-type", "text/plain");

		char*	cmp = msgBuffer;
		char*	emp = cmp + sizeof(msgBuffer) - 1;
		
		if(buffer[tail % sizeof(buffer)] == '[')
		{
			while(tail < head)
			{
				char c = buffer[tail++ % sizeof(buffer)];
				if(c == 0)
				{
					break;
				}

				if(cmp < emp)
				{
					*cmp++ = c;
				}
			
				if(c == ' ')
				{
					break;
				}
			}
		}

		// Check to see if there are tags
		bool	hasTags = false;
		uint16_t	tmpTail = tail;
		while(tmpTail < head)
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

		tagsBuffer[0] = 0;

		#if WIN32
			strcat(tagsBuffer, "WIN32,");
		#endif

		if(gVersionStr[0] != 0)
		{
			strcat(tagsBuffer, gVersionStr);
			strcat(tagsBuffer, ",");
		}

		if(globalTags[0] != 0)
		{
			strcat(tagsBuffer, globalTags);
		}

		char*	ctp = tagsBuffer;
		char*	etp = ctp + sizeof(tagsBuffer) - 1;
		ctp += strlen(tagsBuffer);

		if(hasTags && ctp < etp)
		{
			*ctp++ = ',';
			for(;;)
			{
				char c = buffer[tail++ % sizeof(buffer)];
				if(c == ':')
				{
					++tail;
					break;
				}

				if(ctp < etp)
				{
					*ctp++ = c;
				}
			}
		}
		*ctp++ = 0;

		connection->SendHeaders(1, "X-LOGGLY-TAG", tagsBuffer);

		for(;;)
		{
			char c = buffer[tail++ % sizeof(buffer)];
			if(c == 0)
			{
				break;
			}

			if(cmp < emp)
			{
				*cmp++ = c;
			}
		}
		*cmp++ = 0;

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
