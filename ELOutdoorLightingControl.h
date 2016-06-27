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
		uint8_t						inMotionSensorPin,
		uint8_t						inTransformerPin,
		uint8_t						inTogglePin,
		ILuminosity*				inLuminosityInterface)

	void
	SetOverride(
		bool	inOverrideActive,
		bool	inOverrideState);

private:

	CModule_OutdoorLightingControl(
		IOutdoorLightingInterface*	inInterface,
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
	CommandHomePageHandler(
		IOutputDirector*	inOutput);

	void
	Sunset(
		char const*	inName);

	void
	Sunrise(
		char const*	inName);

	void
	LuxPeriodic(
		char const*	inName,
		void*		inRef);

	bool
	LateNightAlarm(
		char const*	inName,
		void*		inRef);

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
		char const*	inName,
		void*		inRef);

	void
	LateNightTimerExpire(
		char const*	inName,
		void*		inRef);

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
	SetTransformerState(
		bool	inState);

	void
	TransformerTransitionEvent(
		char const*	inName,
		void*		inRef);

	struct SSettings
	{
		float		triggerLux;
		int			lateNightStartHour;
		int			lateNightStartMin;
		int			motionTripTimeoutMins;
		int			lateNightTimeoutMins;
	};

	SSettings	settings;

	uint8_t			motionSensorPin;
	uint8_t			transformerPin;
	uint8_t			togglePin;
	ILuminosity*	luminosityInterface;

	IOutdoorLightingInterface*		olInterface;

	int			toggleCount;
	uint64_t	toggleLastTimeMS;

	uint8_t	timeOfDay;
	bool	motionSensorTrip;
	bool	curTransformerState;
	bool	curTransformerTransitionState;
	bool	luxTriggerState;
	bool	ledsOn;

	bool	curLEDOnState;
	bool	curMotionSensorTrip;

	bool	overrideActive;
	bool	overrideState;
};

extern CModule_OutdoorLightingControl*	gOutdoorLighting;

#endif /* _ELOUTDOORLIGHTINGCONTROL_h */
