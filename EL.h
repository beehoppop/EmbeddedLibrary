#ifndef _EL_H_
#define _EL_H_
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

	This is a common header file for the library which every file includes. This allows the library to include the appropriate
	header files based on the platform and if running on the simulator
*/

#if defined(ARDUINO) && ARDUINO >= 100
	#include "arduino.h"
	#include <EEPROM.h>
	#include <SPI.h>
	#include <Wire.h>

	#define MUNUSED __attribute__((unused))

	#define MAXUINT8    255
	#define MAXINT8     127
	#define MININT8     -128

#elif defined(WIN32)
	#include "ArduinoSimulator.h"

	#define MUNUSED
#else
	#include "WProgram.h"
#endif

#endif /* _EL_H_ */
