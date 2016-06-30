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

#include <ELTouch.h>
#include <ELAssert.h>

enum 
{
	eState_WaitingForChangeToTouched,
	eState_WaitingForTouchSettle,
	eState_WaitingForChangeToRelease,
	eState_WaitingForReleaseSettle,

	eUpdateTimeUS = 50000,
	eSettleTimeMS = 200,

	eTouchThreshold = 2000,
};

CModule_Touch*	gTouchModule;

CModule_Touch::CModule_Touch(
	)
	:
	CModule(0, 0, NULL, 10000)
{
}

void
CModule_Touch::Setup(
	void)
{

}

void
CModule_Touch::Update(
	uint32_t	inDeltaTimeUS)
{
	STouch*	curTouch = touchList;
	for(int i = 0; i < eTouch_MaxEvents; ++i, ++curTouch)
	{
		if(curTouch->source == eTouchSource_Pin)
		{
			int	readValue = touchRead(curTouch->id);
			bool touched = readValue > eTouchThreshold;

			switch(curTouch->state)
			{
				case eState_WaitingForChangeToTouched:
					if(touched)
					{
						curTouch->time = gCurLocalMS;
						curTouch->state = eState_WaitingForTouchSettle;
					}
					break;

				case eState_WaitingForTouchSettle:
					if(!touched)
					{
						curTouch->state = eState_WaitingForChangeToTouched;
						break;
					}

					if(gCurLocalMS - curTouch->time >= eSettleTimeMS)
					{
						curTouch->state = eState_WaitingForChangeToRelease;
						SystemMsg(eMsgLevel_Verbose, "TTch: Touched %d\n", i);
						((curTouch->object)->*(curTouch->method))(curTouch->id, eTouchEvent_Touch, curTouch->reference);
					}
					break;

				case eState_WaitingForChangeToRelease:
					if(!touched)
					{
						//SystemMsg(eMsgLevel_Verbose, "TeensyTouch: Going to release settle %d\n", itr);
						curTouch->time = gCurLocalMS;
						curTouch->state = eState_WaitingForReleaseSettle;
					}
					break;

				case eState_WaitingForReleaseSettle:
					if(touched)
					{
						//SystemMsg(eMsgLevel_Verbose, "MPR121: Going to release waiting %d\n", itr);
						curTouch->state = eState_WaitingForChangeToRelease;
						break;
					}

					if(gCurLocalMS - curTouch->time >= eSettleTimeMS)
					{
						curTouch->state = eState_WaitingForChangeToTouched;
						SystemMsg(eMsgLevel_Verbose, "TTch: Release %d\n", i);
						((curTouch->object)->*(curTouch->method))(curTouch->id, eTouchEvent_Release, curTouch->reference);
					}
					break;
			}
		}
	}
}

void
CModule_Touch::RegisterTouchHandler(
	uint8_t				inID,
	ETouchSource		inSource,
	ITouchEventHandler*	inObject,
	TTouchEventMethod	inMethod,
	void*				inReference)
{
	STouch*	targetTouch = FindTouch(inID, inSource);
	if(targetTouch == NULL)
	{
		targetTouch = FindUnused();
		MReturnOnError(targetTouch == NULL);
	}

	targetTouch->id = inID;
	targetTouch->source = inSource;
	targetTouch->object = inObject;
	targetTouch->method = inMethod;
	targetTouch->reference = inReference;
	targetTouch->state = 0;
	targetTouch->time = 0;
}

CModule_Touch::STouch*
CModule_Touch::FindTouch(
	uint8_t	inID,
	uint8_t	inSource)
{
	for(int i = 0; i < eTouch_MaxEvents; ++i)
	{
		if(touchList[i].id == inID && touchList[i].source == inSource)
		{
			return touchList + i;
		}
	}

	return NULL;
}

CModule_Touch::STouch*
CModule_Touch::FindUnused(
	void)
{
	for(int i = 0; i < eTouch_MaxEvents; ++i)
	{
		if(touchList[i].source == eTouchSource_None)
		{
			return touchList + i;
		}
	}

	return NULL;
}

MModuleImplementation_Start(CModule_Touch)
MModuleImplementation_FinishGlobal(CModule_Touch, gTouchModule)

#if 0
// For future use

#include <Wire.h>

#include <ELAssert.h>
#include <ELModule.h>
#include <ELUtilities.h>

#include "TCMPR121.h"

// MPR121 Register Defines
#define MHD_R	0x2B
#define NHD_R	0x2C
#define	NCL_R 	0x2D
#define	FDL_R	0x2E
#define	MHD_F	0x2F
#define	NHD_F	0x30
#define	NCL_F	0x31
#define	FDL_F	0x32
#define	ELE0_T	0x41
#define	ELE0_R	0x42
#define	ELE1_T	0x43
#define	ELE1_R	0x44
#define	ELE2_T	0x45
#define	ELE2_R	0x46
#define	ELE3_T	0x47
#define	ELE3_R	0x48
#define	ELE4_T	0x49
#define	ELE4_R	0x4A
#define	ELE5_T	0x4B
#define	ELE5_R	0x4C
#define	ELE6_T	0x4D
#define	ELE6_R	0x4E
#define	ELE7_T	0x4F
#define	ELE7_R	0x50
#define	ELE8_T	0x51
#define	ELE8_R	0x52
#define	ELE9_T	0x53
#define	ELE9_R	0x54
#define	ELE10_T	0x55
#define	ELE10_R	0x56
#define	ELE11_T	0x57
#define	ELE11_R	0x58
#define	FIL_CFG	0x5D
#define	ELE_CFG	0x5E
#define GPIO_CTRL0	0x73
#define	GPIO_CTRL1	0x74
#define GPIO_DATA	0x75
#define	GPIO_DIR	0x76
#define	GPIO_EN		0x77
#define	GPIO_SET	0x78
#define	GPIO_CLEAR	0x79
#define	GPIO_TOGGLE	0x7A
#define	ATO_CFG0	0x7B
#define	ATO_CFGU	0x7D
#define	ATO_CFGL	0x7E
#define	ATO_CFGT	0x7F

#define TOU_THRESH	0x26
#define	REL_THRESH	0x2A

enum
{
	eMaxInterfaces = 4,

	eIRQPin = 20,

	eInputCount = 12,
};

struct SSensorInterface
{
	int				port;
	ITouchSensor*	touchSensor;
};

class CModule_MPR121 : public CModule
{
public:
	
	CModule_MPR121(
		);

	void
	Setup(
		void);
	
	void
	Update(
		uint32_t	inDeltaTimeUS);

	void
	Initialize(
		void);

	bool	initialized;
};

static int				gSensorInterfaceCount;
static SSensorInterface	gSensorInterfaceList[eMaxInterfaces];
static CModule_MPR121	gModuleMPR121;
static int				gMPRRegister = 0x5A;
static uint8_t			gTouchState[eInputCount];
static uint32_t			gLastTouchTime[eInputCount];
static bool				gMPR121Present;

enum 
{
	eState_WaitingForChangeToTouched,
	eState_WaitingForTouchSettle,
	eState_WaitingForChangeToRelease,
	eState_WaitingForReleaseSettle,

	eUpdateTimeUS = 50000,
	eSettleTimeMS = 200
};

uint8_t
MPR121SetRegister(
	uint8_t	inRegister, 
	uint8_t	inValue)
{
	Wire.beginTransmission(gMPRRegister);
	Wire.write(inRegister);
	Wire.write(inValue);
	return Wire.endTransmission();
}

CModule_MPR121::CModule_MPR121(
	)
	:
	CModule("mpr1", 0, eUpdateTimeUS)
{

}

void
CModule_MPR121::Setup(
	void)
{

}

void
CModule_MPR121::Initialize(
	void)
{
	if(initialized)
	{
		return;
	}

	initialized = true;

	pinMode(eIRQPin, INPUT);
	digitalWrite(eIRQPin, HIGH); //enable pullup resistor

	//SystemMsg(eMsgLevel_Verbose, "MPR121: Staring wire");
	Wire.begin();
	//SystemMsg(eMsgLevel_Verbose, "MPR121: done wire");

	// Enter config mode
	if(MPR121SetRegister(ELE_CFG, 0x00) != 0)
	{
		gMPR121Present = false;
	} 
 
 	gMPR121Present = true;
 
	// Section A - Controls filtering when data is > baseline.
	MPR121SetRegister(MHD_R, 0x01);
	MPR121SetRegister(NHD_R, 0x01);
	MPR121SetRegister(NCL_R, 0x00);
	MPR121SetRegister(FDL_R, 0x00);

	// Section B - Controls filtering when data is < baseline.
	MPR121SetRegister(MHD_F, 0x01);
	MPR121SetRegister(NHD_F, 0x01);
	MPR121SetRegister(NCL_F, 0xFF);
	MPR121SetRegister(FDL_F, 0x02);
  
	// Section C - Sets touch and release thresholds for each electrode
	for(int itr = 0; itr < eInputCount; ++itr)
	{
		MPR121SetRegister(ELE0_T + itr * 2, TOU_THRESH);
		MPR121SetRegister(ELE0_R + itr * 2, REL_THRESH);
	}
  
	// Section D
	// Set the Filter Configuration
	// Set ESI2
	MPR121SetRegister(FIL_CFG, 0x04);
  
	// Section E
	// Electrode Configuration
	// Set ELE_CFG to 0x00 to return to standby mode
	MPR121SetRegister( ELE_CFG, 0x0C);  // Enables all 12 Electrodes
  
  
	// Section F
	// Enable Auto Config and auto Reconfig
	/*set_register(0x5A, ATO_CFG0, 0x0B);
	set_register(0x5A, ATO_CFGU, 0xC9);  // USL = (Vdd-0.7)/vdd*256 = 0xC9 @3.3V   set_register(0x5A, ATO_CFGL, 0x82);  // LSL = 0.65*USL = 0x82 @3.3V
	set_register(0x5A, ATO_CFGT, 0xB5);*/  // Target = 0.9*USL = 0xB5 @3.3V
	
	// Exist config mode
	MPR121SetRegister(ELE_CFG, 0x0C);

}

void
CModule_MPR121::Update(
	uint32_t	inDeltaTimeUS)
{
	if(!gMPR121Present)
	{
		return;
	}

	//read the touch state from the MPR121
	Wire.requestFrom(gMPRRegister, 2); 
    
	uint16_t LSB = Wire.read();
	uint16_t MSB = Wire.read();
	uint16_t touchedBV = ((MSB << 8) | LSB);
		
	for(int itr = 0; itr < eInputCount; ++itr)
	{
		bool	touched = (touchedBV & (1 << itr)) ? true : false;

		switch(gTouchState[itr])
		{
			case eState_WaitingForChangeToTouched:
				if(touched)
				{
					gLastTouchTime[itr] = gCurLocalMS;
					gTouchState[itr] = eState_WaitingForTouchSettle;
				}
				break;

			case eState_WaitingForTouchSettle:
				if(!touched)
				{
					gTouchState[itr] = eState_WaitingForChangeToTouched;
					break;
				}

				if(gCurLocalMS - gLastTouchTime[itr] >= eSettleTimeMS)
				{
					gTouchState[itr] = eState_WaitingForChangeToRelease;
					SystemMsg(eMsgLevel_Verbose, "MPR121: Touched %d\n", itr);
					for(int itr2 = 0; itr2 < gSensorInterfaceCount; ++itr2)
					{
						gSensorInterfaceList[itr2].touchSensor->Touch(itr);
					}
				}
				break;

			case eState_WaitingForChangeToRelease:
				if(!touched)
				{
					//SystemMsg(eMsgLevel_Verbose, "MPR121: Going to release settle %d\n", itr);
					gLastTouchTime[itr] = gCurLocalMS;
					gTouchState[itr] = eState_WaitingForReleaseSettle;
				}
				break;

			case eState_WaitingForReleaseSettle:
				if(touched)
				{
					//SystemMsg(eMsgLevel_Verbose, "MPR121: Going to release waiting %d\n", itr);
					gTouchState[itr] = eState_WaitingForChangeToRelease;
					break;
				}

				if(gCurLocalMS - gLastTouchTime[itr] >= eSettleTimeMS)
				{
					gTouchState[itr] = eState_WaitingForChangeToTouched;
					SystemMsg(eMsgLevel_Verbose, "MPR121: Release %d\n", itr);
					for(int itr2 = 0; itr2 < gSensorInterfaceCount; ++itr2)
					{
						gSensorInterfaceList[itr2].touchSensor->Release(itr);
					}
				}
				break;
		}
	}
}

void
MPRRegisterTouchSensor(
	int				inPort,
	ITouchSensor*	inTouchSensor)
{
	MAssert(gSensorInterfaceCount < eMaxInterfaces);

	gModuleMPR121.Initialize();

	for(int i = 0; i < gSensorInterfaceCount; ++i)
	{
		if(gSensorInterfaceList[i].touchSensor == inTouchSensor)
		{
			// Don't re register
			gSensorInterfaceList[i].port = inPort;
			return;
		}
	}

	SSensorInterface*	newInterface = gSensorInterfaceList + gSensorInterfaceCount++;
	newInterface->port = inPort;
	newInterface->touchSensor = inTouchSensor;
}

void
MPRUnRegisterTouchSensor(
	int				inPort,
	ITouchSensor*	inTouchSensor)
{
	for(int i = 0; i < gSensorInterfaceCount; ++i)
	{
		if(gSensorInterfaceList[i].touchSensor == inTouchSensor)
		{
			memmove(gSensorInterfaceList + i, gSensorInterfaceList + i + 1, (gSensorInterfaceCount - i - 1) * sizeof(SSensorInterface));
			--gSensorInterfaceCount;
			return;
		}
	}
}

void
MPRSetTouchSensitivity(
	int		inPort,
	int		inID,
	uint8_t	inSensitivity)
{
	// Enter config mode
	MPR121SetRegister(ELE_CFG, 0x00); 

	MPR121SetRegister(ELE0_T + inID * 2, inSensitivity);

	// Exist config mode
	MPR121SetRegister(ELE_CFG, 0x0C);
}

void
MPRSetReleaseSensitivity(
	int		inPort,
	int		inID,
	uint8_t	inSensitivity)
{
	// Enter config mode
	MPR121SetRegister(ELE_CFG, 0x00); 

	MPR121SetRegister(ELE0_R + inID * 2, inSensitivity);

	// Exist config mode
	MPR121SetRegister(ELE_CFG, 0x0C);
}



#endif
