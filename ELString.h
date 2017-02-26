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
		va_list	vaList;
		va_start(vaList, inString);
		SetVA(inString, vaList);
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
		Set(inString);

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
	operator +=(
		char inC)
	{
		return Append(inC);
	}

	// Since overloading [] produces abmiguous overload warnings use () instead
	char
	operator()(
		size_t	inIndex)
	{
		if(inIndex >= length)
		{
			return -1;
		}
		return buffer[inIndex];
	}

	bool
	AppendF(
		char const*	inString,
		...)
	{
		va_list	vaList;
		va_start(vaList, inString);
		bool result = AppendVA(inString, vaList);
		va_end(vaList);
		return result;
	}

	bool
	Append(
		char const*	inString)
	{
		size_t	strLen = strlen(inString);

		if(length + strLen + 1 <= inBufferSize)
		{
			memcpy(buffer + length, inString, strLen + 1);
			length += strLen;
			return true;
		}

		memcpy(buffer + length, inString, inBufferSize - length - 1);
		length = inBufferSize - 1;
		buffer[inBufferSize - 1] = 0;	// Ensure we have a valid cstring
		return false;
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
	AppendVA(
		char const*	inString,
		va_list		inVAList)
	{
		int	result;

		result = vsnprintf(buffer + length, inBufferSize - length, inString, inVAList);
		if(result < 0 || (size_t)result >= inBufferSize - length)
		{
			// we have overflowed
			length = inBufferSize - 1;
			buffer[inBufferSize - 1] = 0;	// Ensure we have a valid cstring
			return false;
		}

		length += result;

		return true;
	}

	bool
	Insert(
		size_t		inPos,
		char const*	inString)
	{
		size_t	strLen = strlen(inString);

		if(inPos >= inBufferSize)
		{
			return false;
		}

		if(inPos + strLen >= inBufferSize)
		{
			length = inBufferSize - 1;
			strLen = length - inPos;
			memcpy(buffer + inPos, inString, strLen);
			buffer[inBufferSize - 1] = 0;	// Ensure we have a valid cstring
			return false;
		}
		
		if(strLen + length >= inBufferSize)
		{
			length = inBufferSize - 1;
			memmove(buffer + inPos + strLen, buffer + inPos, length - strLen - inPos);
			memcpy(buffer + inPos, inString, strLen);
			buffer[inBufferSize - 1] = 0;	// Ensure we have a valid cstring
			return false;
		}

		memmove(buffer + inPos + strLen, buffer + inPos, length - inPos);
		memcpy(buffer + inPos, inString, strLen);

		length += strLen;
		buffer[length] = 0;

		return true;
	}

	bool
	SetF(
		char const*	inString,
		...)
	{
		va_list	vaList;
		va_start(vaList, inString);
		bool result = SetVA(inString, vaList);
		va_end(vaList);
		return result;
	}

	bool
	Set(
		char const*	inString)
	{
		if(inString == NULL)
		{
			buffer[0] = 0;
			length = 0;
			return true;
		}

		size_t	strLen = strlen(inString);
		bool	overflow = strLen > inBufferSize - 1;

		if(overflow)
		{
			strLen = inBufferSize - 1;
		}

		memcpy(buffer, inString, strLen);
		buffer[strLen] = 0;
		length = strLen;

		return overflow;
	}

	bool
	SetVA(
		char const*	inString,
		va_list		inVAList)
	{
		int	result;

		result = vsnprintf(buffer, inBufferSize, inString, inVAList);
		if(result < 0 || (size_t)result >= inBufferSize)
		{
			// we have overflowed
			length = inBufferSize - 1;
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

	size_t
	GetSize(
		void)
	{
		return inBufferSize - 1;
	}

	bool
	StartsWith(
		char const*	inString)
	{
		size_t	strLen = strlen(inString);

		return strncmp(buffer, inString, strLen) == 0;
	}

	bool
	EndsWith(
		char	inChar)
	{
		return buffer[length - 1] == inChar;
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

	bool
	IsSpace(
		void)
	{
		for(size_t i = 0; i < length; ++i)
		{
			if(!isspace(buffer[i]))
			{
				return false;
			}
		}

		return true;
	}

	bool
	IsDigit(
		void)
	{
		for(size_t i = 0; i < length; ++i)
		{
			if(!isdigit(buffer[i]))
			{
				return false;
			}
		}

		return true;
	}

	bool
	IsNumber(
		void)
	{
		for(size_t i = 0; i < length; ++i)
		{
			if(!(isdigit(buffer[i]) || buffer[i] == '-' || buffer[i] == '+'))
			{
				return false;
			}
		}

		return true;
	}

private:

	char	buffer[inBufferSize];
	size_t	length;
};

#endif /* _ELSTRING_H_ */
