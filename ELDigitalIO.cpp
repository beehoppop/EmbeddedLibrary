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

#include <ELAssert.h>
#include <ELModule.h>
#include <ELUtilities.h>
#include <ELDigitalIO.h>

#define MDigitalDebug 0

enum 
{
	eState_WaitingForChangeToActive,
	eState_WaitingForSettleActive,
	eState_WaitingForChangeToDeactive,
	eState_WaitingForSettleDeactive,

	eUpdateTimeUS = 10000
};

CModule_DigitalIO*	gDigitalIOModule;

MModuleImplementation_Start(CModule_DigitalIO)
MModuleImplementation_FinishGlobal(CModule_DigitalIO, gDigitalIOModule)

CModule_DigitalIO::CModule_DigitalIO(
	)
	:
	CModule(0, 0, NULL, eUpdateTimeUS)
{
	memset(pinState, 0, sizeof(pinState));
}

void
CModule_DigitalIO::Update(
	uint32_t	inDeltaTimeUS)
{
	SPinState*	curState = pinState;

	for(int i = 0; i < eDigitalIO_PinCount; ++i, ++curState)
	{
		if(curState->mode == ePinMode_Input)
		{
			bool	value = digitalReadFast(i) > 0;

			switch(curState->curState)
			{
				case eState_WaitingForChangeToActive:
					if(value == curState->activeHigh)
					{
						#if MDigitalDebug
						Serial.printf("dio %d: %d %ld eState_WaitingForChangeToActive -> eState_WaitingForSettleActive\n", i, (int)gCurLocalMS, inDeltaTimeUS);
						#endif
						curState->time = gCurLocalMS;
						curState->curState = eState_WaitingForSettleActive;
					}
					break;

				case eState_WaitingForSettleActive:
					if(gCurLocalMS - curState->time >= curState->settleMS)
					{
						#if MDigitalDebug
						Serial.printf("dio %d: %d %ld eState_WaitingForSettleActive -> eState_WaitingForChangeToDeactive\n", i, (int)gCurLocalMS, inDeltaTimeUS);
						#endif
						curState->curState = eState_WaitingForChangeToDeactive;
						
						((curState->object)->*(curState->method))(i, eDigitalIO_PinActivated, curState->reference);
					}

					else if(value != curState->activeHigh)
					{
						#if MDigitalDebug
						Serial.printf("dio %d: %d %ld eState_WaitingForSettleActive -> eState_WaitingForChangeToActive\n", i, (int)gCurLocalMS, inDeltaTimeUS);
						#endif
						curState->curState = eState_WaitingForChangeToActive;
					}
					break;

				case eState_WaitingForChangeToDeactive:
					if(value != curState->activeHigh)
					{
						#if MDigitalDebug
						Serial.printf("dio %d: %d %ld eState_WaitingForChangeToDeactive -> eState_WaitingForSettleDeactive\n", i, (int)gCurLocalMS, inDeltaTimeUS);
						#endif
						curState->time = gCurLocalMS;
						curState->curState = eState_WaitingForSettleDeactive;
					}
					break;

				case eState_WaitingForSettleDeactive:
					if(gCurLocalMS - curState->time >= curState->settleMS)
					{
						#if MDigitalDebug
						Serial.printf("dio %d: %d %ld eState_WaitingForSettleDeactive -> eState_WaitingForChangeToActive\n", i, (int)gCurLocalMS, inDeltaTimeUS);
						#endif
						curState->curState = eState_WaitingForChangeToActive;
						
						((curState->object)->*(curState->method))(i, eDigitalIO_PinDeactivated, curState->reference);
					}

					else if(value == curState->activeHigh)
					{
						#if MDigitalDebug
						Serial.printf("dio %d: %d %ld eState_WaitingForSettleDeactive -> eState_WaitingForChangeToDeactive\n", i, (int)gCurLocalMS);
						#endif
						curState->curState = eState_WaitingForChangeToDeactive;
					}
					break;
			}
		}
		else if(curState->mode == ePinMode_Output)
		{
			if(curState->time > 0 && gCurLocalMS >= curState->time)
			{
				curState->time = 0;
				digitalWriteFast(i, !curState->activeHigh);
			}
		}
	}
}
	
void
CModule_DigitalIO::RegisterEventHandler(
	uint8_t					inPin,
	bool					inActiveHigh,
	IDigitalIOEventHandler*	inObject,
	TDigitalIOEventMethod	inMethod,
	void*					inReference,
	uint32_t				inSettleTimeMS)
{
	MReturnOnError(inPin >= eDigitalIO_PinCount);

	SPinState*	targetState = pinState + inPin;
	targetState->mode = ePinMode_Input;
	targetState->activeHigh = inActiveHigh;
	targetState->object = inObject;
	targetState->method = inMethod;
	targetState->reference = inReference;
	targetState->settleMS = inSettleTimeMS;
	targetState->time = 0;
	targetState->curState = eState_WaitingForChangeToActive;

	pinMode(inPin, inActiveHigh ? INPUT : INPUT_PULLUP);
}

void
CModule_DigitalIO::SetPinMode(
	uint8_t			inPin,
	EPinInOutMode	inMode,
	bool			inActiveHigh)
{
	MReturnOnError(inPin >= eDigitalIO_PinCount);

	SPinState*	targetState = pinState + inPin;

	targetState->mode = inMode;
	targetState->activeHigh = inActiveHigh;
	targetState->object = NULL;
	targetState->method = NULL;
	targetState->reference = NULL;
	targetState->settleMS = 0;
	targetState->time = 0;
	targetState->curState = 0;

	if(inMode == ePinMode_Input)
	{
		pinMode(inPin, inActiveHigh ? INPUT : INPUT_PULLUP);
	}
	else if(inMode == ePinMode_Output)
	{
		pinMode(inPin, OUTPUT);
	}
}

bool
CModule_DigitalIO::GetInputState(
	uint8_t	inPin)
{
	MReturnOnError(inPin >= eDigitalIO_PinCount, false);

	SPinState*	targetState = pinState + inPin;
	bool	value = digitalReadFast(inPin) > 0;
	
	return value == targetState->activeHigh;
}

void
CModule_DigitalIO::SetOutputActive(
	uint8_t		inPin,
	uint32_t	inDurationMS)
{
	MReturnOnError(inPin >= eDigitalIO_PinCount);

	SPinState*	targetState = pinState + inPin;

	digitalWriteFast(inPin, targetState->activeHigh);

	if(inDurationMS < 0xFFFFFFFF)
	{
		targetState->time = gCurLocalMS + inDurationMS;
	}
	else
	{
		targetState->time = 0;
	}
}

void
CModule_DigitalIO::SetOutputInactive(
	uint8_t	inPin)
{
	MReturnOnError(inPin >= eDigitalIO_PinCount);

	SPinState*	targetState = pinState + inPin;

	digitalWriteFast(inPin, !targetState->activeHigh);
	targetState->time = 0;
}

void
CModule_DigitalIO::SetOutputState(
	uint8_t		inPin,
	bool		inState)
{
	MReturnOnError(inPin >= eDigitalIO_PinCount);

	SPinState*	targetState = pinState + inPin;

	digitalWriteFast(inPin, inState ? targetState->activeHigh : !targetState->activeHigh);
	targetState->time = 0;
}
