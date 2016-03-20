#ifndef _ELDIGITALIO_h
#define _ELDIGITALIO_h
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

	This is used to handle simple digital io. Digital input events can be dispatched to handlers and digital output can be set with a time limit
*/

#include <EL.h>
#include <ELModule.h>

enum
{
	eDigitalIO_PinCount = 34,
	eDigitalIO_DefaultSettleMS = 50,
};

enum EPinInOutMode
{
	ePinMode_Unused,
	ePinMode_Input,
	ePinMode_Output,
};

enum EPinEvent
{
	eDigitalIO_PinActivated,
	eDigitalIO_PinDeactivated
};

class IDigitalIOEventHandler
{
public:
};

typedef void
(IDigitalIOEventHandler::*TDigitalIOEventMethod)(
	uint8_t		inPin,
	EPinEvent	inEvent,
	void*		inReference);

class CModule_DigitalIO : public CModule
{
public:
	
	CModule_DigitalIO(
		);
	
	// Register an event handler method for the given pin
	void
	RegisterEventHandler(
		uint8_t					inPin,
		bool					inActiveHigh,
		IDigitalIOEventHandler*	inObject,
		TDigitalIOEventMethod	inMethod,
		void*					inReference,
		uint32_t				inSettleTimeMS = eDigitalIO_DefaultSettleMS);
	
	// Set the mode and active high (or not) state for the given pin
	void
	SetPinMode(
		uint8_t			inPin,
		EPinInOutMode	inMode,
		bool			inActiveHigh);

	// Get the current state of the given pin
	bool
	GetInputState(
		uint8_t	inPin);

	// Set the output to be active for the given duration, defaults to forever
	void
	SetOutputActive(
		uint8_t		inPin,
		uint32_t	inDurationMS = 0xFFFFFFFF);

	// Set the output to be inactive
	void
	SetOutputInactive(
		uint8_t	inPin);

private:

	virtual void
	Setup(
		void);

	virtual void
	Update(
		uint32_t	inDeltaTimeUS);

	struct SPinState
	{
		uint8_t		mode;
		bool		activeHigh;
		uint8_t		lastState;
		uint32_t	settleMS;
		uint64_t	time;

		IDigitalIOEventHandler*	object;
		TDigitalIOEventMethod	method;
		void*					reference;
	};

	SPinState	pinState[eDigitalIO_PinCount];
};

extern CModule_DigitalIO*	gDigitalIO;

#endif /* _ELDIGITALIO_h */

