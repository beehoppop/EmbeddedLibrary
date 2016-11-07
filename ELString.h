#ifndef _ELSTRING_H_
#define _ELSTRING_H_
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
	A fixed length string buffer that can not be overflowed.
*/

#include "EL.h"

template <size_t inBufferSize> class TString
{
public:
	TString(
		)
	{
		Clear();
	}

	TString(
		char const*	inString,
		...)
	{
		Clear();

		va_list	vaList;
		va_start(vaList, inString);
		AppendVar(inString, vaList);
		va_end(vaList);
	}

	operator char*(
		)
	{
		return buffer;
	}

	operator char const*(
		)
	{
		return buffer;
	}

	bool
	operator ==(
		char const*	inString)
	{
		return strcmp(buffer, inString) == 0;
	}

	TString<inBufferSize>&
	operator =(
		char const*	inString)
	{
		size_t	strLen = strlen(inString);

		if(strLen > inBufferSize - 1)
		{
			strLen = inBufferSize - 1;
		}

		memcpy(buffer, inString, strLen);
		buffer[strLen] = 0;
		length = strLen;

		return *this;
	}

	TString<inBufferSize>&
	operator +=(
		char const*	inString)
	{
		Append(inString);
		return *this;
	}

	bool
	Append(
		char const*	inString,
		...)
	{
		va_list	vaList;
		va_start(vaList, inString);
		bool result = AppendVar(inString, vaList);
		va_end(vaList);
		return result;
	}

	bool
	Append(
		char inC)
	{
		if(length >= inBufferSize - 1)
		{
			return false;
		}

		buffer[length++] = inC;
		buffer[length] = 0;	// Ensure we always have a valid cstring

		return true;
	}

	bool
	AppendVar(
		char const*	inString,
		va_list		inVAList)
	{
		int	result;

		result = vsnprintf(buffer + length, inBufferSize - length, inString, inVAList);
		if(result < 0 || (size_t)result >= inBufferSize - length)
		{
			// we have overflowed
			length = inBufferSize;
			buffer[inBufferSize - 1] = 0;	// Ensure we have a valid cstring
			return false;
		}

		length += result;

		return true;
	}

	bool
	Set(
		char const*	inString,
		...)
	{
		va_list	vaList;
		va_start(vaList, inString);
		bool result = SetVar(inString, vaList);
		va_end(vaList);
		return result;
	}

	bool
	SetVar(
		char const*	inString,
		va_list		inVAList)
	{
		int	result;

		result = vsnprintf(buffer, inBufferSize, inString, inVAList);
		if(result < 0 || (size_t)result >= inBufferSize - length)
		{
			// we have overflowed
			length = inBufferSize;
			buffer[inBufferSize - 1] = 0;	// Ensure we have a valid cstring
			return false;
		}

		length = result;

		return true;
	}

	void
	Clear(
		void)
	{
		length = 0;
		buffer[0] = 0;
	}

	size_t
	GetLength(
		void)
	{
		return length;
	}

	bool
	StartsWith(
		char const*	inString)
	{
		size_t	strLen = strlen(inString);

		return strncmp(buffer, inString, strLen) == 0;
	}

	bool
	TrimAfterChr(
		char	inC)
	{
		char*	charPtr = strchr(buffer, inC);
		if(charPtr == NULL)
		{
			return false;
		}
		*charPtr = 0;
		length = strlen(buffer);
		return true;
	}

	bool
	TrimBeforeChr(
		char	inC)
	{
		char*	charPtr = strchr(buffer, inC);
		if(charPtr == NULL)
		{
			return false;
		}
		length = buffer + length - charPtr;
		memmove(buffer, charPtr + 1, length + 1);
		return true;
	}

	void
	TrimStartingSpace(
		void)
	{
		char*	nonSpaceStr = buffer;
		while(isspace(*nonSpaceStr))
		{
			++nonSpaceStr;
		}
		length = buffer + length - nonSpaceStr;
		memmove(buffer, nonSpaceStr, length + 1);
	}

	void
	TrimUntilNextWord(
		void)
	{
		char*	sp = buffer;
		while(!isspace(*sp))
		{
			++sp;
		}
		while(isspace(*sp))
		{
			++sp;
		}
		length = buffer + length - sp;
		memmove(buffer, sp, length + 1);
	}

	bool
	SetChar(
		size_t	inPos,
		char	inC)
	{
		if(inPos >= length)
		{
			return false;
		}

		buffer[inPos] = inC;
		return true;
	}

	bool
	Contains(
		char const*	inString)
	{
		return strstr(buffer, inString) != NULL;
	}

private:

	char	buffer[inBufferSize];
	size_t	length;
};

#endif /* _ELSTRING_H_ */
