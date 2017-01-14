#ifndef _ELUTILITIES_H_
#define _ELUTILITIES_H_
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

#include "EL.h"

#define MMax(x, y) (((x) > (y)) ? (x) : (y))
#define MMin(x, y) (((x) < (y)) ? (x) : (y))
#define MPin(inMin, inValue, inMax) ((inValue) < (inMin) ? (inMin) : (inValue) > (inMax) ? (inMax) : (inValue))
#define MStaticArrayLength(a) (sizeof(a) / sizeof(a[0]))
#define MStaticArrayDelete(a, i) memmove(a + (i), a + (i) + 1, (MStaticArrayLength(a) - (i) - 1) * sizeof(a[0]))



bool
IsStrDigit(
	char const*	inStr);

bool
IsStrAlpha(
	char const*	inStr);

void
LoadDataFromEEPROM(
	void*		inDst,
	uint16_t	inStartAddress,
	uint16_t	inSize);

void
WriteDataToEEPROM(
	void*		inSrc,
	uint16_t	inStartAddress,
	uint16_t	inSize);

char const*
StringizeUInt32(
	uint32_t	inValue);

char*
strrstr(
	char const*	inStr, 
	char const* inSubStr);

// Compute the coefficients of y = a * x ^ 2 + b * x + c
void
ComputeQuadradicCoefficients(
	float	outCoefficients[3],	// 0 = a, 1 = b, 2 = c
	float	inSamples[6]);		// 3 samples of x, y (x0, y0, x1, y1, x2, y2)

// Compute the dependent variable y given x and the 3 coefficients
float
ComputeQuadradicValue(
	float	inX,
	float	inCoefficients[3]);

void
WaitForSerialPort(
	void);

float
GetRandomFloat(
	float	inMin,
	float	inMax);

int32_t
GetRandomInt(
	int32_t	inMin,
	int32_t	inMax);

float
GetRandomFloatGuassian(
	float	inMean,
	float	inStandardDeviation);

inline float
ReMapValue(
	float	inValue,
	float	inValueMin,
	float	inValueMax,
	float	inNewRangeMin,
	float	inNewRangeMax)
{
	return inNewRangeMin + (inValue - inValueMin) / (inValueMax - inValueMin) * (inNewRangeMax - inNewRangeMin);
}

bool
BufferEndsWithStr(
	char const*	inBuffer,
	size_t		inBufferSize,
	char const*	inStr);

/*
float
InterpolateValues(
	float	inStart,
	float	inEnd,
	float	inProgression,
*/

#endif /* _ELUTILITIES_H_ */
