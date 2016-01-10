#ifndef _ELREALTIME_H_
#define _ELREALTIME_H_
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

	This module handles date and time. It allows for an external time provider, understands time zones, and provides alarm and event services.

	Serial Port Commands:
		time_set [year] [month] [day] [hour] [min] [sec] ["local" | "utc"] - sets the current time and sets it with the provider if possible
		time_get - prints the time to the console

		timezone_set [name] [dst start week] [dst start day of week] [dst start month] [dst start hour] [dst utc offset mins] [std start week] [std start day of week] [std start month] [std start hour] [std utc offset mins]
			[name] - the name of the time zone
			[dst start week] - 0 is the last week of the month, 1-4 specifies first through 4th week of the month for daylight savings time start
			[dst start day of week] - 1-7 the start day of the week for daylight savings time start
			[dst start month] - 1-12 the month for daylight savings time start
			[dst start hour] - the hour for daylight savings time start
			[dst utc offset mins] - the offset from UTC for daylight savings time
			[std start week] - 0 is the last week of the month, 1-4 specifies first through 4th week of the month for standard time start
			[std start day of week] - 1-7 the start day of the week for standard time start
			[std start month] - 1-12 the month for standard time start
			[std start hour] - the hour for standard time start
			[std utc offset mins] - the offset from UTC for standard time

		timezone_get - prints the current time zone info in the above format to the serial port

		realtime_dump - dump the list of scheduled alarms and events - used for debugging
		realtime_set_mult - speed up time by the given amount - used for debugging
*/

#include <ELModule.h>
#include <ELSerial.h>

#define MIsLeapYear(inYear) (((inYear) & 3) == 0 && (((inYear) % 25) != 0 || ((inYear) & 15) == 0))

enum
{
	eAlarm_Any = -1,

	eAlarm_MaxActive = 8,
	eEvent_MaxActive = 8,
	eTimeChangeHandler_MaxCount = 8,

	eRealTime_MaxNameLength = 15,
};

// This specifies a timezone change
struct STimeZoneOffsetSpecifier
{
    uint8_t	week;		// Last = 0, First = 1, Second = 2, Third = 3, Fourth = 4
    uint8_t	dayOfWeek;	// day of week, 1=Sun, 2=Mon, ... 7=Sat
    uint8_t	month;		// 1=Jan, 2=Feb, ... 12=Dec
    uint8_t	hour;		// 0-23
	int		offsetMins;	// This is the offset from UTC
};

// This specifies the start of daylight savings time and the start of standard time - together these define a single timezone
struct STimeZoneRule
{
    char						abbrev[eRealTime_MaxNameLength + 1];	
	STimeZoneOffsetSpecifier	dstStart;
	STimeZoneOffsetSpecifier	stdStart;
};

typedef uint32_t	TEpochTime;	// Secs since Jan 1 00:00 1970 - compatible with standard time definitions but redefined here to eliminate dependencies and conflicts

// A provider of a time source implements this interface
class IRealTimeDataProvider
{
public:
	
	virtual void
	SetUTCDateAndTime(
		int			inYear,			// 20xx
		int			inMonth,		// 1 to 12
		int			inDayOfMonth,	// 1 to 31
		int			inHour,			// 00 to 23
		int			inMinute,		// 00 to 59
		int			inSecond) = 0;	// 00 to 59

	// This requests a syncronization from the provider which must call gRealTime->SetDateAndTime() to set the time, this allows the fetching of the date and time to be asyncronous
	virtual void
	RequestSync(
		void) = 0;
};

// This is a dummy class for defining a alarm or event handler object
class IRealTimeHandler
{
public:

};

// A typedef for a alarm handler method, alarm methods should return true if it wants to be rescheduled
typedef bool
(IRealTimeHandler::*TRealTimeAlarmMethod)(
	char const*	inName,
	void*		inReference);

// A typedef for a event handler method
typedef void
(IRealTimeHandler::*TRealTimeEventMethod)(
	char const*	inName,
	void*		inReference);

// A typedef for a time change handler
typedef void
(IRealTimeHandler::*TRealTimeChangeMethod)(
	char const*	inName,
	bool		inTimeZone);

class CRealTime : public CModule, public ISerialCmdHandler
{
public:
	
	// Set the current timezone and optionally write it to eeprom
	void
	SetTimeZone(
		STimeZoneRule&	inTimeZone,
		bool			inWriteToEEPROM);

	// Set the provider and the frequency that time should be retrieved from it
	void
	SetProvider(
		IRealTimeDataProvider*	inProvider,				// If provider is NULL the user is responsible for calling SetDateAndTime and setting an alarm to periodically sync as needed
		uint32_t				inProviderSyncPeriod);	// In seconds, the period between refreshing the time with the given provider
	
	// Set the current date and time as components
	void
	SetDateAndTime(
		int		inYear,			// xxxx 4 digit year
		int		inMonth,		// 1 to 12
		int		inDayOfMonth,	// 1 to 31
		int		inHour,			// 00 to 23
		int		inMinute,		// 00 to 59
		int		inSecond,		// 00 to 59
		bool	inUTC = false);
	
	// Set the current date and time as a TEpochTime
	void
	SetEpochTime(
		TEpochTime	inEpochTime,
		bool		inUTC = false);
	
	// Get the current date and time components
	void
	GetDateAndTime(
		int&	outYear,		// xxxx 4 digit year
		int&	outMonth,		// 1 to 12
		int&	outDayOfMonth,	// 1 to 31
		int&	outDayOfWeek,	// 1 to 7
		int&	outHour,		// 00 to 23
		int&	outMinute,		// 00 to 59
		int&	outSecond,		// 00 to 59
		bool	inUTC = false);

	TEpochTime
	GetEpochTime(
		bool	inUTC = false);
	
	int
	GetYearNow(
		bool	inUTC = false);

	int	
	GetMonthNow(
		bool	inUTC = false);

	int
	GetDayOfMonthNow(
		bool	inUTC = false);

	int	
	GetDayOfWeekNow(
		bool	inUTC = false);

	int	
	GetHourNow(
		bool	inUTC = false);

	int	
	GetMinutesNow(
		bool	inUTC = false);

	int
	GetSecondsNow(
		bool	inUTC = false);

	int
	GetYearFromEpoch(
		TEpochTime	inEpochTime);

	int
	GetMonthFromEpoch(
		TEpochTime	inEpochTime);

	int
	GetDayOfMonthFromEpoch(
		TEpochTime	inEpochTime);

	int
	GetDayOfWeekFromEpoch(
		TEpochTime	inEpochTime);

	int
	GetHourFromEpoch(
		TEpochTime	inEpochTime);

	int
	GetMinuteFromEpoch(
		TEpochTime	inEpochTime);

	int
	GetSecondFromEpoch(
		TEpochTime	inEpochTime);
	
	// Compute epoch time given the time components
	TEpochTime
	GetEpochTimeFromComponents(
		int			inYear,			// xxxx 4 digit year
		int			inMonth,		// 1 to 12
		int			inDayOfMonth,	// 1 to 31
		int			inHour,			// 00 to 23
		int			inMinute,		// 00 to 59
		int			inSecond);		// 00 to 59
	
	// Compute the time components given epoch time
	void
	GetComponentsFromEpochTime(
		TEpochTime	inEpochTime,
		int&		outYear,		// xxxx 4 digit year
		int&		outMonth,		// 1 to 12
		int&		outDayOfMonth,	// 1 to 31
		int&		outDayOfWeek,	// 1 to 7
		int&		outHour,		// 00 to 23
		int&		outMinute,		// 00 to 59
		int&		outSecond);		// 00 to 59
	
	// Convert the given epoch time from local time to UTC time
	TEpochTime
	LocalToUTC(
		TEpochTime	inLocalEpochTime);

	// Convert the given epoch time from UTC to local time
	TEpochTime
	UTCToLocal(
		TEpochTime	inUTCEpochTime);
	
	void
	LocalToUTC(
		int&	ioYear,
		int&	ioMonth,
		int&	ioDayOfMonth,
		int&	ioHour,
		int&	ioMinute,
		int&	ioSecond);
	
	void
	UTCToLocal(
		int&	ioYear,
		int&	ioMonth,
		int&	ioDayOfMonth,
		int&	ioHour,
		int&	ioMinute,
		int&	ioSecond);

	// Return true if the given epoch time is in daylight savings time
	bool
	InDST(
		TEpochTime	inEpochTime,
		bool		inUTC = false);

	// Register a alarm handler to be called at the specified time or interval
	void
	RegisterAlarm(
		char const*			inAlarmName,	// All alarms are referred to by a unique name
		int					inYear,			// xxxx 4 digit year or eAlarm_Any
		int					inMonth,		// 1 to 12 or eAlarm_Any
		int					inDayOfMonth,	// 1 to 31 or eAlarm_Any
		int					inDayOfWeek,	// 1 to 7 or eAlarm_Any
		int					inHour,			// 00 to 23 or eAlarm_Any
		int					inMinute,		// 00 to 59 or eAlarm_Any
		int					inSecond,		// 00 to 59 or eAlarm_Any
		IRealTimeHandler*	inObject,		// The object on which the method below lives
		TRealTimeAlarmMethod	inMethod,	// The method on the above object
		void*				inReference,	// The reference value passed into the above method
		bool				inUTC = false);
	
	// Cancel the given alarm
	void
	CancelAlarm(
		char const*	inAlarmName);
	
	// Register an event to be called in the given time or for the given periodic interval
	void
	RegisterEvent(
		char const*			inEventName,	// All events have a unique name
		uint64_t			inPeriodUS,		// The period for which to call
		bool				inOnlyOnce,		// True if the event is only called once
		IRealTimeHandler*	inObject,		// The object on which the method below lives
		TRealTimeEventMethod	inMethod,	// The method on the above object
		void*				inReference);	// The reference value passed into the above method

	// Cancel the given event
	void
	CancelEvent(
		char const*	inEventName);
	
	// Register a handler for when time has changed
	void
	RegisterTimeChangeHandler(
		char const*				inName,	// All handlers have a unique name
		IRealTimeHandler*		inObject,
		TRealTimeChangeMethod	inMethod);
	
	// Cancel the given handler
	void
	CancelTimeChangeHandler(
		char const*	inName);

	// Create a provider for the DS3234 dead-on RTC clock on main SPI bus with the given chipselect pin
	IRealTimeDataProvider*
	CreateDS3234Provider(
		uint8_t	inChipSelectPin);

	// Given the date and time (of which components may be eAlarm_Any) return the next date time past the current time, return false if there is not a valid date and time past the current time
	bool
	GetNextDateTime(
		int&	ioYear,			// xxxx 4 digit year or eAlarm_Any
		int&	ioMonth,		// 1 to 12 or eAlarm_Any
		int&	ioDay,			// 1 to 31 or eAlarm_Any
		int&	ioDayOfWeek,	// 1 to 7 or eAlarm_Any
		int&	ioHour,			// 00 to 23 or eAlarm_Any
		int&	ioMin,			// 00 to 59 or eAlarm_Any
		int&	ioSec,			// 00 to 59 or eAlarm_Any
		bool	inUTC = false);

	// Given the date and time (of which components may be eAlarm_Any) return the next date time past the current time, return false if there is not a valid date and time past the current time
	bool
	GetNextDateTimeFromTime(
		TEpochTime	inTime,
		int&		ioYear,			// xxxx 4 digit year or eAlarm_Any
		int&		ioMonth,		// 1 to 12 or eAlarm_Any
		int&		ioDay,			// 1 to 31 or eAlarm_Any
		int&		ioDayOfWeek,	// 1 to 7 or eAlarm_Any
		int&		ioHour,			// 00 to 23 or eAlarm_Any
		int&		ioMin,			// 00 to 59 or eAlarm_Any
		int&		ioSec);			// 00 to 59 or eAlarm_Any
	
	// Return -1 if the given time components are earlier then now, 1 if later, or 0 if right now
	int
	CompareDateTimeWithNow(
		int		inYear,
		int		inMonth,
		int		inDay,
		int		inHour,
		int		inMin,
		int		inSec,
		bool	inUTC);

private:

	CRealTime(
		void);

	virtual void
	Setup(
		void);

	virtual void
	Update(
		uint32_t	inDeltaTimeUS);

	struct SAlarm
	{
		char				name[eRealTime_MaxNameLength + 1];
		int					year;
		int					month;
		int					dayOfMonth;
		int					dayOfWeek;
		int					hour;
		int					minute;
		int					second;
		IRealTimeHandler*	object;
		TRealTimeAlarmMethod	method;
		void*				reference;
		bool				utc;

		TEpochTime	nextTriggerTimeUTC;
	};
	
	struct SEvent
	{
		char				name[eRealTime_MaxNameLength + 1];
		uint64_t			periodUS;
		bool				onceOnly;
		uint64_t			lastFireTime;
		IRealTimeHandler*	object;
		TRealTimeEventMethod	method;
		void*				reference;
	};

	struct STimeChangeHandler
	{
		char					name[eRealTime_MaxNameLength + 1];
		IRealTimeHandler*		object;
		TRealTimeChangeMethod	method;
	};

	SAlarm	alarmArray[eAlarm_MaxActive];
	SEvent	eventArray[eEvent_MaxActive];
	STimeChangeHandler	timeChangeHandlerArray[eTimeChangeHandler_MaxCount];

	IRealTimeDataProvider*	provider;
	uint32_t				providerSyncPeriod;

	TEpochTime	epocUTCTimeAtLastSet;
	uint64_t	localMSAtLastSet;

	STimeZoneRule	timeZoneInfo;
	TEpochTime	dstStartUTCEpocTime;
	TEpochTime	dstEndUTCEpocTime;

	TEpochTime	dstStartUTC;
	TEpochTime	stdStartUTC;
	TEpochTime	dstStartLocal;
	TEpochTime	stdStartLocal;

	int	timeMultiplier;

	SAlarm*
	FindAlarmByName(
		char const*	inName);

	SAlarm*
	FindAlarmFirstEmpty(
		void);

	SEvent*
	FindEventByName(
		char const*	inName);

	SEvent*
	FindEventFirstEmpty(
		void);

	STimeChangeHandler*
	FindTimeChangeHandlerByName(
		char const*	inName);

	STimeChangeHandler*
	FindTimeChangeHandlerFirstEmpty(
		void);

	void
	ComputeDSTStartAndEnd(
		int	inYear);

	TEpochTime
	ComputeEpochTimeForOffsetSpecifier(
		STimeZoneOffsetSpecifier const&	inSpecifier,
		int								inYear);

	void
	ScheduleAlarm(
		SAlarm*	inAlarm);

	bool
	SerialSetTime(
		int			inArgC,
		char const*	inArgv[]);

	bool
	SerialGetTime(
		int			inArgC,
		char const*	inArgv[]);

	bool
	SerialSetTimeZone(
		int			inArgC,
		char const*	inArgv[]);

	bool
	SerialGetTimeZone(
		int			inArgC,
		char const*	inArgv[]);

	bool
	SerialDumpTable(
		int			inArgC,
		char const*	inArgv[]);

	bool
	SerialSetMultiplier(
		int			inArgC,
		char const*	inArgv[]);

	static CRealTime	module;
};

extern int			gDaysInMonth[12];	// This is 0 based not 1 based
extern CRealTime*	gRealTime;

#endif /* _ELREALTIME_H_ */
