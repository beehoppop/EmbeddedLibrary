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

#include <ctype.h>
#include <math.h>

#if !defined(WIN32)
#include <RamMonitor.h>
RamMonitor gRamMonitor;
#endif

#include "ELUtilities.h"

bool
IsStrDigit(
	char const*	inStr)
{
	if(inStr == NULL)
	{
		return false;
	}

	char const*	cp = inStr;

	for(;;)
	{
		char c = *cp++;

		if(c == 0)
		{
			return true;
		}

		if(!isdigit(c))
		{
			return false;
		}
	}

	return false;
}

bool
IsStrAlpha(
	char const*	inStr)
{
	if(inStr == NULL)
	{
		return false;
	}

	char const*	cp = inStr;

	for(;;)
	{
		char c = *cp++;

		if(c == 0)
		{
			return true;
		}

		if(!isalpha(c) && c != '_')
		{
			return false;
		}
	}

	return false;
}

void
LoadDataFromEEPROM(
	void*		inDst,
	uint16_t	inStartAddress,
	uint16_t	inSize)
{
	uint8_t*	cp = (uint8_t*)inDst;

	for(uint16_t idx = inStartAddress; idx < inStartAddress + inSize; ++idx)
	{
		*cp++ = EEPROM.read(idx);
	}
}

void
WriteDataToEEPROM(
	void*		inSrc,
	uint16_t	inStartAddress,
	uint16_t	inSize)
{
	uint8_t*	cp = (uint8_t*)inSrc;

	for(uint16_t idx = inStartAddress; idx < inStartAddress + inSize; ++idx)
	{
		EEPROM.write(idx, *cp++);
	}
}

char const*
StringizeUInt32(
	uint32_t	inValue)
{
	static char buffer[5];
	buffer[0] = (char)((inValue >> 24) & 0xFF);
	buffer[1] = (char)((inValue >> 16) & 0xFF);
	buffer[2] = (char)((inValue >> 8) & 0xFF);
	buffer[3] = (char)((inValue >> 0) & 0xFF);
	buffer[4] = 0;
	return buffer;
}

char*
strrstr(
	char const*	inStr, 
	char const* inSubStr)
{
	size_t		strLen = strlen(inStr);
	size_t		subLen = strlen(inSubStr);
	char const*	s;

	if (subLen > strLen)
	{
		return NULL;
	}

	for (s = inStr + strLen - subLen; s >= inStr; --s)
	{
		if (strncmp(s, inSubStr, subLen) == 0)
		{
			return (char*)s;
		}
	}

	return NULL;
}

void
ComputeQuadradicCoefficients(
	float	outCoefficients[3],
	float	inSamples[6])
{
	float	x0 = inSamples[0];
	float	x1 = inSamples[2];
	float	x2 = inSamples[4];
	float	sumX = x0 + x1 + x2;
	float	sumXY = x0 * inSamples[1] + x1 * inSamples[3] + x2 * inSamples[5];
	x0 *= x0;
	x1 *= x1;
	x2 *= x2;
	float	sumX2 = x0 + x1 + x2;
	float	sumX2Y = x0 * inSamples[1] + x1 * inSamples[3] + x2 * inSamples[5];
	x0 *= inSamples[0];
	x1 *= inSamples[2];
	x2 *= inSamples[4];
	float	sumX3 = x0 + x1 + x2;
	x0 *= inSamples[0];
	x1 *= inSamples[2];
	x2 *= inSamples[4];
	float	sumX4 = x0 + x1 + x2;
	float	sumY = inSamples[1] + inSamples[3] + inSamples[5];

	float	oneThird = 1.0f / 3.0f;
	float	sum2_XX = sumX2 - sumX * sumX * oneThird;
	float	sum2_XY = sumXY - sumX * sumY * oneThird;
	float	sum2_XX2 = sumX3 - sumX2 * sumX * oneThird;
	float	sum2_X2Y = sumX2Y - sumX2 * sumY * oneThird;
	float	sum2_X2X2 = sumX4 - sumX2 * sumX2 * oneThird;

	float	abDenom = 1.0f / (sum2_XX * sum2_X2X2 - sum2_XX2 * sum2_XX2);
	outCoefficients[0] = (sum2_X2Y * sum2_XX - sum2_XY * sum2_XX2) * abDenom;
	outCoefficients[1] = (sum2_XY * sum2_X2X2 - sum2_X2Y * sum2_XX2) * abDenom;
	outCoefficients[2] = sumY * oneThird - outCoefficients[1] * sumX * oneThird - outCoefficients[0] * sumX2 * oneThird;
}

float
ComputeQuadradicValue(
	float	inX,
	float	inCoefficients[3])
{
	return inX * inX * inCoefficients[0] + inX * inCoefficients[1] + inCoefficients[2];
}

void
WaitForSerialPort(
	void)
{
	for(;;)
	{
		Serial.printf("waiting for s\n");
		int ab = Serial.available();
		if(ab > 0)
		{
			char r = Serial.read();
			if(r == 's')
			{
				break;
			}
		}
		delay(1000);
	}
}

float
GetRandomFloat(
	float	inMin,
	float	inMax)
{
	float	ranf = (float)rand() / (float)RAND_MAX;

	return inMin + ranf * (inMax - inMin);
}

int32_t
GetRandomInt(
	int32_t	inMin,
	int32_t	inMax)
{
	return (rand() % (inMax - inMin)) + inMin;
}

float
GetRandomFloatGuassian(
	float	inMean,
	float	inStandardDeviation)
{
/* boxmuller.c           Implements the Polar form of the Box-Muller
                         Transformation

                      (c) Copyright 1994, Everett F. Carter Jr.
                          Permission is granted by the author to use
			  this software for any application provided this
			  copyright notice is preserved.

*/

	float x1, x2, w, y1;
	static float y2;
	static int use_last = 0;

	if (use_last)		        /* use value from previous call */
	{
		y1 = y2;
		use_last = 0;
	}
	else
	{
		do {
			x1 = 2.0f * GetRandomFloat(0.0f, 1.0f) - 1.0f;
			x2 = 2.0f * GetRandomFloat(0.0f, 1.0f) - 1.0f;
			w = x1 * x1 + x2 * x2;
		} while ( w >= 1.0 );

		w = (float)sqrt( (-2.0 * log( w ) ) / w );
		y1 = x1 * w;
		y2 = x2 * w;
		use_last = 1;
	}

	return inMean + y1 * inStandardDeviation;
}

uint32_t
GetFreeMemory(
	void)
{
#if !defined(WIN32)
	return gRamMonitor.unallocated();
#else
	return 0;
#endif
}
