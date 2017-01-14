#ifndef _ELSCHEDULER_H_
#define _ELSCHEDULER_H_
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

#include "EL.h"
#include "ELModule.h"
#include "ELRealTime.h"
#include "ELString.h"
#include "ELCalendarEvent.h"

enum EScheduler_TimeType
{
	eTimeType_Sunrise,
	eTimeType_Sunset,
	eTimeType_Time,
};

enum EScheduler_DateType
{
	eDateType_Holiday,
	eDateType_Date
};

enum EEventType
{
	eEventType_Start,
	eEventType_End
};

enum EDayOfWeek
{
	eDay_Sunday,
	eDay_Monday,
	eDay_Tuesday,
	eDay_Wednesday,
	eDay_Thursday,
	eDay_Friday,
	eDay_Saturday
};

enum EScheduler
{
	eScheduler_Any = -1,	// Any year, month, or day of the event
	eScheduler_Entire = 0,	// The specific month or day of the event
};

typedef void*	TSchedulerPeriodRef;

struct CSchedulerThreshold
{
	int8_t	dateType;	// Is the date a Holiday (whose exact date can vary year to year) or a specific date
	int8_t	holiday;	// If this is a holiday the specific EScheduler_Holiday
	int8_t	year;		// For eDate_SpecificDate this can be -1 for any year or it's the exact year. This is ignored for eDate_Holiday
	int8_t	month;		// For eDate_SpecificDate this can be -1 for any month or it's the exact month. For eDate_Holiday -1 means either the start or end of the month
	int8_t	dayOfMonth;	// For eDate_SpecificDate this can be -1 for any day or it's the exact day. For eDate_Holiday -1 means wither the start or end of the month
	int8_t	daysOfWeek;	// This is a bitmap of the days of the week

	int8_t	timeType;
	int8_t	hour;		// For eTimeOfDay_Sunrise and eTimeOfDay_Sunset these are relative to either sunrise or sunset for the date specified above
	int8_t	minute;
	int8_t	second;

	bool	localTime;	// true if time is local, false if utc
};

struct CSchedulerPeriod
{
	CSchedulerThreshold	start;
	CSchedulerThreshold	end;
};

// This is a dummy class for defining a alarm or event handler object
class ISchedulerHandler
{
public:

};

// A typedef for a event handler method, event methods should return true if it wants to be rescheduled
typedef bool
(ISchedulerHandler::*TSchedulerMethod)(
	TSchedulerPeriodRef	inPeriodRef,
	void*				inRefCon);

class CModule_Scheduler : public CModule
{
public:

	MModule_Declaration(CModule_Scheduler)

	TSchedulerPeriodRef*
	CreatePeriod(
		char const*			inPeriodName,
		ISchedulerHandler*	inObject,		// The object on which the method below lives
		TSchedulerMethod	inMethod,		// The method on the above object
		void*				inReference);	// The reference value passed into the above method

	void
	DestroyPeriod(
		TSchedulerPeriodRef*	inPeriodRef);

	bool
	SchedulePeriod(
		TSchedulerPeriodRef*	inPeriodRef,
		CSchedulerPeriod const&	inEventData);

	bool
	InPeriod(
		TSchedulerPeriodRef*	inPeriodRef,
		TEpochTime				inEpochTime,
		bool					inLocalTime);

	/*
		Format: [date|holiday]:year/month/[day|week|dayofweek] [sunrise|sunset|time]:hour:minute:second
	*/
	bool
	ParseStringToThreshold(
		CSchedulerThreshold&	outThreshold,
		char const*				inString);

	void
	ThresholdToString(
		TString<128>&	outString,
		CSchedulerThreshold const&	inThreshold);
};

extern CModule_Scheduler*	gSchedulerModule;

#endif /* _ELSCHEDULER_H_ */
