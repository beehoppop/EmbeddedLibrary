// 
// 
// 

#include "ELAssert.h"
#include "ELModule.h"
#include "ELUtilities.h"
#include "ELConfig.h"

enum
{
	eEEPROM_VersionOffset = 0,
	eEEPROM_ListStart = 1,

	eEEPROM_Version = 100,

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
};

static int			gModuleCount;
static CModule*		gModuleList[eMaxModuleCount];
static SEEPROMEntry	gEEPROMEntryList[eMaxModuleCount];
static bool			gTooManyModules = false;
static bool			gTearingDown = false;
static uint32_t		gLastMillis;
static uint32_t		gLastMicros;
static bool			gFlashLED;

uint64_t	gCurLocalMS;
uint64_t	gCurLocalUS;

CModule::CModule(
	char const*	inUID,
	uint16_t	inEEPROMSize,
	uint16_t	inEEPROMVersion,
	uint32_t	inUpdateTimeUS,
	uint8_t		inPriority,
	bool		inEnabled)
	:
	uid((inUID[0] << 24) | (inUID[1] << 16) | (inUID[2] << 8) | inUID[3]),
	eepromSize(inEEPROMSize),
	eepromVersion(inEEPROMVersion),
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
}

void
CModule::Update(
	uint32_t	inDeltaTimeUS)
{

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
	for(int i = eepromOffset; i < eepromOffset + eepromSize; ++i)
	{
		EEPROM.write(i, 0xFF);
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

int
EEPROMCompare(
	void const*	inA,
	void const*	inB)
{
	SEEPROMEntry const*	a = (SEEPROMEntry const*)inA;
	SEEPROMEntry const*	b = (SEEPROMEntry const*)inB;

	if(a->offset < b->offset)
	{
		return -1;
	}

	if(a->offset > b->offset)
	{
		return 1;
	}

	return 0;
}

uint16_t
FindAvailableEEPROMOffset(
	uint16_t		inSize)
{
	uint16_t	offset = eEEPROM_ListStart + eMaxModuleCount * sizeof(SEEPROMEntry);

	for(int i = 0; i < eMaxModuleCount; ++i)
	{
		if(gEEPROMEntryList[i].uid == 0xFFFFFFFF)
		{
			continue;
		}

		if(offset + inSize <= gEEPROMEntryList[i].offset)
		{
			MAssert(offset + inSize <= eEEPROM_Size);
			return offset;
		}

		offset = gEEPROMEntryList[i].offset + gEEPROMEntryList[i].size;
	}

	MAssert(offset + inSize <= eEEPROM_Size);

	return offset;
}

void
CModule::SetupAll(
	bool	inFlashLED)
{
	gFlashLED = inFlashLED;
	if(inFlashLED)
	{
		pinMode(13, OUTPUT);
		digitalWrite(13, 1);
	}

	Serial.begin(115200);

	if(gTooManyModules)
	{
		while(true)
		{
			Serial.printf("Too Many Modules\n");	// This msg is too early for anything but serial
		}
	}

	uint8_t	eepromVersion = EEPROM.read(eEEPROM_VersionOffset);

	if(eepromVersion != eEEPROM_Version)
	{
		for(int i = eEEPROM_ListStart; i < eEEPROM_ListStart + (int)sizeof(gEEPROMEntryList); ++i)
		{
			EEPROM.write(i, 0xFF);
		}
		EEPROM.write(eEEPROM_VersionOffset, eEEPROM_Version);
	}

	bool	changes = false;

	LoadDataFromEEPROM(gEEPROMEntryList, eEEPROM_ListStart, sizeof(gEEPROMEntryList));

	for(int i = 0; i < gModuleCount; ++i)
	{
		if(gModuleList[i]->eepromSize == 0)
			continue;

		SEEPROMEntry*	target = FindEEPROMEntry(gModuleList[i]->uid);

		if(target != NULL && (target->size != gModuleList[i]->eepromSize || target->version != gModuleList[i]->eepromVersion))
		{
			//DebugMsg(eDbgLevel_Medium, "Module: %s changed size\n", StringizeUInt32(gModuleList[i]->uid));
			target->uid = 0xFFFFFFFF;
			target->offset = 0xFFFF;
			target->size = 0xFFFF;
			target->version = 0xFFFF;
			target = NULL;
		}

		if(target == NULL)
		{
			target = FindEEPROMEntry(0xFFFFFFFF);
			MAssert(target != NULL);

			target->offset = FindAvailableEEPROMOffset(gModuleList[i]->eepromSize);
			target->uid = gModuleList[i]->uid;
			target->size = gModuleList[i]->eepromSize;
			target->version = gModuleList[i]->eepromVersion;

			gModuleList[i]->eepromOffset = target->offset;
			gModuleList[i]->EEPROMInitialize();

			//DebugMsg(eDbgLevel_Medium, "Module: %s added\n", StringizeUInt32(target->uid));

			changes = true;
		}

		gModuleList[i]->eepromOffset = target->offset;
		//DebugMsg(eDbgLevel_Medium, "Module: %s offset is %d\n", StringizeUInt32(target->uid), target->offset);
	}

	for(int i = 0; i < eMaxModuleCount; ++i)
	{
		if(gEEPROMEntryList[i].uid == 0xFFFFFFFF)
		{
			continue;
		}

		CModule*	target = FindModule(gEEPROMEntryList[i].uid);

		if(target == NULL)
		{
			DebugMsg(eDbgLevel_Medium, "Module: %s removed\n", StringizeUInt32(gEEPROMEntryList[i].uid));
			gEEPROMEntryList[i].uid = 0xFFFFFFFF;
			gEEPROMEntryList[i].offset = 0xFFFF;
			gEEPROMEntryList[i].size = 0xFFFF;
			changes = true;
		}
	}

	if(changes)
	{
		qsort(gEEPROMEntryList, eMaxModuleCount, sizeof(SEEPROMEntry), EEPROMCompare);
		WriteDataToEEPROM(gEEPROMEntryList, eEEPROM_ListStart, sizeof(gEEPROMEntryList));
	}

	#if MDebugDelayStart
		if(MDebugTargetNode == 0xFF || MDebugTargetNode == gConfig.GetVal(eConfigVar_NodeID))
		{
			delay(6000);
		}
	#endif

	gCurLocalMS = millis();
	gCurLocalUS = micros();

	uint32_t	timeOffsetUS = 0;

	for(int priorityItr = 256; priorityItr-- > 0;)
	{
		for(int i = 0; i < gModuleCount; ++i)
		{
			if(gModuleList[i]->priority == priorityItr)
			{
				#if MDebugDelayEachModule || MDebugDelayStart
					DebugMsg(eDbgLevel_Medium, "Module: Setup %s\n", StringizeUInt32(gModuleList[i]->uid));
				#endif
				#if MDebugDelayEachModule
					if(MDebugTargetNode == 0xFF || MDebugTargetNode == gConfig.GetVal(eConfigVar_NodeID))
					{
						delay(3000);
					}
				#endif
				gModuleList[i]->Setup();
				gModuleList[i]->lastUpdateUS = gCurLocalUS;
			}
		}
	}

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

	SetupAll(gFlashLED);

	gTearingDown = true;

	#if MDebugModules
	DebugMsg(eDbgLevel_Medium, "Module: ResetAllState Complete\n");
	#endif
}

void
CModule::LoopAll(
	void)
{
	if(gFlashLED && gConfig.GetVal(eConfigVar_BlinkLED) == 1)
	{
		static bool	on = false;

		digitalWriteFast(13, on);
		on = !on;
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

		uint32_t	updateDeltaUS = gCurLocalUS - gModuleList[i]->lastUpdateUS;
		if(updateDeltaUS >= gModuleList[i]->updateTimeUS)
		{
			//Serial.printf("Updating %s\n", StringizeUInt32(gModuleList[i]->uid));
			if(gModuleList[i]->enabled)
			{
				gModuleList[i]->Update(updateDeltaUS);
			}
			gModuleList[i]->lastUpdateUS = gCurLocalUS;
		}
	}

	gTearingDown = false;
}
