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

#include "ELAssert.h"
#include "ELConfig.h"
#include "ELUtilities.h"

CModule_Config*	gConfig;
CModule_Config	CModule_Config::module;

CModule_Config::CModule_Config(
	)
	:
	CModule("cnfg", sizeof(configVar), 0, 0, 255)
{
	gConfig = this;
}

void
CModule_Config::Setup(
	void)
{
	MAssert(eepromOffset > 0);

	LoadDataFromEEPROM(configVar, eepromOffset, sizeof(configVar));
}

void
CModule_Config::ResetState(
	void)
{
}

uint8_t
CModule_Config::GetVal(
	uint8_t	inVar)
{
	MAssert(eepromOffset > 0);
	MAssert(inVar < eConfigVar_Max);
	return configVar[inVar];
}

void
CModule_Config::SetVal(
	uint8_t	inVar,
	uint8_t	inVal)
{
	MAssert(eepromOffset > 0);
	MAssert(inVar < eConfigVar_Max);
	configVar[inVar] = inVal;
	EEPROM.write(eepromOffset + inVar, inVal);
}
