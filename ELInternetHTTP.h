#ifndef _EL_INTERNETHTTP_H_
#define _EL_INTERNETHTTP_H_
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

	This is a common header file for the library which every file includes. This allows me to include the appropriate
	header files based on the platform and if I am running the API simulator on Windows (and someday OSX)
*/
#include <ELModule.h>
#include <ELInternet.h>
#include <ELRealTime.h>

#define MInternetCreateHTTPConnection(inServerPort, inServerAddress, inMethod) gInternetModule->CreateHTTPConnection(inServerPort, inServerAddress, this, static_cast<THTTPResponseHandlerMethod>(&inMethod))

enum
{
	eHTTPTimeoutMS = 10000,

};

class CHTTPConnection : public IInternetHandler, public IRealTimeHandler
{
public:
	
	void
	StartRequest(
		char const*	inVerb,
		char const*	inURL,
		int			inParameterCount = 0,
		...);							// Variable length string parameters param name and param value times inParameterCount

	void
	SetHeaders(
		int	inHeaderCount,
		...);					// Variable length string headers header name and header value times inHeaderCount

	void
	CompleteRequest(
		char const*	inBody = "");

	void
	CloseConnection(
		void);

	void
	DumpDebugInfo(
		IOutputDirector*	inOutput);

private:
	
	enum
	{
		eResponseState_None,
		eResponseState_HTTP,
		eResponseState_Headers,
		eResponseState_Body,
	};

	~CHTTPConnection(
		);

	CHTTPConnection(
		char const*							inServer,
		uint16_t							inPort,
		IInternetHandler*					inInternetHandler,
		THTTPResponseHandlerMethod			inResponseMethod);

	void
	ResponseHandlerMethod(
		EConnectionResponse	inResponse,
		uint16_t			inLocalPort,
		int					inDataSize,
		char const*			inData);

	void
	SendData(
		char const*	inData,
		bool		inFlush);
	
	void
	ProcessResponseData(
		int			inDataSize,
		char const*	inData);
	
	void
	ProcessResponseLine(
		void);
	
	void
	FinishResponse(
		void);

	void
	CheckForTimeoutEvent(
		TRealTimeEventRef	inRef,
		void*				inRefCon);

	void
	ReopenConnection(
		void);

	IInternetHandler*					internetHandler;
	THTTPResponseHandlerMethod			responseMethod;

	TString<64>	serverAddress;
	uint16_t	serverPort;
	uint16_t	localPort;

	TString<512>	tempBuffer;
	TString<256>	parameterBuffer;

	TRealTimeEventRef	eventRef;

	uint32_t	dataSentTimeMS;
	uint16_t	responseContentSize;
	uint16_t	responseHTTPCode;
	uint8_t		responseState;
	bool		openInProgress;
	bool		flushPending;
	bool		isPost;

	friend class CModule_Internet;
};

#endif /* _EL_INTERNETHTTP_H_ */
