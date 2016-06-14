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
	eTransformerWarmUpSecs = 2,		// The number of seconds to allow the transformer to warm up before use
	eToggleCountResetMS = 1000,		// The time in ms to reset the pushbutton toggle count

	ePushCount_ToggleState = 1,		// The number of pushes to toggle state
	ePushCount_MotionTrip = 2,		// The number of pushes to simulate a motion trip
};

char const*	gTimeOfDayStr[] = {"Day", "Night", "Latenight"};

CModule_OutdoorLightingControl*	gOutdoorLighting;

CModule_OutdoorLightingControl::CModule_OutdoorLightingControl(
	IOutdoorLightingInterface*	inInterface,
	uint8_t						inMotionSensorPin,
	uint8_t						inTransformerPin,
	uint8_t						inTogglePin,
	ILuminosity*				inLuminosityInterface)
	:
	CModule("odlc", sizeof(SSettings), 0, &settings, 250000, 1),
	motionSensorPin(inMotionSensorPin),
	transformerPin(inTransformerPin),
	togglePin(inTogglePin),
	luminosityInterface(inLuminosityInterface),
	olInterface(inInterface)
{
	gOutdoorLighting = this;
	timeOfDay = eTimeOfDay_Day;
	motionSensorTrip = false;
	curTransformerState = false;
	curTransformerTransitionState = false;
	luxTriggerState = false;
	curLEDOnState = false;
	curMotionSensorTrip = false;
	overrideActive = false;
	overrideState = false;

	toggleCount = 0;
	toggleLastTimeMS = 0;

	// This is called to check if this module should be setup now or wait for the module SetupAll call
	CheckSetupNow();
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
	gSunRiseAndSet->RegisterSunsetEvent("Sunset1", eAlarm_Any, eAlarm_Any, eAlarm_Any, eAlarm_Any, this, static_cast<TSunRiseAndSetEventMethod>(&CModule_OutdoorLightingControl::Sunset));
	gSunRiseAndSet->RegisterSunriseEvent("Sunrise1", eAlarm_Any, eAlarm_Any, eAlarm_Any, eAlarm_Any, this, static_cast<TSunRiseAndSetEventMethod>(&CModule_OutdoorLightingControl::Sunrise));
	gRealTime->RegisterAlarm("LateNightAlarm", eAlarm_Any, eAlarm_Any, eAlarm_Any, eAlarm_Any, settings.lateNightStartHour, settings.lateNightStartMin, 0, this, static_cast<TRealTimeAlarmMethod>(&CModule_OutdoorLightingControl::LateNightAlarm), NULL);
	if(togglePin != 0xFF)
	{
		gDigitalIO->RegisterEventHandler(togglePin, false, this, static_cast<TDigitalIOEventMethod>(&CModule_OutdoorLightingControl::ButtonPush), NULL, 100);
	}
	if(motionSensorPin != 0xFF)
	{
		gDigitalIO->RegisterEventHandler(motionSensorPin, false, this, static_cast<TDigitalIOEventMethod>(&CModule_OutdoorLightingControl::MotionSensorTrigger), NULL);
	}
	gCommand->RegisterCommand("set_ledstate", this, static_cast<TCmdHandlerMethod>(&CModule_OutdoorLightingControl::SetLEDState), "[on | off] : Turn LEDs on or off until the next event");
	gCommand->RegisterCommand("set_latenightstarttime", this, static_cast<TCmdHandlerMethod>(&CModule_OutdoorLightingControl::SetLateNightStartTime));
	gCommand->RegisterCommand("get_latenightstarttime", this, static_cast<TCmdHandlerMethod>(&CModule_OutdoorLightingControl::GetLateNightStartTime));
	gCommand->RegisterCommand("set_triggerlux", this, static_cast<TCmdHandlerMethod>(&CModule_OutdoorLightingControl::SetTriggerLux));
	gCommand->RegisterCommand("get_triggerlux", this, static_cast<TCmdHandlerMethod>(&CModule_OutdoorLightingControl::GetTriggerLux));
	gCommand->RegisterCommand("set_motionTO", this, static_cast<TCmdHandlerMethod>(&CModule_OutdoorLightingControl::SetMotionTripTimeout));
	gCommand->RegisterCommand("get_motionTO", this, static_cast<TCmdHandlerMethod>(&CModule_OutdoorLightingControl::GetMotionTripTimeout));
	gCommand->RegisterCommand("set_latenightTO", this, static_cast<TCmdHandlerMethod>(&CModule_OutdoorLightingControl::SetLateNightTimeout));
	gCommand->RegisterCommand("get_latenightTO", this, static_cast<TCmdHandlerMethod>(&CModule_OutdoorLightingControl::GetLateNightTimeout));
	gCommand->RegisterCommand("set_override", this, static_cast<TCmdHandlerMethod>(&CModule_OutdoorLightingControl::SetOverride));
	gCommand->RegisterCommand("get_override", this, static_cast<TCmdHandlerMethod>(&CModule_OutdoorLightingControl::GetOverride));

	// Setup the initial state given the current time of day and sunset/sunrise time
	TEpochTime	curTime = gRealTime->GetEpochTime(false);
	int	year, month, day, dow, hour, min, sec;
	gRealTime->GetComponentsFromEpochTime(curTime, year, month, day, dow, hour, min, sec);

	SystemMsg("Setup time is %02d/%02d/%04d %02d:%02d:%02d\n", month, day, year, hour, min, sec);

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

	SystemMsg("Setup time of day = %d\n", timeOfDay);

	ledsOn = timeOfDay == eTimeOfDay_Night;

	gRealTime->RegisterEvent("LuxPeriodic", 1 * 60 * 1000000, false, this, static_cast<TRealTimeEventMethod>(&CModule_OutdoorLightingControl::LuxPeriodic), NULL);

	// Update lux trigger
	if(luminosityInterface != NULL)
	{
		float	curLux = luminosityInterface->GetActualLux();
		luxTriggerState = settings.triggerLux < curLux;
	}

	gInternet->CommandServer_RegisterFrontPage(this, static_cast<TInternetServerPageMethod>(&CModule_OutdoorLightingControl::CommandHomePageHandler));
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
			SystemMsg("Toggle state to %d\n", ledsOn);

			if(timeOfDay == eTimeOfDay_LateNight)
			{
				if(ledsOn)
				{
					gRealTime->RegisterEvent("LateNightTimerExpire", settings.lateNightTimeoutMins * 60 * 1000000, true, this, static_cast<TRealTimeEventMethod>(&CModule_OutdoorLightingControl::LateNightTimerExpire), NULL);
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
		// Update lux trigger
		float	curLux = luminosityInterface->GetActualLux();
		luxTriggerState = curLux < settings.triggerLux;

		if(timeOfDay == eTimeOfDay_Day)
		{
			ledsOn = luxTriggerState;
		}
	}

	if(motionSensorTrip != curMotionSensorTrip)
	{
		olInterface->MotionSensorStateChange(motionSensorTrip);
		curMotionSensorTrip = motionSensorTrip;
	}

	bool newLEDOnState = overrideActive ? overrideState : ledsOn;
	if(newLEDOnState != curLEDOnState)
	{
		SetTransformerState(newLEDOnState);

		if(curTransformerState == newLEDOnState)
		{
			olInterface->LEDStateChange(newLEDOnState);
			curLEDOnState = newLEDOnState;
		}
	}
}

void
CModule_OutdoorLightingControl::CommandHomePageHandler(
	IOutputDirector*	inOutput)
{
	// Send html via in Output to add to the command server home page served to clients

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
		inOutput->printf("<tr><td>Actual Lux</td><td>%f</td></tr>", luminosityInterface->GetActualLux());
	}

	// add ledsOn
	inOutput->printf("<tr><td>LED State</td><td>%s</td></tr>", ledsOn ? "On" : "Off");

	// add overrideOn
	inOutput->printf("<tr><td>Override Active</td><td>%s</td></tr>", overrideActive ? "Yes" : "No");

	// add overrideState
	inOutput->printf("<tr><td>Override State</td><td>%s</td></tr>", overrideState ? "On" : "Off");

	// add motionSensorTrip
	inOutput->printf("<tr><td>Motion Sensor Trip</td><td>%s</td></tr>", motionSensorTrip ? "On" : "Off");

	// add curTransformerState
	inOutput->printf("<tr><td>Transformer State</td><td>%s</td></tr>", curTransformerState ? "On" : "Off");

	// add luxTriggerState
	inOutput->printf("<tr><td>Lux Trigger State</td><td>%s</td></tr>", luxTriggerState ? "On" : "Off");

	// add settings.triggerLux
	inOutput->printf("<tr><td>Trigger Lux</td><td>%f</td></tr>", settings.triggerLux);

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

	ledsOn = true;

	if(luminosityInterface != NULL)
	{
		SystemMsg("lux: lux = %f\n", luminosityInterface->GetActualLux());
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
	if(luminosityInterface != NULL)
	{
		// Update lux trigger
		float	curLux = luminosityInterface->GetActualLux();

		SystemMsg("lux: lux = %f, luxTriggerState=%d\n", curLux, luxTriggerState);
	}
}

void
CModule_OutdoorLightingControl::ButtonPush(
	uint8_t		inPin,
	EPinEvent	inEvent,
	void*		inReference)
{
	if(inEvent == eDigitalIO_PinActivated)
	{
		toggleLastTimeMS = gCurLocalMS;
		++toggleCount;
		SystemMsg("toggleCount = %d\n", toggleCount);
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
	if(inEvent == eDigitalIO_PinActivated)
	{
		SystemMsg("Motion sensor tripped\n");
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

			// Set an event for settings.motionTripTimeoutMins mins from now
			gRealTime->RegisterEvent("MotionTripCD", settings.motionTripTimeoutMins * 60 * 1000000, true, this, static_cast<TRealTimeEventMethod>(&CModule_OutdoorLightingControl::MotionTripCooldown), NULL);
		}
		else
		{
			SystemMsg("Motion sensor off, daytime, no cooldown timer\n");
			motionSensorTrip = false;
			ledsOn = false;
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
	ledsOn = false;
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
		ledsOn = true;
	}
	else if(strcmp(inArgv[1], "off") == 0)
	{
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
		
	gRealTime->RegisterAlarm("LateNightAlarm", eAlarm_Any, eAlarm_Any, eAlarm_Any, eAlarm_Any, settings.lateNightStartHour, settings.lateNightStartMin, 0, this, static_cast<TRealTimeAlarmMethod>(&CModule_OutdoorLightingControl::LateNightAlarm), NULL);

	EEPROMSave();

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
	if(inArgC != 2)
	{
		return eCmd_Failed;
	}

	settings.triggerLux = (float)atof(inArgv[1]);

	EEPROMSave();

	return eCmd_Succeeded;
}

uint8_t
CModule_OutdoorLightingControl::GetTriggerLux(
	IOutputDirector*	inOutput,
	int					inArgC,
	char const*			inArgv[])
{
	inOutput->printf("%f\n", settings.triggerLux);

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
CModule_OutdoorLightingControl::SetTransformerState(
	bool	inState)
{
	if(curTransformerTransitionState == inState)
	{
		return;
	}

	SystemMsg("Transformer state to %d\n", inState);

	curTransformerTransitionState = inState;

	if(inState)
	{
		digitalWriteFast(transformerPin, true);

		// Let transformer warm up for 2 seconds
		gRealTime->RegisterEvent("XfmrWarmup", eTransformerWarmUpSecs * 1000000, true, this, static_cast<TRealTimeEventMethod>(&CModule_OutdoorLightingControl::TransformerTransitionEvent), NULL);
	}
	else
	{
		// Set the cur state to false right away to stop updating leds
		curTransformerState = false;

		// Let led update finish
		gRealTime->RegisterEvent("XfmrWarmup", eTransformerWarmUpSecs * 1000000, true, this, static_cast<TRealTimeEventMethod>(&CModule_OutdoorLightingControl::TransformerTransitionEvent), NULL);
	}
}

void
CModule_OutdoorLightingControl::TransformerTransitionEvent(
	char const*	inName,
	void*		inRef)
{
	SystemMsg("Transformer transition %d\n", curTransformerTransitionState);
	curTransformerState = curTransformerTransitionState;
	digitalWriteFast(transformerPin, curTransformerState);
}
