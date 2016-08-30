#ifndef _EL_CALENDAREVENT_H_
#define _EL_CALENDAREVENT_H_
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

	Get information about holidays from dates
*/

#include "EL.h"

enum EHoliday
{
	eHoliday_None,
	eHoliday_NewYearsEve,
	eHoliday_NewYearsDay,
	eHoliday_ValintinesDay,
	eHoliday_SaintPatricksDay,
	eHoliday_Easter,
	eHoliday_IndependenceDay,
	eHoliday_Holloween,
	eHoliday_Thanksgiving,
	eHoliday_ChristmasEve,
	eHoliday_ChristmasDay,
};

struct SCalendarEventTime
{
	int8_t	year;
	int8_t	month;
	int8_t	weekOfMonth;
	int8_t	dayOfMonth;
	int8_t	dayOfWeek;
	int8_t	hourOfDay;
	int8_t	minuteOfHour;
};

struct SCalendarEvent
{
	SCalendarEventTime	start;
	SCalendarEventTime	end;
};

EHoliday
GetHolidayForDate(
	int	inYear,
	int	inMonth,
	int	inDay);

void
GetDateForHoliday(
	int&		outMonth,
	int&		outDay,
	EHoliday	inHoliday,
	int			inYear);

#endif /* _EL_CALENDAREVENT_H_ */
