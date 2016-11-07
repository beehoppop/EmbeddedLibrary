#ifndef _EL_REMOTELOGGING_H_
#define _EL_REMOTELOGGING_H_
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

#include "ELModule.h"
#include "ELOutput.h"
#include "ELInternet.h"
#include "ELCommand.h"

class CModule_Loggly : public CModule, public IOutputDirector, IInternetHandler, public ICmdHandler
{
public:
	
	MModule_Declaration(
		CModule_Loggly,
		char const*	inGlobalTags)		// Must be a static string

	virtual void
	write(
		char const*	inMsg,
		size_t		inBytes);

	void
	SendLog(
		char const*	inTags,
		char const*	inFormat,
		...);

private:

	CModule_Loggly(
		char const*	inGlobalTags);		// Must be a static string
	
	virtual void
	Setup(
		void);

	uint16_t
	GetQueueLength(
		void);

	uint16_t
	GetQueueLengthFromGivenTail(
		uint16_t	inTail);
	
	virtual void
	Update(
		uint32_t	inDeltaUS);

	void
	HTTPResponseHandlerMethod(
		uint16_t			inHTTPReturnCode,
		int					inDataSize,
		char const*			inData);
	
	uint8_t
	Command_SetUUID(
		IOutputDirector*	inOutput,
		int					inArgC,
		char const*			inArgV[]);
	
	uint8_t
	Command_GetUUID(
		IOutputDirector*	inOutput,
		int					inArgC,
		char const*			inArgV[]);
	
	void
	UpdateServerData(
		void);

	struct SSettings
	{
		char	uuid[48];
		char	serverAddress[64];
	};

	SSettings			settings;
	TString<64>			url;
	uint16_t			head;
	uint16_t			tail;
	char				buffer[2048];
	char const*			globalTags;
	bool				requestInProgress;
	CHTTPConnection*	connection;
};

extern CModule_Loggly*	gLoggly;

#endif /* _EL_REMOTELOGGING_H_ */
