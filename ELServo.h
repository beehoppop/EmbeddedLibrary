#ifndef _ELSERVO_H_
#define _ELSERVO_H_

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

	This is a generalized interface to any servo. A specific servo implementation is responsible for mapping the concepts in this API
	to its particular mechanism.

	Servo position: Think of the servo in its natural orientation facing straight at you as drawn in its schematic. Attach a hand to the shaft
		like a clock hour hand. At 0 degrees the hand is pointing straight up (12 o'clock), at 90 degrees it is pointing right (3 o'clock),at 180 
		degrees it is pointing straight down (6 o'clock), at 270 degrees it is pointing left (9 o'clock). Since it is often convenient to think
		of servo position as moving either clockwise (right) or counterclockwise (left) from the straight up position the API interprets a 0 
		position to be straight up, a 0.5 position to be 90 degrees, a -0.5 position to be 270 degrees, and 1.0 or -1.0 to be 180 degrees.
		When moving from a -0.5 to 0.5 or vice versa the servo will always move through the 0 position. 

		Not all servo's can go to all positions on the clock.

*/

#include <EL.h>

// This is a convenience macro for registering commands
#define MServoCallbackRegister(inServoInterface, inCallbackMethod, inCallbackReference) inServoInterface->RegisterCallback(this, static_cast<TCallbackHandlerMethod>(&inCallbackMethod), inCallbackReference)

enum EServoCallbackEvent
{
	eCallbackEvent_ReachedGoal,
	eCallbackEvent_GoalUnreachable,
	eCallbackEvent_UnknownError
};

// A dummy class for the command handler method object
class IServoHandler
{
public:
};

// The typedef for the command handler method
typedef uint8_t
(IServoHandler::*TCallbackHandlerMethod)(
	uint32_t			inRefCon,
	EServoCallbackEvent	inEvent);

class IServo
{
public:

	// Return true if the servo is moving under its own power
	virtual bool
	IsSelfMoving(
		void) = 0;

	// Return true if the servo is being moved externally
	virtual bool
	IsBeingMoved(
		void) = 0;

	// Return the current distance from the goal
	virtual float
	GetDistanceFromGoal(
		void) = 0;

	// Return the current position
	virtual float
	GetPosition(
		void) = 0;

	// Return the current speed
	virtual float
	GetSpeed(
		void) = 0;

	// Return the current load
	virtual float
	GetLoad(
		void) = 0;

	// Return the servo's max counterclockwise and clockwise position from 0 (hand pointing straight up at 12 o'clock)
	virtual void
	GetPositionLimits(
		float&	outCounterclockwise,
		float&	outClockwise) = 0;
	
	// Set the servo in continuous rotation mode or not
	virtual void
	SetContinuousRotation(
		bool	inContinuousRotation,
		bool	inImmediate = true) = 0;

	// Set the torque mode
	virtual void
	SetTorque(
		float	inTorque,	// 0 is lowest torque/highest max speed, 1 is highest torque/lowest max speed
		bool	inImmediate = true) = 0;

	// Set the torque mode
	virtual void
	SetContinuousSpeed(
		float	inSpeed,	// 0 is stop, 1 is highest CW speed, -1 is highest CCW speed
		bool	inImmediate = true) = 0;

	// Tell the servo to move to the absolute current position
	virtual void
	SetGoalPosition(
		float	inPosition,	// See position description above
		float	inSpeed,	// 0 is slowest speed, 1 is fastest speed
		bool	inImmediate = true) = 0;

	// Flush all pending actions
	virtual void
	Flush(
		void) = 0;

	// Register a callback for async events such as reaching the goal position or an error
	virtual void
	RegisterCallback(
		IServoHandler*			inObject,
		TCallbackHandlerMethod	inMethod,
		uint32_t				inRefCon) = 0;
};

#endif /* _ELSERVO_H_ */
