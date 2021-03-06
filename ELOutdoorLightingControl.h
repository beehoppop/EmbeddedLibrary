#ifndef _ELOUTDOORLIGHTINGCONTROL_h
#define _ELOUTDOORLIGHTINGCONTROL_h
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

enum
{
	eTimeOfDay_Day,
	eTimeOfDay_Night,
	eTimeOfDay_LateNight,
};

// A dummy class for the command handler method object
class IOutdoorLightingInterface
{
public:

	virtual void
	LEDStateChange(
		bool	inLEDsOn) = 0;

	virtual void
	MotionSensorStateChange(
		bool	inMotionSensorTriggered) = 0;

	virtual void
	LuxSensorStateChange(
		bool	inTriggered) = 0;

	virtual void
	PushButtonStateChange(
		int	inToggleCount) = 0;

	virtual void
	TimeOfDayChange(
		int	inTimeOfDay) = 0;

};

class CModule_OutdoorLightingControl : public CModule, public IRealTimeHandler, public ISunRiseAndSetEventHandler, public IDigitalIOEventHandler, public ICmdHandler, public IInternetHandler
{
public:

	MModule_Declaration(
		CModule_OutdoorLightingControl,
		IOutdoorLightingInterface*	inInterface,
		bool						inManualOnOff,
		uint8_t						inMotionSensorPin,
		uint8_t						inTransformerPin,
		uint8_t						inTogglePin,
		ILuminosity*				inLuminosityInterface)

	void
	SetOverride(
		bool	inOverrideActive,
		bool	inOverrideState);

	float
	GetAvgBrightness(
		void);

	float
	GetAvgLux(
		void);

private:

	CModule_OutdoorLightingControl(
		IOutdoorLightingInterface*	inInterface,
		bool						inManualOnOff,
		uint8_t						inMotionSensorPin,
		uint8_t						inTransformerPin,
		uint8_t						inTogglePin,
		ILuminosity*				inLuminosityInterface);

	void
	Setup(
		void);

	void
	Update(
		uint32_t inDeltaTimeUS);

	void
	UpdateTimes(
		void);

	void
	TimeChangeMethod(
		char const*	inName,
		bool		inTimeZone);

	void
	CommandHomePageHandler(
		IOutputDirector*	inOutput,
		int					inParamCount,
		char const**		inParamList);

	void
	Sunset(
		char const*	inName);

	void
	Sunrise(
		char const*	inName);

	void
	LuxPeriodic(
		TRealTimeEventRef	inRef,
		void*				inRefCon);

	bool
	LateNightAlarm(
		TRealTimeAlarmRef	inAlarmRef,
		void*				inRefCon);

	void
	ButtonPush(
		uint8_t		inPin,
		EPinEvent	inEvent,
		void*		inReference);

	void
	MotionSensorTrigger(
		uint8_t		inPin,
		EPinEvent	inEvent,
		void*		inReference);

	void
	MotionTripCooldown(
		TRealTimeEventRef	inRef,
		void*				inRefCon);

	void
	LateNightTimerExpire(
		TRealTimeEventRef	inRef,
		void*				inRefCon);

	uint8_t
	SetLEDState(
		IOutputDirector*	inOutput,
		int					inArgC,
		char const*			inArgv[]);

	uint8_t
	SetLateNightStartTime(
		IOutputDirector*	inOutput,
		int					inArgC,
		char const*			inArgv[]);

	uint8_t
	GetLateNightStartTime(
		IOutputDirector*	inOutput,
		int					inArgC,
		char const*			inArgv[]);

	uint8_t
	SetTriggerLux(
		IOutputDirector*	inOutput,
		int					inArgC,
		char const*			inArgv[]);

	uint8_t
	GetTriggerLux(
		IOutputDirector*	inOutput,
		int					inArgC,
		char const*			inArgv[]);

	uint8_t
	SetLuxMonitor(
		IOutputDirector*	inOutput,
		int					inArgC,
		char const*			inArgv[]);

	uint8_t
	GetLuxMonitor(
		IOutputDirector*	inOutput,
		int					inArgC,
		char const*			inArgv[]);

	uint8_t
	SetMotionTripTimeout(
		IOutputDirector*	inOutput,
		int					inArgC,
		char const*			inArgv[]);

	uint8_t
	GetMotionTripTimeout(
		IOutputDirector*	inOutput,
		int					inArgC,
		char const*			inArgv[]);

	uint8_t
	SetLateNightTimeout(
		IOutputDirector*	inOutput,
		int					inArgC,
		char const*			inArgv[]);

	uint8_t
	GetLateNightTimeout(
		IOutputDirector*	inOutput,
		int					inArgC,
		char const*			inArgv[]);

	uint8_t
	SetOverride(
		IOutputDirector*	inOutput,
		int					inArgC,
		char const*			inArgv[]);

	uint8_t
	GetOverride(
		IOutputDirector*	inOutput,
		int					inArgC,
		char const*			inArgv[]);

	void
	TransformerTransitionOnCompleted(
		TRealTimeEventRef	inRef,
		void*				inRefCon);

	void
	TransformerTransitionOffCompleted(
		TRealTimeEventRef	inRef,
		void*				inRefCon);

	struct SSettings
	{
		float		triggerLuxLow;	// When normalized brightness falls below this during the day turn on LEDs
		float		triggerLuxHigh;	// After low trigger when brightness returns above this turn LEDs off
		int			lateNightStartHour;
		int			lateNightStartMin;
		int			motionTripTimeoutMins;
		int			lateNightTimeoutMins;
		bool		monitorLux;
	};

	enum
	{
		eLuxBufferSize = 20,
	};

	SSettings	settings;

	uint8_t			motionSensorPin;
	uint8_t			transformerPin;
	uint8_t			togglePin;
	ILuminosity*	luminosityInterface;
	IOutdoorLightingInterface*		olInterface;

	TRealTimeAlarmRef	lateNightAlarm;
	TRealTimeAlarmRef	lateNightTimerExpireEvent;
	TRealTimeEventRef	luxPeriodicEvent;
	TRealTimeEventRef	XfrmToOffEvent;
	TRealTimeEventRef	XFrmToOnEvent;
	TRealTimeEventRef	MotionTripCooldownEvent;

	TSunRiseAndSetEventRef	SunriseEvent;
	TSunRiseAndSetEventRef	SunsetEvent;

	int			toggleCount;
	uint64_t	toggleLastTimeMS;

	float		luxBuffer[eLuxBufferSize];
	uint8_t		luxBufferIndex;

	uint8_t	timeOfDay;
	bool	motionSensorTrip;
	bool	luxLowWater;
	bool	ledsOn;

	bool	curLEDOnState;
	bool	curMotionSensorTrip;
	bool	curLuxTrigger;

	bool	overrideActive;
	bool	overrideState;

	bool	manualOnOff;
};

extern CModule_OutdoorLightingControl*	gOutdoorLighting;

#endif /* _ELOUTDOORLIGHTINGCONTROL_h */
