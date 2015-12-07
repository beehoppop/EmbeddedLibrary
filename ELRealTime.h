#pragma once

#ifndef _ELREALTIME_H_
#define _ELREALTIME_H_

#include "ELModule.h"

enum
{
	eAlarm_Any = -1,

	eAlarm_MaxActive = 8,
	eEvent_MaxActive = 8,

	eRealTime_MaxNameLength = 15,
};

struct STimeZoneOffsetSpecifier
{
    uint8_t	week;		// Last = 0, First = 1, Second = 2, Third = 3, Fourth = 4
    uint8_t	dayOfWeek;	// day of week, 1=Sun, 2=Mon, ... 7=Sat
    uint8_t	month;		// 1=Jan, 2=Feb, ... 12=Dec
    uint8_t	hour;		// 0-23
	int		offsetMins;
};

struct STimeZoneRule
{
    char						abbrev[eRealTime_MaxNameLength + 1];	
	STimeZoneOffsetSpecifier	dstStart;
	STimeZoneOffsetSpecifier	stdStart;
};

typedef uint32_t	TELEpochTime;	// Secs since Jan 1 00:00 1970 - compatible with standard time definitions but redefined here to eliminate dependencies and conflicts

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

	// This requests a syncronization from the provider which must call gRealTime.SetDateAndTime() to set the time, this allows the fetching of the date and time to be asyncronous
	virtual void
	RequestSync(
		void) = 0;
};

class IRealTimeHandler
{
public:
};

typedef void
(IRealTimeHandler::*TRealTimeMethod)(
	char const*	inName,
	void*		inReference);

class CRealTime : public CModule
{
public:

	CRealTime(
		void);

	virtual void
	Update(
		uint32_t	inDeltaTimeUS);
	
	void
	SetTimeZone(
		STimeZoneRule&	inTimeZone);

	void
	SetProvider(
		IRealTimeDataProvider*	inProvider,				// If provider is NULL the user is responsible for calling SetDateAndTime and setting an alarm to periodically sync as needed
		uint32_t				inProviderSyncPeriod);	// In seconds, the period between refreshing the time with the given provider

	void
	SetDateAndTime(
		int		inYear,			// xxxx 4 digit year
		int		inMonth,		// 1 to 12
		int		inDayOfMonth,	// 1 to 31
		int		inHour,			// 00 to 23
		int		inMinute,		// 00 to 59
		int		inSecond,		// 00 to 59
		bool	inUTC = false);
	
	void
	SetEpochTime(
		TELEpochTime	inEpochTime,
		bool			inUTC = false);

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

	TELEpochTime
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
		bool	inUTC);

	int
	GetYearFromEpoch(
		TELEpochTime	inEpochTime);

	int
	GetMonthFromEpoch(
		TELEpochTime	inEpochTime);

	int
	GetDayOfMonthFromEpoch(
		TELEpochTime	inEpochTime);

	int
	GetDayOfWeekFromEpoch(
		TELEpochTime	inEpochTime);

	int
	GetHourFromEpoch(
		TELEpochTime	inEpochTime);

	int
	GetMinuteFromEpoch(
		TELEpochTime	inEpochTime);

	int
	GetSecondFromEpoch(
		TELEpochTime	inEpochTime);

	TELEpochTime
	GetEpochTimeFromComponents(
		int			inYear,			// xxxx 4 digit year
		int			inMonth,		// 1 to 12
		int			inDayOfMonth,	// 1 to 31
		int			inHour,			// 00 to 23
		int			inMinute,		// 00 to 59
		int			inSecond);		// 00 to 59

	void
	GetComponentsFromEpochTime(
		TELEpochTime	inEpochTime,
		int&			outYear,		// xxxx 4 digit year
		int&			outMonth,		// 1 to 12
		int&			outDayOfMonth,	// 1 to 31
		int&			outDayOfWeek,	// 1 to 7
		int&			outHour,		// 00 to 23
		int&			outMinute,		// 00 to 59
		int&			outSecond);		// 00 to 59

	TELEpochTime
	LocalToUTC(
		TELEpochTime	inLocalEpochTime);

	TELEpochTime
	UTCToLocal(
		TELEpochTime	inUTCEpochTime);

	bool
	InDST(
		TELEpochTime	inEpochTime,
		bool			inUTC = false);

	void
	RegisterAlarm(
		char const*			inAlarmName,
		int					inYear,			// xxxx 4 digit year or eAlarm_Any
		int					inMonth,		// 1 to 12 or eAlarm_Any
		int					inDayOfMonth,	// 1 to 31 or eAlarm_Any
		int					inDayOfWeek,	// 1 to 7 or eAlarm_Any
		int					inHour,			// 00 to 23 or eAlarm_Any
		int					inMinute,		// 00 to 59 or eAlarm_Any
		int					inSecond,		// 00 to 59 or eAlarm_Any
		IRealTimeHandler*	inObject,		// The object on which the method below lives
		TRealTimeMethod		inMethod,		// The method on the above object
		void*				inReference,	// The reference value passed into the above method
		bool				inUTC = false);

	void
	CancelAlarm(
		char const*	inAlarmName);

	void
	RegisterEvent(
		char const*			inEventName,
		uint64_t			inPeriodUS,
		bool				inOnlyOnce,
		IRealTimeHandler*	inObject,		// The object on which the method below lives
		TRealTimeMethod		inMethod,		// The method on the above object
		void*				inReference);	// The reference value passed into the above method

	void
	CancelEvent(
		char const*	inEventName);

	IRealTimeDataProvider*
	CreateDS3234Provider(
		uint8_t	inChipSelectPin);

private:

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
		TRealTimeMethod		method;
		void*				reference;
		bool				utc;
	};
	
	struct SEvent
	{
		char				name[eRealTime_MaxNameLength + 1];
		uint64_t			periodUS;
		bool				onceOnly;
		uint64_t			lastFireTime;
		IRealTimeHandler*	object;
		TRealTimeMethod		method;
		void*				reference;
	};

	SAlarm	alarmArray[eAlarm_MaxActive];
	SEvent	eventArray[eEvent_MaxActive];

	IRealTimeDataProvider*	provider;
	uint32_t				provierSyncPeriodMS;

	TELEpochTime	epocUTCTimeAtLastSet;
	uint32_t		localMSAtLastSet;

	TELEpochTime	lastUTCEpochTimeAlarmCheck;

	STimeZoneRule	timeZoneInfo;
	TELEpochTime	dstStartUTCEpocTime;
	TELEpochTime	dstEndUTCEpocTime;

	TELEpochTime	dstStartUTC;
	TELEpochTime	stdStartUTC;
	TELEpochTime	dstStartLocal;
	TELEpochTime	stdStartLocal;

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

	void
	ComputeDSTStartAndEnd(
		int	inYear);

	TELEpochTime
	ComputeEpochTimeForOffsetSpecifier(
		STimeZoneOffsetSpecifier const&	inSpecifier,
		int								inYear);
};

extern int			gDaysInMonth[12];	// This is 0 based not 1 based
extern CRealTime	gRealTime;

#endif /* _ELREALTIME_H_ */
