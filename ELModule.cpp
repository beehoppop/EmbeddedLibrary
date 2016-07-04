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
	eEEPROM_ModuleCountOffset = 1,
	eEEPROM_ListStart = 2,

	eEEPROM_Version = 104,

	eEEPROM_Size = 2048,

	eEEPROM_UIDLength = 32,

	eMaxEEPROMModules = 16,
};

#define MDebugDelayEachModule 0
#define MDebugModules 0
#define MDebugTargetNode 0xFF

struct SEEPROMEntry
{
	char		uid[eEEPROM_UIDLength];
	uint16_t	offset;
	uint16_t	size;
	uint16_t	version;
	bool		inUse;
};

static uint32_t		gModuleCount;
static CModule*		gModuleList[eMaxModuleCount];
static SEEPROMEntry	gEEPROMEntryList[eMaxEEPROMModules];
static bool			gTooManyModules = false;
static bool			gTearingDown = false;
static uint32_t		gLastMillis;
static uint32_t		gLastMicros;
static bool			gFlashLED;
static int			gBlinkLEDIndex;		// config index for blink LED config var
static char const*	gCurrentModuleConstructingName;
static uint32_t		gCurrentModuleClassSize;
static bool			gSetupStarted;

uint64_t	gCurLocalMS;
uint64_t	gCurLocalUS;

char const*	gVersionStr;

class CModuleManager : public CModule, public ICmdHandler
{
public:

	MModule_Declaration(CModuleManager)

private:

	CModuleManager(
		)
		:
		CModule()
	{
		CModule_Command::Include();
		CModule_Config::Include();
	}

	virtual void
	Setup(
		void)
	{
		MAssert(gCommandModule != NULL);
		MAssert(gConfigModule != NULL);
		MCommandRegister("alive", CModuleManager::SerialCmdAlive, "Return the build date and version as proof of life");
		gBlinkLEDIndex = gConfigModule->RegisterConfigVar("blink_led");
	}

	uint8_t
	SerialCmdAlive(
		IOutputDirector*	inOutput,
		int					inArgC,
		char const*			inArgV[])
	{
		inOutput->printf("ALIVE node=%d ver=%s build date=%s %s\n", gConfigModule->GetVal(gConfigModule->nodeIDIndex), gVersionStr, __DATE__, __TIME__);
		
		return eCmd_Succeeded;
	}

};

MModuleImplementation_Start(CModuleManager)
MModuleImplementation_Finish(CModuleManager)

CModule::CModule(
	uint16_t	inEEPROMSize,
	uint16_t	inEEPROMVersion,
	void*		inEEPROMData,
	uint32_t	inUpdateTimeUS,
	bool		inEnabled)
	:
	eepromSize(inEEPROMSize),
	eepromVersion(inEEPROMVersion),
	eepromData(inEEPROMData),
	updateTimeUS(inUpdateTimeUS),
	enabled(inEnabled),
	hasBeenSetup(false)
{
	MAssert(strlen(gCurrentModuleConstructingName) <= eEEPROM_UIDLength - 1);
	uid = gCurrentModuleConstructingName;
	classSize = gCurrentModuleClassSize;
}

void
CModule::DoneConstructing(
	void)
{
	if(gModuleCount >= MStaticArrayLength(gModuleList))
	{
		gTooManyModules = true;
		return;
	}
	gModuleList[gModuleCount++] = this;

	SetupIfNeeded();

	#if MDebugModules
	SystemMsg("%s DoneConstruction: free mem = %ld", uid, GetFreeMemory());
	#endif
}

void
CModule::SetupIfNeeded(
	void)
{
	if(gSetupStarted && enabled && !hasBeenSetup)
	{
		#if MDebugDelayEachModule
			SystemMsg("Module: Setup %s\n", uid);
			//Need to fix this code to not reference gConfigModule since it has not been initialized yet
			//if(MDebugTargetNode == 0xFF || MDebugTargetNode == gConfigModule->GetVal(eConfigVar_NodeID))
			{
				delay(1000);
			}
		#endif
		// Set this module up now
		Setup();
		lastUpdateUS = gCurLocalUS;
		hasBeenSetup = true;
	}
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
	SetupIfNeeded();
}

void
CModule::Update(
	uint32_t	inDeltaTimeUS)
{
	//Serial.printf("default %s %d\n", uid, enabled);
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
	char const*		inUID)
{
	for(uint32_t i = 0; i < MStaticArrayLength(gEEPROMEntryList); ++i)
	{
		if(strcmp(gEEPROMEntryList[i].uid, inUID) == 0)
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

	CModule_SysMsgSerialHandler::Include();
	CModuleManager::Include();

	SystemMsg(eMsgLevel_Basic, "Module: count=%d\n", gModuleCount);

	MAssert(gTooManyModules == false);

	bool	changes = false;
	uint8_t	eepromVersion = EEPROM.read(eEEPROM_VersionOffset);
	uint8_t	eepromModuleCount = EEPROM.read(eEEPROM_ModuleCountOffset);
	if(eepromVersion == eEEPROM_Version && eepromModuleCount == MStaticArrayLength(gEEPROMEntryList))
	{
		LoadDataFromEEPROM(gEEPROMEntryList, eEEPROM_ListStart, sizeof(gEEPROMEntryList));
		for(uint32_t i = 0; i < MStaticArrayLength(gEEPROMEntryList); ++i)
		{
			gEEPROMEntryList[i].inUse = false;
		}

		int	totalEEPROMSize = eEEPROM_ListStart + sizeof(gEEPROMEntryList);
		for(uint32_t i = 0; i < gModuleCount; ++i)
		{
			CModule*	curModule = gModuleList[i];
			if(curModule->eepromSize == 0)
				continue;
		
			MAssert(curModule->eepromData != NULL);	// If the module needs eeprom storage it must provide local memory for it

			SEEPROMEntry*	target = FindEEPROMEntry(curModule->uid);

			if(target == NULL)
			{
				SystemMsg(eMsgLevel_Basic, "Module %s: no eeprom entry\n", curModule->uid);
				changes = true;
			}
			else if(target->size != curModule->eepromSize || target->version != curModule->eepromVersion)
			{
				SystemMsg(eMsgLevel_Basic, "Module %s: eeprom changed version or size\n", curModule->uid);
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
		SystemMsg(eMsgLevel_Basic, "EEPROM version mismatch old=%d new=%d\n", eepromVersion, eEEPROM_Version);

		changes = true;
		for(uint32_t i = 0; i < MStaticArrayLength(gEEPROMEntryList); ++i)
		{
			gEEPROMEntryList[i].inUse = false;
		}
	}

	if(changes)
	{
		// since changes have been made compress all module eeprom data into low eeprom space
		uint16_t		curOffset = eEEPROM_ListStart + sizeof(gEEPROMEntryList);
		SEEPROMEntry*	curEEPROM = gEEPROMEntryList;
		for(uint32_t i = 0; i < gModuleCount; ++i)
		{
			CModule*	curModule = gModuleList[i];
			if(curModule->eepromSize == 0)
				continue;

			MAssert(curEEPROM < gEEPROMEntryList + MStaticArrayLength(gEEPROMEntryList));
			curModule->eepromOffset = curOffset;
			strcpy(curEEPROM->uid, curModule->uid);
			curEEPROM->offset = curOffset;
			curEEPROM->size = curModule->eepromSize;
			curEEPROM->version = curModule->eepromVersion;
			if(!curEEPROM->inUse)
			{
				SystemMsg(eMsgLevel_Basic, "Module %s: Initializing eeprom\n", curModule->uid);
				curModule->EEPROMInitialize();
			}
			WriteDataToEEPROM(curModule->eepromData, curOffset, curModule->eepromSize);
			++curEEPROM;
			curOffset += curModule->eepromSize;
		}

		WriteDataToEEPROM(gEEPROMEntryList, eEEPROM_ListStart, sizeof(gEEPROMEntryList));
		EEPROM.write(eEEPROM_VersionOffset, eEEPROM_Version);
		EEPROM.write(eEEPROM_ModuleCountOffset, MStaticArrayLength(gEEPROMEntryList));
	}

	gSetupStarted = true;

	gCurLocalMS = millis();
	gCurLocalUS = micros();

	for(uint32_t i = 0; i < gModuleCount; ++i)
	{
		CModule*	curModule = gModuleList[i];
		curModule->SetupIfNeeded();
	}

	gConfigModule->SetupFinished();

	#if MDebugDelayEachModule
		SystemMsg("Module: Setup Complete\n"); delay(100);
	#endif
}

void
CModule::TearDownAll(
	void)
{
	for(uint32_t i = 0; i < gModuleCount; ++i)
	{
		#if MDebugModules
		SystemMsg(eMsgLevel_Medium, "Module: TearDown %s\n", gModuleList[i]->uid);
		delay(3000);
		#endif
		gModuleList[i]->TearDown();
	}

	gTearingDown = true;

	#if MDebugModules
	SystemMsg(eMsgLevel_Medium, "Module: TearDown Complete\n");
	#endif
}

void
CModule::ResetAllState(
	void)
{
	MAssert(0); // This needs work to ensure modules don't get re-added to the lists

	TearDownAll();

	for(uint32_t i = 0; i < gModuleCount; ++i)
	{
		SystemMsg(eMsgLevel_Medium, "Module: ResetState %s\n", gModuleList[i]->uid);
		#if MDebugModules
		delay(3000);
		#endif
		gModuleList[i]->ResetState();
	}

	SetupAll(gVersionStr, gFlashLED);

	gTearingDown = true;

	#if MDebugModules
	SystemMsg(eMsgLevel_Medium, "Module: ResetAllState Complete\n");
	#endif
}

bool gFlag;
void
CModule::LoopAll(
	void)
{

	if(gFlashLED && gConfigModule->GetVal(gBlinkLEDIndex) == 1)
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

	for(uint32_t i = 0; i < gModuleCount; ++i)
	{
		if(gTearingDown)
		{
			break;
		}
		gModuleList[i]->UpdateIfNeeded();
	}

	gTearingDown = false;
}

void
CModule::UpdateIfNeeded(
	void)
{
	uint32_t	curMillis = millis();
	uint32_t	curMicros = micros();

	gCurLocalMS += curMillis - gLastMillis;
	gCurLocalUS += curMicros - gLastMicros;
	gLastMillis = curMillis;
	gLastMicros = curMicros;

	uint64_t	updateDeltaUS = gCurLocalUS - lastUpdateUS;
	if(updateDeltaUS >= updateTimeUS)
	{
		if(enabled)
		{
			//Serial.printf("Updating %s\n", uid); delay(100);
			#if 0
			uint32_t startMS = millis();
			#endif
			Update((uint32_t)updateDeltaUS);
			#if 0
			uint32_t doneMS = millis();
			uint32_t	result = doneMS - startMS;
			if(result > 0)
			{
				Serial.printf("%s = %d\n", uid, result);
			}
			#endif
			lastUpdateUS = gCurLocalUS;
		}
	}
}

void
StartingModuleConstruction(
	char const*	inClassName,
	uint32_t	inClassSize)
{
	#if MDebugModules
	SystemMsg("%s StartConstruction: class size = %ld, free mem = %ld", inClassName, inClassSize, GetFreeMemory());
	#endif
	MAssert(sizeof(inClassName) <= GetFreeMemory());
	gCurrentModuleConstructingName = inClassName;
	gCurrentModuleClassSize = inClassSize;
}
