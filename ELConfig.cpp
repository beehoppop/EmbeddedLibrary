/*
	ELModule.cpp
*/

#include "ELAssert.h"
#include "ELConfig.h"
#include "ELUtilities.h"

CModule_Config::CModule_Config(
	)
	:
	CModule("cnfg", sizeof(configVar), 0, 255)
{
	setupComplete = false;
}

void
CModule_Config::Setup(
	void)
{
	if(setupComplete)
	{
		return;
	}

	MAssert(eepromOffset > 0);

	LoadDataFromEEPROM(configVar, eepromOffset, sizeof(configVar));

	setupComplete = true;
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
	Setup();
	MAssert(inVar < eConfigVar_Max);
	return configVar[inVar];
}

void
CModule_Config::SetVal(
	uint8_t	inVar,
	uint8_t	inVal)
{
	Setup();
	MAssert(inVar < eConfigVar_Max);
	configVar[inVar] = inVal;
	EEPROM.write(eepromOffset + inVar, inVal);
}

CModule_Config	gConfig;
