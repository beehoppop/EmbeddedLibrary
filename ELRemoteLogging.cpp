#include "ELRemoteLogging.h"
#include "ELAssert.h"

MModuleImplementation_Start(
	CModule_Loggly,
	char const*	inGlobalTags,
	char const*	inServerAddress,
	char const*	inURL)
MModuleImplementation(CModule_Loggly, inGlobalTags, inServerAddress, inURL)

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
	DoneIncluding();
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
	connection = gInternetModule->CreateHTTPConnection(serverAddress, 80, this, static_cast<THTTPResponseHandlerMethod>(&CModule_Loggly::HTTPResponseHandlerMethod));
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

		if(hasTags || globalTags[0] != 0)
		{
			char*	ctp = tagsBuffer;
			char*	etp = ctp + sizeof(tagsBuffer) - 1;
			strncpy(ctp, globalTags, sizeof(tagsBuffer));
			tagsBuffer[sizeof(tagsBuffer) - 1] = 0;
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
		}

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
