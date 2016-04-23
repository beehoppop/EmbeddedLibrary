#include "ELRemoteLogging.h"

CModule_Loggly::CModule_Loggly(
	char const*	inGlobalTags,
	char const*	inServerAddress,
	char const*	inURL)
	:
	CModule("lgly", 0, 0, NULL, 50000, -1)
{
	head = tail = 0;
	globalTags = inGlobalTags;
	serverAddress = inServerAddress;
	url = inURL;
	requestInProgress = false;
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
	connection = gInternet->CreateHTTPConnection(serverAddress, 80, this, static_cast<THTTPResponseHandlerMethod>(&CModule_Loggly::HTTPResponseHandlerMethod));
}

void
CModule_Loggly::Update(
	uint32_t	inDeltaUS)
{
	if(requestInProgress == false && head > tail)
	{
		char	tagsBuffer[128];
		char	msgBuffer[256];

		connection->StartRequest("POST", url);
		connection->SendHeaders(1, "content-type", "text/plain");

		// build the tags

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

		if(hasTags || globalTags[0] != 0)
		{
			char*	cp = tagsBuffer;
			char*	ep = cp + sizeof(tagsBuffer) - 1;
			strncpy(cp, globalTags, sizeof(tagsBuffer));
			tagsBuffer[sizeof(tagsBuffer) - 1] = 0;
			cp += strlen(tagsBuffer);

			if(hasTags && cp < ep)
			{
				*cp++ = ',';
				for(;;)
				{
					char c = buffer[tail++ % sizeof(buffer)];
					if(c == ':')
					{
						++tail;
						break;
					}

					if(cp < ep)
					{
						*cp++ = c;
					}
				}
			}
			*cp++ = 0;

			connection->SendHeaders(1, "X-LOGGLY-TAG", tagsBuffer);
		}

		char*	cp = msgBuffer;
		char*	ep = cp + sizeof(msgBuffer) - 1;
		for(;;)
		{
			char c = buffer[tail++ % sizeof(buffer)];
			if(c == 0)
			{
				break;
			}

			if(cp < ep)
			{
				*cp++ = c;
			}
		}
		*cp++ = 0;

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
