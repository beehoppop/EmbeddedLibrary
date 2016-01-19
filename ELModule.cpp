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
#include "ELModule.h"
#include "ELUtilities.h"
#include "ELConfig.h"

enum
{
	eEEPROM_VersionOffset = 0,
	eEEPROM_ListStart = 1,

	eEEPROM_Version = 101,

	eEEPROM_Size = 2048

};

#define MDebugDelayEachModule 0
#define MDebugDelayStart 0
#define MDebugTargetNode 0xFF

struct SEEPROMEntry
{
	uint32_t	uid;
	uint16_t	offset;
	uint16_t	size;
	uint16_t	version;
	bool		inUse;
};

static int			gModuleCount;
static CModule*		gModuleList[eMaxModuleCount];
static SEEPROMEntry	gEEPROMEntryList[eMaxModuleCount];
static bool			gTooManyModules = false;
static bool			gTearingDown = false;
static uint32_t		gLastMillis;
static uint32_t		gLastMicros;
static bool			gFlashLED;
static int			gBlinkLEDIndex;		// config index for blink LED config var

uint64_t	gCurLocalMS;
uint64_t	gCurLocalUS;

char const*	gVersionStr;

class CModuleManager : public CModule, public ICmdHandler
{
	CModuleManager(
		)
		:
		CModule("mdmg", 0, 0, 0, 253)
	{
	}

	virtual void
	Setup(
		void)
	{
		gCommand->RegisterCommand("alive", this, static_cast<TCmdHandlerMethod>(&CModuleManager::SerialCmdAlive));
		gBlinkLEDIndex = gConfig->RegisterConfigVar("blink_led");
	}

	uint8_t
	SerialCmdAlive(
		IOutputDirector*	inOutput,
		int					inArgC,
		char const*			inArgV[])
	{
		inOutput->printf("ALIVE node=%d ver=%s build date=%s %s\n", gConfig->GetVal(gConfig->nodeIDIndex), gVersionStr, __DATE__, __TIME__);
		
		return eCmd_Succeeded;
	}

	static CModuleManager	module;
};
CModuleManager	CModuleManager::module;

CModule::CModule(
	char const*	inUID,
	uint16_t	inEEPROMSize,
	uint16_t	inEEPROMVersion,
	void*		inEEPROMData,
	uint32_t	inUpdateTimeUS,
	uint8_t		inPriority,
	bool		inEnabled)
	:
	uid((inUID[0] << 24) | (inUID[1] << 16) | (inUID[2] << 8) | inUID[3]),
	eepromSize(inEEPROMSize),
	eepromVersion(inEEPROMVersion),
	eepromData(inEEPROMData),
	updateTimeUS(inUpdateTimeUS),
	priority(inPriority),
	enabled(inEnabled)
{
	if(gModuleCount >= eMaxModuleCount)
	{
		gTooManyModules = true;
		return;
	}

	gModuleList[gModuleCount++] = this;
}

void
CModule::Setup(
	void)
{

}

void
CModule::TearDown(
	void)
{

}

bool
CModule::GetEnabledState(
	void)
{
	return enabled;
}

void
CModule::SetEnabledState(
	bool	inEnabled)
{
	enabled = inEnabled;

	if(enabled && !hasBeenSetup)
	{
		Setup();
		lastUpdateUS = gCurLocalUS;
		hasBeenSetup = true;
	}
}

void
CModule::Update(
	uint32_t	inDeltaTimeUS)
{
	//Serial.printf("default %s %d\n", StringizeUInt32(uid), enabled);
}
	
void
CModule::ResetState(
	void)
{
	EEPROMInitialize();
}

void
CModule::EEPROMInitialize(
	void)
{
	if(eepromData != NULL && eepromSize > 0)
	{
		memset(eepromData, 0, eepromSize);
	}
}

void
CModule::EEPROMSave(
	void)
{
	if(eepromData != NULL && eepromSize > 0)
	{
		WriteDataToEEPROM(eepromData, eepromOffset, eepromSize);
	}
}

static SEEPROMEntry*
FindEEPROMEntry(
	uint32_t		inUID)
{
	for(int i = 0; i < eMaxModuleCount; ++i)
	{
		if(gEEPROMEntryList[i].uid == inUID)
			return gEEPROMEntryList + i;
	}

	return NULL;
}

void
CModule::SetupAll(
	char const*	inVersionStr,
	bool		inFlashLED)
{
	gVersionStr = inVersionStr;
	gFlashLED = inFlashLED;
	if(inFlashLED)
	{
		pinMode(13, OUTPUT);
		digitalWrite(13, 1);
	}

	Serial.begin(115200);

	DebugMsg(eDbgLevel_Basic, "Module count=%d\n", gModuleCount);

	MAssert(gTooManyModules == false);	// This is reported here since we can't safely access the serial port in constructors

	bool	changes = false;
	uint8_t	eepromVersion = EEPROM.read(eEEPROM_VersionOffset);
	if(eepromVersion == eEEPROM_Version)
	{
		LoadDataFromEEPROM(gEEPROMEntryList, eEEPROM_ListStart, sizeof(gEEPROMEntryList));
		for(int i = 0; i < eMaxModuleCount; ++i)
		{
			gEEPROMEntryList[i].inUse = false;
		}

		int	totalEEPROMSize = eEEPROM_ListStart + sizeof(gEEPROMEntryList);
		for(int i = 0; i < gModuleCount; ++i)
		{
			CModule*	curModule = gModuleList[i];
			if(curModule->eepromSize == 0)
				continue;
		
			MAssert(curModule->eepromData != NULL);	// If the module needs eeprom storage it must provide local memory for it

			SEEPROMEntry*	target = FindEEPROMEntry(curModule->uid);

			if(target == NULL)
			{
				DebugMsg(eDbgLevel_Medium, "Module %s: no eeprom entry\n", StringizeUInt32(curModule->uid));
				changes = true;
			}
			else if(target->size != curModule->eepromSize || target->version != curModule->eepromVersion)
			{
				DebugMsg(eDbgLevel_Medium, "Module %s: eeprom changed version or size\n", StringizeUInt32(curModule->uid));
				changes = true;
			}
			else
			{
				curModule->eepromOffset = target->offset;
				LoadDataFromEEPROM(curModule->eepromData, curModule->eepromOffset, curModule->eepromSize);
				target->inUse = true;
			}

			totalEEPROMSize += curModule->eepromSize;
		}
		MAssert(totalEEPROMSize <= eEEPROM_Size);
	}
	else
	{
		DebugMsg(eDbgLevel_Basic, "EEPROM version mismatch old=%d new=%d\n", eepromVersion, eEEPROM_Version);

		changes = true;
		for(int i = 0; i < eMaxModuleCount; ++i)
		{
			gEEPROMEntryList[i].inUse = false;
		}
	}

	if(changes)
	{
		// since changes have been made compress all module eeprom data into low eeprom space
		uint16_t		curOffset = eEEPROM_ListStart + sizeof(gEEPROMEntryList);
		SEEPROMEntry*	curEEPROM = gEEPROMEntryList;
		for(int i = 0; i < gModuleCount; ++i)
		{
			CModule*	curModule = gModuleList[i];
			if(curModule->eepromSize == 0)
				continue;

			curModule->eepromOffset = curOffset;
			curEEPROM->uid = curModule->uid;
			curEEPROM->offset = curOffset;
			curEEPROM->size = curModule->eepromSize;
			curEEPROM->version = curModule->eepromVersion;
			if(!curEEPROM->inUse)
			{
				DebugMsg(eDbgLevel_Medium, "Module %s: Initializing eeprom\n", StringizeUInt32(curModule->uid));
				curModule->EEPROMInitialize();
			}
			WriteDataToEEPROM(curModule->eepromData, curOffset, curModule->eepromSize);
			++curEEPROM;
			curOffset += curModule->eepromSize;
		}

		WriteDataToEEPROM(gEEPROMEntryList, eEEPROM_ListStart, sizeof(gEEPROMEntryList));
		EEPROM.write(eEEPROM_VersionOffset, eEEPROM_Version);
	}

	#if MDebugDelayStart
		if(MDebugTargetNode == 0xFF /*|| MDebugTargetNode == gConfig->GetVal(eConfigVar_NodeID)*/)	// 		Need to fix this code to not reference gConfig since it has not been initialized yet
		{
			delay(6000);
		}
	#endif

	gCurLocalMS = millis();
	gCurLocalUS = micros();

	for(int priorityItr = 256; priorityItr-- > 0;)
	{
		for(int i = 0; i < gModuleCount; ++i)
		{
			CModule*	curModule = gModuleList[i];
			if(curModule->priority == priorityItr)
			{
				if(!curModule->enabled)
				{
					// Don't try to setup modules that are not enabled
					continue;
				}

				#if MDebugDelayEachModule || MDebugDelayStart
					DebugMsg(eDbgLevel_Medium, "Module: Setup %s\n", StringizeUInt32(curModule->uid));
				#endif
				#if MDebugDelayEachModule
					//Need to fix this code to not reference gConfig since it has not been initialized yet
					//if(MDebugTargetNode == 0xFF || MDebugTargetNode == gConfig->GetVal(eConfigVar_NodeID))
					{
						delay(3000);
					}
				#endif
				curModule->Setup();
				curModule->lastUpdateUS = gCurLocalUS;
				curModule->hasBeenSetup = true;
			}
		}
	}

	gConfig->SetupFinished();

	#if MDebugDelayEachModule || MDebugDelayStart
		DebugMsg(eDbgLevel_Medium, "Module: Setup Complete\n");
	#endif
}

void
CModule::TearDownAll(
	void)
{
	for(int priorityItr = 0; priorityItr < 256; ++priorityItr)
	{
		for(int i = 0; i < gModuleCount; ++i)
		{
			if(gModuleList[i]->priority == priorityItr)
			{
				#if MDebugModules
				DebugMsg(eDbgLevel_Medium, "Module: TearDown %s\n", StringizeUInt32(gModuleList[i]->uid));
				delay(3000);
				#endif
				gModuleList[i]->TearDown();
			}
		}
	}

	gTearingDown = true;

	#if MDebugModules
	DebugMsg(eDbgLevel_Medium, "Module: TearDown Complete\n");
	#endif
}

void
CModule::ResetAllState(
	void)
{
	TearDownAll();

	for(int i = 0; i < gModuleCount; ++i)
	{
		DebugMsg(eDbgLevel_Medium, "Module: ResetState %s\n", StringizeUInt32(gModuleList[i]->uid));
		#if MDebugModules
		delay(3000);
		#endif
		gModuleList[i]->ResetState();
	}

	SetupAll(gVersionStr, gFlashLED);

	gTearingDown = true;

	#if MDebugModules
	DebugMsg(eDbgLevel_Medium, "Module: ResetAllState Complete\n");
	#endif
}

void
CModule::LoopAll(
	void)
{
	if(gFlashLED && gConfig->GetVal(gBlinkLEDIndex) == 1)
	{
		static bool	on = false;
		static uint64_t	lastBlinkTime;

		if(gCurLocalMS - lastBlinkTime >= 500)
		{
			on = !on;
			digitalWriteFast(13, on);
			lastBlinkTime = gCurLocalMS;
		}
	}

	for(int i = 0; i < gModuleCount; ++i)
	{
		if(gTearingDown)
		{
			break;
		}

		uint32_t	curMillis = millis();
		uint32_t	curMicros = micros();

		gCurLocalMS += curMillis - gLastMillis;
		gCurLocalUS += curMicros - gLastMicros;
		gLastMillis = curMillis;
		gLastMicros = curMicros;

		uint64_t	updateDeltaUS = gCurLocalUS - gModuleList[i]->lastUpdateUS;
		if(updateDeltaUS >= gModuleList[i]->updateTimeUS)
		{
			//Serial.printf("Updating %s %d\n", StringizeUInt32(gModuleList[i]->uid), gModuleList[i]->enabled);
			if(gModuleList[i]->enabled)
			{
				gModuleList[i]->Update((uint32_t)updateDeltaUS);
				gModuleList[i]->lastUpdateUS = gCurLocalUS;
			}
		}
	}

	gTearingDown = false;
}

/*


CModule*
FindModule(
	uint32_t	inUID)
{
	for(int i = 0; i < gModuleCount; ++i)
	{
		if(gModuleList[i]->uid == inUID)
			return gModuleList[i];
	}

	return NULL;
}


*/