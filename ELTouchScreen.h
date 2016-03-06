#ifndef _ELTOUCHSCREEN_H_
#define _ELTOUCHSCREEN_H_
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

#include <ELModule.h>

enum ETouchScreenEvent
{
	eTouchScreenEvent_Down,
	eTouchScreenEvent_Up
};

enum
{
	eTouchScreen_MaxHandlers = 16
};

// 0, 0 is bottom left, values increase to the top and right
struct STouchScreenRect
{
	int	left, right, top, bottom;	// bottom, left inclusive, top, right exclusive
};

// A dummy class for the touchscreen handler method object
class ITouchScreenHandler
{
public:
};

// The typedef for the command handler method
typedef void
(ITouchScreenHandler::*TTouchScreenHandlerMethod)(
	ETouchScreenEvent	inEvent,
	int					inX,
	int					inY,
	void*				inRefCon);

class ITouchScreenProvider
{
public:

	virtual bool
	GetTouch(
		int&	outTouchX,
		int&	outTouchY) = 0;

};

class CModule_TouchScreen
{
public:
	
	void
	SetTouchScreenProvider(
		ITouchScreenHandler*	inProvider);

	void
	RegisterEventHandler(
		STouchScreenRect const&		inRect,
		ITouchScreenHandler*		inHandler,
		TTouchScreenHandlerMethod	inMethod,
		void*						inRefCont);

private:
	
	CModule_TouchScreen(
		);

	virtual void
	Setup(
		void);

	virtual void
	Update(
		uint32_t	inDeltaTimeUS);

	struct STouchHandler
	{
		STouchScreenRect			rect;
		ITouchScreenHandler*		object;
		TTouchScreenHandlerMethod	method;
		void*						refCon;
	};

	STouchHandler	handlerList[eTouchScreen_MaxHandlers];

	static CModule_TouchScreen	module;

};

extern CModule_TouchScreen*	gTouchScreen;

#endif /* _ELTOUCHSCREEN_H_ */
