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

#include "ELAssert.h"
#include "ELConfig.h"
#include "ELUtilities.h"
#include "ELCommand.h"

CModule_Config*	gConfig;

CModule_Config::CModule_Config(
	)
	:
	CModule("cnfg", sizeof(configVars), 1, configVars, 0, 127)
{
	memset(configVars, 0, sizeof(configVars));
	memset(configVarUsed, 0, sizeof(configVarUsed));
}

void
CModule_Config::Setup(
	void)
{
	gConfig = this;
	MAssert(eepromOffset > 0);

	gCommand->RegisterCommand("config_set", this, static_cast<TCmdHandlerMethod>(&CModule_Config::SetConfig));
	gCommand->RegisterCommand("config_get", this, static_cast<TCmdHandlerMethod>(&CModule_Config::GetConfig));

	nodeIDIndex = RegisterConfigVar("node_id");
	debugLevelIndex = RegisterConfigVar("debug_level");
}

uint8_t
CModule_Config::GetVal(
	uint8_t	inVar)
{
	MAssert(eepromOffset > 0);
	MAssert(inVar < eConfigVar_Max);
	return configVars[inVar].value;
}

void
CModule_Config::SetVal(
	uint8_t	inVar,
	uint8_t	inVal)
{
	MAssert(eepromOffset > 0);
	MAssert(inVar < eConfigVar_Max);
	configVars[inVar].value = inVal;
	EEPROMSave();
}

uint8_t
CModule_Config::SetConfig(
	IOutputDirector*	inOutput,
	int					inArgC,
	char const*			inArgV[])
{
	if(inArgC != 3)
	{
		return eCmd_Failed;
	}

	int	targetVar = GetVarFromStr(inArgV[1]);
	int	targetVal = atoi(inArgV[2]);

	if(targetVar < 0)
	{
		return eCmd_Failed;
	}

	SetVal((uint8_t)targetVar, (uint8_t)targetVal);

	return eCmd_Succeeded;
}

uint8_t
CModule_Config::GetConfig(
	IOutputDirector*	inOutput,
	int					inArgC,
	char const*			inArgV[])
{
	if(inArgC != 2)
	{
		return eCmd_Failed;
	}
	
	int	targetVar = GetVarFromStr(inArgV[1]);

	if(targetVar < 0)
	{
		return eCmd_Failed;
	}

	inOutput->printf("%s = %d\n", inArgV[1], GetVal((uint8_t)targetVar));

	return eCmd_Succeeded;
}

int
CModule_Config::RegisterConfigVar(
	char const*	inName)
{
	MReturnOnError(strlen(inName) > eConfigVar_MaxNameLength, -1);

	int	targetIndex = -1;
	for(int i = 0; i < eConfigVar_Max; ++i)
	{
		if(strcmp(inName, configVars[i].name) == 0)
		{
			configVarUsed[i] = true;
			return i;
		}

		if(targetIndex < 0 && configVars[i].name[0] == 0)
		{
			targetIndex = i;
		}
	}

	MReturnOnError(targetIndex < 0, -1);

	configVarUsed[targetIndex] = true;
	strcpy(configVars[targetIndex].name, inName);
	configVars[targetIndex].value = 0;

	EEPROMSave();

	return targetIndex;
}

int
CModule_Config::GetVarFromStr(
	char const*	inStr)
{
	for(int i = 0; i < eConfigVar_Max; ++i)
	{
		if(strcmp(inStr, configVars[i].name) == 0)
		{
			return i;
		}
	}

	return -1;
}

void
CModule_Config::SetupFinished(
	void)
{
	for(int i = 0; i < eConfigVar_Max; ++i)
	{
		if(configVarUsed[i] == false)
		{
			// This var is no longer in use so reclaim it
			configVars[i].name[0] = 0;
		}
	}
}
