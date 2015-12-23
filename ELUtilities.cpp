/*
	Author: Brent Pease

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
