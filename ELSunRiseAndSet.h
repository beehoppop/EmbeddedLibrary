#ifndef _ELSUNRISEANDSET_H_
#define _ELSUNRISEANDSET_H_
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
	Based on the following code:

	SUNRISET.C - computes Sun rise/set times, start/end of twilight, and
				 the length of the day at any date and latitude

	Written as DAYLEN.C, 1989-08-16

	Modified to SUNRISET.C, 1992-12-01

	(c) Paul Schlyter, 1989, 1992

	Released to the public domain by Paul Schlyter, December 1992

	Ported to EmbeddedLibrary and generally restructured by Brent Pease 2015

*/

/*
	ABOUT

	Provide services for computing and triggering events around sunrise and sunset

*/

#include "ELRealTime.h"
#include "ELCommand.h"

const double	cSunOffset_SunsetSunRise		= -35.0/60.0;	// Use eSunRelativePosition_UpperLimb
const double	cSunOffset_CivilTwilight		= -6.0;			// Use eSunRelativePosition_Center
const double	cSunOffset_NauticalTwilight		= -12.0;		// Use eSunRelativePosition_Center
const double	cSunOffset_AstronomicalTwilight	= -18.0;		// Use eSunRelativePosition_Center

enum ESunRelativePosition
{
	eSunRelativePosition_Center,
	eSunRelativePosition_UpperLimb,
};

enum
{
	eMaxSunRiseSetEvents = 4,
};

#define MSunriseCreateEvent(inEventName, inMethod, ...) gSunRiseAndSet->CreateEvent(inEventName, this, static_cast<TSunRiseAndSetEventMethod>(&inMethod), true, ## __VA_ARGS__)
#define MSunsetCreateEvent(inEventName, inMethod, ...) gSunRiseAndSet->CreateEvent(inEventName, this, static_cast<TSunRiseAndSetEventMethod>(&inMethod), false, ## __VA_ARGS__)

typedef void*	TSunRiseAndSetEventRef;

class ISunRiseAndSetEventHandler
{
public:
};

typedef void
(ISunRiseAndSetEventHandler::*TSunRiseAndSetEventMethod)(
	char const*	inName);


class CModule_SunRiseAndSet : public CModule, public IRealTimeHandler, public ICmdHandler
{
public:

	MModule_Declaration(CModule_SunRiseAndSet)

	void
	SetLongitudeAndLatitude(
		double	inLongitude,
		double	inLatitude,
		bool	inSaveInEEPROM = false);

	void
	GetLongitudeAndLatitude(
		double&	outLongitude,
		double&	outLatitude);

	TSunRiseAndSetEventRef
	CreateEvent(
		char const*					inEventName,	// Must be a static string
		ISunRiseAndSetEventHandler*	inCmdHandler,
		TSunRiseAndSetEventMethod	inMethod,
		bool						inSunrise,
		double						inSunOffset = cSunOffset_SunsetSunRise,
		int							inSunRelativePosition = eSunRelativePosition_UpperLimb);

	void
	DestroyEvent(
		TSunRiseAndSetEventRef	inRef);

	void
	ScheduleEvent(
		TSunRiseAndSetEventRef	inEventRef,
		int						inYear,			// The specific year for the event or eAlarm_Any
		int						inMonth,		// The specific month for the event or eAlarm_Any
		int						inDay,			// The specific day for the event or eAlarm_Any
		int						inDOW,			// The specific day of week or eAlarm_Any
		bool					inUTC = false);

	void
	UnscheduleEvent(
		TSunRiseAndSetEventRef	inRef);

	/***************************************************************************/
	/* Note: year,month,date = calendar date, 1801-2099 only.             */
	/*       Eastern longitude positive, Western longitude negative       */
	/*       Northern latitude positive, Southern latitude negative       */
	/*       The longitude value IS critical in this function!            */
	/*       inSunOffset = the altitude which the Sun should cross        */
	/*               Set to -35/60 degrees for rise/set, -6 degrees       */
	/*               for civil, -12 degrees for nautical and -18          */
	/*               degrees for astronomical twilight.                   */
	/*        outSunriseTime = where to store the rise time               */
	/*        outSunsetTime  = where to store the set  time               */
	/*                Both times are relative to the specified altitude,  */
	/*                and thus this function can be used to compute       */
	/*                various twilight times, as well as rise/set times   */
	/* Return value:  0 = sun rises/sets this day, times stored at        */
	/*                    *trise and *tset.                               */
	/*               +1 = sun above the specified "horizon" 24 hours.     */
	/*                    *outSunrise set to time when the sun is at south,    */
	/*                    minus 12 hours while *tset is set to the south  */
	/*                    time plus 12 hours. "Day" length = 24 hours     */
	/*               -1 = sun is below the specified "horizon" 24 hours   */
	/*                    "Day" length = 0 hours, *trise and *tset are    */
	/*                    both set to the time when the sun is at south.  */
	/*                                                                    */
	/**********************************************************************/
	int
	GetSunRiseAndSetEpochTime(
		TEpochTime&	outSunrise,
		TEpochTime&	outSunset,
		int			inYear,		// 1801 to 2099
		int			inMonth,	// 1 to 12
		int			inDay,		// 1 to 31
		bool		inUTC,		// true if the given date and returned time are in UTC
		double		inLongitude = 1000.0,
		double		inLatitude = 1000.0,
		double		inSunOffset = cSunOffset_SunsetSunRise,
		int			inSunRelativePosition = eSunRelativePosition_UpperLimb);

	TEpochTime
	GetSunriseEpochTime(
		int		inYear,		// 1801 to 2099
		int		inMonth,	// 1 to 12
		int		inDay,		// 1 to 31
		bool	inUTC,		// true if the given date and returned time are in UTC
		double	inLongitude = 1000.0,
		double	inLatitude = 1000.0,
		double	inSunOffset = cSunOffset_SunsetSunRise,
		int		inSunRelativePosition = eSunRelativePosition_UpperLimb);

	TEpochTime
	GetSunsetEpochTime(
		int		inYear,		// 1801 to 2099
		int		inMonth,	// 1 to 12
		int		inDay,		// 1 to 31
		bool	inUTC,		// true if the given date and returned time are in UTC
		double	inLongitude = 1000.0,
		double	inLatitude = 1000.0,
		double	inSunOffset = cSunOffset_SunsetSunRise,
		int		inSunRelativePosition = eSunRelativePosition_UpperLimb);

	/**********************************************************************/
	/* Note: year,month,date = calendar date, 1801-2099 only.             */
	/*       Eastern longitude positive, Western longitude negative       */
	/*       Northern latitude positive, Southern latitude negative       */
	/*       The longitude value is not critical. Set it to the correct   */
	/*       longitude if you're picky, otherwise set to to, say, 0.0     */
	/*       The latitude however IS critical - be sure to get it correct */
	/*       inSunOffset = the altitude which the Sun should cross        */
	/*               Set to -35/60 degrees for rise/set, -6 degrees       */
	/*               for civil, -12 degrees for nautical and -18          */
	/*               degrees for astronomical twilight.                   */
	/**********************************************************************/
	double
	GetDayLength(
		int		inYear,		// 1801 to 2099
		int		inMonth,	// 1 to 12
		int		inDay,		// 1 to 31
		double	inLongitude = 1000.0,
		double	inLatitude = 1000.0,
		double	inSunOffset = cSunOffset_SunsetSunRise,
		int		inSunRelativePosition = eSunRelativePosition_UpperLimb);

private:
	
	CModule_SunRiseAndSet(
		);

	virtual void
	Setup(
		void);

	struct SEvent
	{
		char const*	name;
		bool		sunRise;
		int			year;
		int			month;
		int			day;
		int			dow;
		double		sunOffset;
		int			sunRelativePosition;
		bool		utc;
		TRealTimeAlarmRef			alarmRef;
		ISunRiseAndSetEventHandler*	cmdHandler;
		TSunRiseAndSetEventMethod	method;
	};

	uint8_t
	SerialSetLonLat(
		IOutputDirector*	inOutput,
		int					inArgC,
		char const*			inArgV[]);

	uint8_t
	SerialGetLonLat(
		IOutputDirector*	inOutput,
		int					inArgC,
		char const*			inArgV[]);

	void
	ScheduleNextEvent(
		SEvent*	inEvent);

	bool
	RealTimeAlarmHandler(
		TRealTimeAlarmRef	inRef,
		void*				inRefCon);

	void
	RealTimeChangeHandler(
		char const*	inName,
		bool		inTimeZone);

	struct SSettings
	{
		double	lon, lat;
	};

	SEvent		eventList[eMaxSunRiseSetEvents];
	SSettings	settings;
};

extern CModule_SunRiseAndSet*	gSunRiseAndSet;

#endif /* _ELSUNRISEANDSET_H_ */
