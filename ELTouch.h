#ifndef _ELTOUCH_h
#define _ELTOUCH_h
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

#include <EL.h>
#include <ELModule.h>

enum
{
	eTouch_MaxEvents = 16,
};

enum ETouchEvent
{
	eTouchEvent_Touch,
	eTouchEvent_Release,
};

enum ETouchSource
{
	eTouchSource_None,
	eTouchSource_Pin,
	eTouchSource_MPR121	// This is not supported yet
};

class ITouchEventHandler
{
public:
};

typedef bool
(ITouchEventHandler::*TTouchEventMethod)(
	uint8_t		inID,
	ETouchEvent	inEvent,
	void*		inReference);

class CModule_Touch : public CModule
{
public:

	// Register an event handler method for the given pin
	void
	RegisterTouchHandler(
		uint8_t				inID,
		ETouchSource		inSource,
		ITouchEventHandler*	inObject,
		TTouchEventMethod	inMethod,
		void*				inReference);

private:
	
	CModule_Touch(
		);

	virtual void
	Setup(
		void);

	virtual void
	Update(
		uint32_t	inDeltaTimeUS);

	struct STouch
	{
		uint8_t				id;
		uint8_t				source;
		uint8_t				state;
		ITouchEventHandler*	object;
		TTouchEventMethod	method;
		void*				reference;

		uint64_t	time;

	};

	STouch*
	FindTouch(
		uint8_t	inID,
		uint8_t	inSource);

	STouch*
	FindUnused(
		void);

	int		touchCount;
	STouch	touchList[eTouch_MaxEvents];

	static CModule_Touch	module;

};

extern CModule_Touch*	gTouch;

#endif /* _ELTOUCH_h */

