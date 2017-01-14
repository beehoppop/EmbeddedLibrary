#ifndef _ELOUTPUT_H_
#define _ELOUTPUT_H_

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

#include <EL.h>
#include <ELString.h>

#define MOutputDirectorOrSerial(inOutput, ...) if(inOutput != NULL) { inOutput->printf(__VA_ARGS__); } else { Serial.printf(__VA_ARGS__); }

class IOutputDirector
{
public:

	virtual void
	printf(
		char const*	inMsg,
		...)
	{
		va_list	varArgs;
		va_start(varArgs, inMsg);
		TString<512>	vabuffer;
		vabuffer.SetVA(inMsg, varArgs);
		va_end(varArgs);
		
		write(vabuffer);
	}

	virtual void
	write(
		char const*	inMsg)
	{
		write(inMsg, strlen(inMsg));
	}

	virtual void
	write(
		char const*	inMsg,
		size_t		inBytes) = 0;

};

#endif /* _ELOUTPUT_H_ */
