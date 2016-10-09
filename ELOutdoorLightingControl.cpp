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
	ABOUT

*/
#include <EL.h>
#include <ELAssert.h>
#include <ELModule.h>
#include <ELRealTime.h>
#include <ELDigitalIO.h>
#include <ELSunRiseAndSet.h>
#include <ELLuminositySensor.h>
#include <ELUtilities.h>
#include <ELInternet.h>
#include <ELOutdoorLightingControl.h>

enum
{
	eTransformerWarmUpMicros = 2000000,		// The number of microseconds to allow the transformer to warm up before use
	eLEDsCoolDownMicros = 500000,			// The number of microseconds to allow the LEDs to turn off before turning off the transformer
	eToggleCountResetMS = 1000,		// The time in ms to reset the pushbutton toggle count

	ePushCount_ToggleState = 1,		// The number of pushes to toggle state
	ePushCount_MotionTrip = 2,		// The number of pushes to simulate a motion trip
};

char const*	gTimeOfDayStr[] = {"Day", "Night", "Latenight"};

CModule_OutdoorLightingControl*	gOutdoorLighting;

MModuleImplementation_Start(
	CModule_OutdoorLightingControl, 
	IOutdoorLightingInterface*	inInterface,
	uint8_t						inMotionSensorPin,
	uint8_t						inTransformerPin,
	uint8_t						inTogglePin,
	ILuminosity*				inLuminosityInterface)
MModuleImplementation_Finish(CModule_OutdoorLightingControl, inInterface, inMotionSensorPin, inTransformerPin, inTogglePin, inLuminosityInterface)

CModule_OutdoorLightingControl::CModule_OutdoorLightingControl(
	IOutdoorLightingInterface*	inInterface,
	uint8_t						inMotionSensorPin,
	uint8_t						inTransformerPin,
	uint8_t						inTogglePin,
	ILuminosity*				inLuminosityInterface)
	:
	CModule(sizeof(SSettings), 0, &settings, 250000, 1),
	motionSensorPin(inMotionSensorPin),
	transformerPin(inTransformerPin),
	togglePin(inTogglePin),
	luminosityInterface(inLuminosityInterface),
	olInterface(inInterface)
{
	gOutdoorLighting = this;
	timeOfDay = eTimeOfDay_Day;
	motionSensorTrip = false;
	ledsOn = false;
	curLEDOnState = false;
	curMotionSensorTrip = false;
	overrideActive = false;
	overrideState = false;

	luxLowWater = false;
	curLuxTrigger = false;
	luxBufferIndex = 0;

	toggleCount = 0;
	toggleLastTimeMS = 0;

	// Begin including dependent modules
	CModule_Command::Include();
	CModule_SunRiseAndSet::Include();
	CModule_RealTime::Include();
	CModule_DigitalIO::Include();
	CModule_Internet::Include();
}

void
CModule_OutdoorLightingControl::SetOverride(
	bool	inOverrideActive,
	bool	inOverrideState)
{
	overrideActive = inOverrideActive;
	overrideState = inOverrideState;
}

void
CModule_OutdoorLightingControl::Setup(
	void)
{
	// Setup the transformer relay output pin
	pinMode(transformerPin, OUTPUT);
	digitalWriteFast(transformerPin, false);

	// Register the alarms, events, and commands
	MRealTimeRegisterTimeChange("OutdoorLighting", CModule_OutdoorLightingControl::TimeChangeMethod);
	if(togglePin != 0xFF)
	{
		MDigitalIORegisterEventHandler(togglePin, false, CModule_OutdoorLightingControl::ButtonPush, NULL);
	}
	if(motionSensorPin != 0xFF)
	{
		MDigitalIORegisterEventHandler(motionSensorPin, false, CModule_OutdoorLightingControl::MotionSensorTrigger, NULL, 500);
	}

	MCommandRegister("ledstate_set", CModule_OutdoorLightingControl::SetLEDState, "[on | off] : Turn LEDs on or off until the next event");
	MCommandRegister("latenight_set", CModule_OutdoorLightingControl::SetLateNightStartTime, "[hour] [min] : Set the hour and minute of late night");
	MCommandRegister("latenight_get", CModule_OutdoorLightingControl::GetLateNightStartTime, "[hour] [min] : Get the hour and minute of late night");
	MCommandRegister("luxtrigger_set", CModule_OutdoorLightingControl::SetTriggerLux, "");
	MCommandRegister("luxtrigger_get", CModule_OutdoorLightingControl::GetTriggerLux, "");
	MCommandRegister("luxmonitor_set", CModule_OutdoorLightingControl::SetLuxMonitor, "");
	MCommandRegister("luxmonitor_get", CModule_OutdoorLightingControl::GetLuxMonitor, "");
	MCommandRegister("motiontimeout_set", CModule_OutdoorLightingControl::SetMotionTripTimeout, "");
	MCommandRegister("motiontimeout_get", CModule_OutdoorLightingControl::GetMotionTripTimeout, "");
	MCommandRegister("latenighttimeout_set", CModule_OutdoorLightingControl::SetLateNightTimeout, "");
	MCommandRegister("latenighttimeout_get", CModule_OutdoorLightingControl::GetLateNightTimeout, "");
	MCommandRegister("override_set", CModule_OutdoorLightingControl::SetOverride, "[active 0|1] [state 0|1] : Set override and overrided state on or off");
	MCommandRegister("override_get", CModule_OutdoorLightingControl::GetOverride, "");

	MRealTimeRegisterEvent("LuxPeriodic", 1 * 60 * 1000000, false, CModule_OutdoorLightingControl::LuxPeriodic, NULL);

	UpdateTimes();

	MInternetRegisterPage("/", CModule_OutdoorLightingControl::CommandHomePageHandler);
}

void
CModule_OutdoorLightingControl::UpdateTimes(
	void)
{
	MSunsetRegisterEvent("Sunset1", eAlarm_Any, eAlarm_Any, eAlarm_Any, eAlarm_Any, CModule_OutdoorLightingControl::Sunset);
	MSunriseRegisterEvent("Sunrise1", eAlarm_Any, eAlarm_Any, eAlarm_Any, eAlarm_Any, CModule_OutdoorLightingControl::Sunrise);
	MRealTimeRegisterAlarm("LateNightAlarm", eAlarm_Any, eAlarm_Any, eAlarm_Any, eAlarm_Any, settings.lateNightStartHour, settings.lateNightStartMin, 0, CModule_OutdoorLightingControl::LateNightAlarm, NULL);

	// Setup the state given the current time of day and sunset/sunrise time
	TEpochTime	curTime = gRealTime->GetEpochTime(false);
	int	year, month, day, dow, hour, min, sec;
	gRealTime->GetComponentsFromEpochTime(curTime, year, month, day, dow, hour, min, sec);

	TEpochTime sunriseTime, sunsetTime;
	gSunRiseAndSet->GetSunRiseAndSetEpochTime(sunriseTime, sunsetTime, year, month, day, false);
		
	int	eventYear, eventMonth, eventDay, eventDOW, eventHour, eventMin, eventSec;
	gRealTime->GetComponentsFromEpochTime(sunriseTime, eventYear, eventMonth, eventDay, eventDOW, eventHour, eventMin, eventSec);
	SystemMsg("Setup sunrise time is %02d/%02d/%04d %02d:%02d:%02d\n", eventMonth, eventDay, eventYear, eventHour, eventMin, eventSec);
	
	gRealTime->GetComponentsFromEpochTime(sunsetTime, eventYear, eventMonth, eventDay, eventDOW, eventHour, eventMin, eventSec);
	SystemMsg("Setup sunset time is %02d/%02d/%04d %02d:%02d:%02d\n", eventMonth, eventDay, eventYear, eventHour, eventMin, eventSec);

	TEpochTime lateNightTime = gRealTime->GetEpochTimeFromComponents(year, month, day, settings.lateNightStartHour, settings.lateNightStartMin, 0);

	if(curTime > sunsetTime || curTime < sunriseTime)
	{
		if(lateNightTime < curTime || curTime < sunriseTime)
		{
			timeOfDay = eTimeOfDay_LateNight;
		}
		else
		{
			timeOfDay = eTimeOfDay_Night;
		}
	}
	else
	{
		timeOfDay = eTimeOfDay_Day;
	}
	olInterface->TimeOfDayChange(timeOfDay);

	SystemMsg("Setup time of day = %s\n", gTimeOfDayStr[timeOfDay]);

	ledsOn = timeOfDay == eTimeOfDay_Night;
}

void
CModule_OutdoorLightingControl::TimeChangeMethod(
	char const*	inName,
	bool		inTimeZone)
{
	UpdateTimes();
}

float
CModule_OutdoorLightingControl::GetAvgBrightness(
	void)
{
	if(luminosityInterface == NULL)
	{
		return 0;
	}

	return luminosityInterface->GetNormalizedBrightness(GetAvgLux());
}

float
CModule_OutdoorLightingControl::GetAvgLux(
	void)
{
	float totalLux = 0.0f;
	for(int i = 0; i < eLuxBufferSize; ++i)
	{
		totalLux += luxBuffer[i];
	}
	return totalLux / (float)eLuxBufferSize;
}

void
CModule_OutdoorLightingControl::Update(
	uint32_t inDeltaTimeUS)
{
	// Check on the toggle button
	if(gCurLocalMS - toggleLastTimeMS >= eToggleCountResetMS && toggleCount > 0)
	{
		// It has timed out so now check the toggle count for the action

		motionSensorTrip = false;	// Ensure motion trip is off

		SystemMsg("Toggle count is %d\n", toggleCount);

		if(toggleCount == ePushCount_ToggleState)
		{
			ledsOn = !ledsOn;
			SystemMsg("Toggle led state to %d\n", ledsOn);

			if(timeOfDay == eTimeOfDay_LateNight)
			{
				if(ledsOn)
				{
					MRealTimeRegisterEvent("LateNightTimerExpire", settings.lateNightTimeoutMins * 60 * 1000000, true, CModule_OutdoorLightingControl::LateNightTimerExpire, NULL);
				}
				else
				{
					gRealTime->CancelEvent("LateNightTimerExpire");
				}
			}
		}
		else if(toggleCount == ePushCount_MotionTrip)
		{
			SystemMsg("Simulating motion sensor trip\n");
			MotionSensorTrigger(motionSensorPin, eDigitalIO_PinActivated, NULL);
			MotionSensorTrigger(motionSensorPin, eDigitalIO_PinDeactivated, NULL);
		}

		olInterface->PushButtonStateChange(toggleCount);

		toggleCount = 0;
	}

	if(luminosityInterface != NULL)
	{
		luxBuffer[luxBufferIndex++ % eLuxBufferSize] = luminosityInterface->GetActualLux();

		if(luxBufferIndex == 1)
		{
			// If this is the first entry in the buffer initialize the rest of the buffer to this value
			for(int i = 1; i < eLuxBufferSize; ++i)
			{
				luxBuffer[i] = luxBuffer[0];
			}
		}

		// Update lux trigger
		if(timeOfDay == eTimeOfDay_Day)
		{
			if(!luxLowWater)
			{
				if(GetAvgLux() <= settings.triggerLuxLow)
				{
					ledsOn = true;
					luxLowWater = true;
				}
			}
			else if(GetAvgLux() >= settings.triggerLuxHigh)
			{
				ledsOn = false;
				luxLowWater = false;
			}
		}
	}

	if(motionSensorTrip != curMotionSensorTrip)
	{
		SystemMsg("Motion trip state to %d\n", motionSensorTrip);
		olInterface->MotionSensorStateChange(motionSensorTrip);
		curMotionSensorTrip = motionSensorTrip;
	}

	if(luxLowWater != curLuxTrigger)
	{
		SystemMsg("Lux trigger state to %d\n", luxLowWater);
		olInterface->LuxSensorStateChange(luxLowWater);
		curLuxTrigger = luxLowWater;
	}

	bool newLEDOnState = overrideActive ? overrideState : ledsOn;
	if(newLEDOnState != curLEDOnState)
	{
		curLEDOnState = newLEDOnState;

		SystemMsg("Setting led state to %d", curLEDOnState);

		if(curLEDOnState)
		{
			gRealTime->CancelEvent("XfmrToOff");
			MRealTimeRegisterEvent("XfmrToOn", eTransformerWarmUpMicros, true, CModule_OutdoorLightingControl::TransformerTransitionOnCompleted, NULL);
			digitalWriteFast(transformerPin, true);
		}
		else
		{
			gRealTime->CancelEvent("XfmrToOn");
			MRealTimeRegisterEvent("XfmrToOff", eLEDsCoolDownMicros, true, CModule_OutdoorLightingControl::TransformerTransitionOffCompleted, NULL);
			olInterface->LEDStateChange(false);
		}		
	}
}

void
CModule_OutdoorLightingControl::CommandHomePageHandler(
	IOutputDirector*	inOutput,
	int					inParamCount,
	char const**		inParamList)
{
	// Send html via in Output to add to the command server home page served to clients

	inOutput->printf("<form action=\"cmd_data\"><input type=\"submit\" value=\"LEDs On\"><input type=\"hidden\" name=\"Command\" value=\"ledstate_set on\"></form>");
	inOutput->printf("<form action=\"cmd_data\"><input type=\"submit\" value=\"LEDs Off\"><input type=\"hidden\" name=\"Command\" value=\"ledstate_set off\"></form>");

	inOutput->printf("<table border=\"1\">");
	inOutput->printf("<tr><th>Parameter</th><th>Value</th></tr>");

	// Add cur local time
	int	year, month, dom, dow, hour, min, sec;
	gRealTime->GetDateAndTime(year, month, dom, dow, hour, min, sec, false);
	inOutput->printf("<tr><td>Time</td><td>%04d-%02d-%02d %02d:%02d:%02d</td></tr>", year, month, dom, hour, min, sec);

	// Add sunset time
	TEpochTime sunsetTime = gSunRiseAndSet->GetSunsetEpochTime(year, month, dom, false);
	gRealTime->GetComponentsFromEpochTime(sunsetTime, year, month, dom, dow, hour, min, sec);
	inOutput->printf("<tr><td>Sunset Time</td><td>%04d-%02d-%02d %02d:%02d:%02d</td></tr>", year, month, dom, hour, min, sec);

	// add timeOfDay
	inOutput->printf("<tr><td>Time Of Day</td><td>%s</td></tr>", gTimeOfDayStr[timeOfDay]);

	if(luminosityInterface != NULL)
	{
		// Add cur lux
		inOutput->printf("<tr><td>Current Lux</td><td>%f</td></tr>", GetAvgLux());
		inOutput->printf("<tr><td>Current Brightness</td><td>%f</td></tr>", GetAvgBrightness());

		// add monitor lux
		inOutput->printf("<tr><td>Monitoring Lux</td><td>%s</td></tr>", settings.monitorLux ? "On" : "Off");

		// add lux trigger state
		inOutput->printf("<tr><td>Lux Low Water State</td><td>%s</td></tr>", luxLowWater ? "On" : "Off");
	}

	// add ledsOn
	inOutput->printf("<tr><td>LED State</td><td>%s</td></tr>", ledsOn ? "On" : "Off");

	// add overrideOn
	inOutput->printf("<tr><td>Override Active</td><td>%s</td></tr>", overrideActive ? "Yes" : "No");

	// add overrideState
	inOutput->printf("<tr><td>Override State</td><td>%s</td></tr>", overrideState ? "On" : "Off");

	// add motionSensorTrip
	inOutput->printf("<tr><td>Motion Sensor Trip</td><td>%s</td></tr>", motionSensorTrip ? "On" : "Off");

	// add settings.triggerLuxLow
	inOutput->printf("<tr><td>Lux Low Water</td><td>%f</td></tr>", settings.triggerLuxLow);

	// add settings.triggerLuxHigh
	inOutput->printf("<tr><td>Lux High Water</td><td>%f</td></tr>", settings.triggerLuxHigh);

	// add settings.lateNightStartHour and min
	inOutput->printf("<tr><td>Late Night Start</td><td>%02d:%02d</td></tr>", settings.lateNightStartHour, settings.lateNightStartMin);

	// add settings.motionTripTimeoutMins
	inOutput->printf("<tr><td>Motion Trip Timeout</td><td>%d</td></tr>", settings.motionTripTimeoutMins);

	// add settings.lateNightTimeoutMins
	inOutput->printf("<tr><td>Late Night Toggle Timeout</td><td>%d</td></tr>", settings.lateNightTimeoutMins);

	inOutput->printf("</table>");
}

void
CModule_OutdoorLightingControl::Sunset(
	char const*	inName)
{
	SystemMsg("Sunset\n");

	// Update state
	timeOfDay = eTimeOfDay_Night;
	olInterface->TimeOfDayChange(timeOfDay);

	luxLowWater = false;
	ledsOn = true;

	if(luminosityInterface != NULL)
	{
		SystemMsg("lux: sunset lux = %f brightness = %f\n", GetAvgLux(), GetAvgBrightness());
	}
}

void
CModule_OutdoorLightingControl::Sunrise(
	char const*	inName)
{
	SystemMsg("Sunrise\n");

	// Update state
	timeOfDay = eTimeOfDay_Day;
	olInterface->TimeOfDayChange(timeOfDay);

	ledsOn = false;
}

void
CModule_OutdoorLightingControl::LuxPeriodic(
	char const*	inName,
	void*		inRef)
{
	if(luminosityInterface == NULL || !settings.monitorLux)
	{
		return;
	}

	SystemMsg("lux: avgLux = %f, avgBrightness = %f, luxLowWater=%d\n", GetAvgLux(), GetAvgBrightness(), luxLowWater);
}

void
CModule_OutdoorLightingControl::ButtonPush(
	uint8_t		inPin,
	EPinEvent	inEvent,
	void*		inReference)
{
	if(inEvent == eDigitalIO_PinActivated)
	{
		SystemMsg("Button pushed");
		toggleLastTimeMS = gCurLocalMS;
		++toggleCount;
	}
}

bool
CModule_OutdoorLightingControl::LateNightAlarm(
	char const*	inName,
	void*		inRef)
{
	SystemMsg("Late Night Alarm\n");

	// Update state
	timeOfDay = eTimeOfDay_LateNight;
	olInterface->TimeOfDayChange(timeOfDay);

	ledsOn = false;

	return true;	// return true to reschedule the alarm
}

void
CModule_OutdoorLightingControl::MotionSensorTrigger(
	uint8_t		inPin,
	EPinEvent	inEvent,
	void*		inReference)
{
	if(timeOfDay != eTimeOfDay_Day)
	{
		SystemMsg("Motion sensor %s", inEvent == eDigitalIO_PinActivated ? "activated" : "deactivated");
	}

	if(inEvent == eDigitalIO_PinActivated)
	{
		motionSensorTrip = true;

		if(timeOfDay != eTimeOfDay_Day)
		{
			ledsOn = true;
		}
	}
	else if(motionSensorTrip)
	{
		if(timeOfDay != eTimeOfDay_Day)
		{
			SystemMsg("Motion sensor off, setting cooldown timer\n");

			// Set an event for settings.motionTripTimeoutMins mins from now, when it expires motionSensorTrip will be set to false
			MRealTimeRegisterEvent("MotionTripCD", settings.motionTripTimeoutMins * 60 * 1000000, true, CModule_OutdoorLightingControl::MotionTripCooldown, NULL);
		}
		else
		{
			// Its daytime so just immediately set motionSensorTrip to false 
			motionSensorTrip = false;
		}
	}
}

void
CModule_OutdoorLightingControl::MotionTripCooldown(
	char const*	inName,
	void*		inRef)
{
	SystemMsg("Motion trip cooled down\n");
	motionSensorTrip = false;

	if(timeOfDay == eTimeOfDay_LateNight)
	{
		ledsOn = false;
	}
}

void
CModule_OutdoorLightingControl::LateNightTimerExpire(
	char const*	inName,
	void*		inRef)
{
	SystemMsg("Late night timer expired\n");
	ledsOn = false;
}
	
uint8_t
CModule_OutdoorLightingControl::SetLEDState(
	IOutputDirector*	inOutput,
	int					inArgC,
	char const*			inArgv[])
{
	if(inArgC != 2)
	{
		return eCmd_Failed;
	}

	if(strcmp(inArgv[1], "on") == 0)
	{
		SystemMsg("ledsOn set to true\n");
		ledsOn = true;
	}
	else if(strcmp(inArgv[1], "off") == 0)
	{
		SystemMsg("ledsOn set to false\n");
		ledsOn = false;
	}
	else
	{
		return eCmd_Failed;
	}

	return eCmd_Succeeded;
}

uint8_t
CModule_OutdoorLightingControl::SetLateNightStartTime(
	IOutputDirector*	inOutput,
	int					inArgC,
	char const*			inArgv[])
{
	if(inArgC != 3)
	{
		return eCmd_Failed;
	}

	settings.lateNightStartHour = atoi(inArgv[1]);
	settings.lateNightStartMin = atoi(inArgv[2]);
	
	EEPROMSave();

	UpdateTimes();

	return eCmd_Succeeded;
}

uint8_t
CModule_OutdoorLightingControl::GetLateNightStartTime(
	IOutputDirector*	inOutput,
	int					inArgC,
	char const*			inArgv[])
{
	inOutput->printf("%02d:%02d\n", settings.lateNightStartHour, settings.lateNightStartMin);

	return eCmd_Succeeded;
}

uint8_t
CModule_OutdoorLightingControl::SetTriggerLux(
	IOutputDirector*	inOutput,
	int					inArgC,
	char const*			inArgv[])
{
	if(inArgC != 3)
	{
		return eCmd_Failed;
	}

	settings.triggerLuxLow = (float)atof(inArgv[1]);
	settings.triggerLuxHigh = (float)atof(inArgv[2]);

	EEPROMSave();

	return eCmd_Succeeded;
}

uint8_t
CModule_OutdoorLightingControl::GetTriggerLux(
	IOutputDirector*	inOutput,
	int					inArgC,
	char const*			inArgv[])
{
	inOutput->printf("low=%f high=%f\n", settings.triggerLuxLow, settings.triggerLuxHigh);

	return eCmd_Succeeded;
}

uint8_t
CModule_OutdoorLightingControl::SetLuxMonitor(
	IOutputDirector*	inOutput,
	int					inArgC,
	char const*			inArgv[])
{
	if(inArgC != 2)
	{
		return eCmd_Failed;
	}

	settings.monitorLux = strcmp(inArgv[1], "on") == 0;

	EEPROMSave();

	return eCmd_Succeeded;
}

uint8_t
CModule_OutdoorLightingControl::GetLuxMonitor(
	IOutputDirector*	inOutput,
	int					inArgC,
	char const*			inArgv[])
{
	inOutput->printf("monitorLux=%d\n", settings.monitorLux);

	return eCmd_Succeeded;
}

uint8_t
CModule_OutdoorLightingControl::SetMotionTripTimeout(
	IOutputDirector*	inOutput,
	int					inArgC,
	char const*			inArgv[])
{
	if(inArgC != 2)
	{
		return eCmd_Failed;
	}

	settings.motionTripTimeoutMins = atoi(inArgv[1]);

	EEPROMSave();

	return eCmd_Succeeded;
}

uint8_t
CModule_OutdoorLightingControl::GetMotionTripTimeout(
	IOutputDirector*	inOutput,
	int					inArgC,
	char const*			inArgv[])
{
	inOutput->printf("%d\n", settings.motionTripTimeoutMins);

	return eCmd_Succeeded;
}

uint8_t
CModule_OutdoorLightingControl::SetLateNightTimeout(
	IOutputDirector*	inOutput,
	int					inArgC,
	char const*			inArgv[])
{
	if(inArgC != 2)
	{
		return eCmd_Failed;
	}

	settings.lateNightTimeoutMins = atoi(inArgv[1]);

	EEPROMSave();

	return eCmd_Succeeded;
}

uint8_t
CModule_OutdoorLightingControl::GetLateNightTimeout(
	IOutputDirector*	inOutput,
	int					inArgC,
	char const*			inArgv[])
{
	inOutput->printf("%d\n", settings.lateNightTimeoutMins);

	return eCmd_Succeeded;
}

uint8_t
CModule_OutdoorLightingControl::SetOverride(
	IOutputDirector*	inOutput,
	int					inArgC,
	char const*			inArgv[])
{
	if(inArgC != 3)
	{
		return eCmd_Failed;
	}

	overrideActive = atoi(inArgv[1]) > 0;
	overrideState = atoi(inArgv[2]) > 0;

	SystemMsg("Setting override to overrideActive=%d overrideState=%d\n", overrideActive, overrideState);

	return eCmd_Succeeded;
}

uint8_t
CModule_OutdoorLightingControl::GetOverride(
	IOutputDirector*	inOutput,
	int					inArgC,
	char const*			inArgv[])
{
	inOutput->printf("active=%d state=%d\n", overrideActive, overrideState);

	return eCmd_Succeeded;
}

void
CModule_OutdoorLightingControl::TransformerTransitionOnCompleted(
	char const*	inName,
	void*		inRef)
{
	SystemMsg("Transformer transition on completed");
	olInterface->LEDStateChange(true);
}

void
CModule_OutdoorLightingControl::TransformerTransitionOffCompleted(
	char const*	inName,
	void*		inRef)
{
	SystemMsg("Transformer transition off completed");
	digitalWriteFast(transformerPin, false);
}
