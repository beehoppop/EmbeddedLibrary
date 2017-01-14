#ifndef _TEMPERATURESENSOR_H_
#define _TEMPERATURESENSOR_H_
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

#include "EL.h"

class ITemperatureSensor
{
public:
	
	virtual void
	Destroy(
		void) = 0;

	virtual bool
	Initialize(
		void) = 0;

	virtual void
	SetSensorState(
		bool	inOn) = 0;

	virtual float
	ReadTempF(
		void) = 0;

	virtual float
	ReadTempC(
		void) = 0;

	virtual int16_t
	ReadTempC16ths(
		void) = 0;

};

ITemperatureSensor*
CreateMCP9808(
	uint8_t	inI2CAddr = 0x18);

#endif /* _TEMPERATURESENSOR_H_ */
