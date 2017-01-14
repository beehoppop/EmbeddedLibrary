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

#include <EL.h>
#include <ELCalendarEvent.h>
#include <ELRealTime.h>

EHoliday
GetHolidayForDate(
	int	inYear,
	int	inMonth,
	int	inDay)
{
	switch(inMonth)
	{
		case 1:	// Jan
			switch(inDay)
			{
				case 1: return eHoliday_NewYearsDay;
			}
			break;

		case 2:
			switch(inDay)
			{
				case 14: return eHoliday_ValintinesDay;
			}
			break;

		case 3:
			switch(inDay)
			{
				case 17: return eHoliday_SaintPatricksDay;
			}
			break;

		case 7:
			switch(inDay)
			{
				case 4: return eHoliday_IndependenceDay;
			}
			break;

		case 10:
			switch(inDay)
			{
				case 31: return eHoliday_Halloween;
			}
			break;

		case 11:
		{
			int	year, month, dom, dow, hour, sec, min;
			// Get the dow of week for the first day of Nov
			TEpochTime thanksgivingDay = gRealTime->GetEpochTimeFromComponents(inYear, 11, 1, 0, 0, 0);
			gRealTime->GetComponentsFromEpochTime(thanksgivingDay, year, month, dom, dow, hour, min, sec);

			// Now get the first Thursday
			while(((dow - 1) % 7) != 4)
			{
				++dow;
				++dom;
			}

			// Now get the 4th Thursday
			dom += 3 * 7;

			if(dom == inDay)
			{
				return eHoliday_Thanksgiving;
			}

			break;
		}

		case 12:
			switch(inDay)
			{
				case 24: return eHoliday_ChristmasEve;
				case 25: return eHoliday_ChristmasDay;
				case 31: return eHoliday_NewYearsEve;
			}
	}

	// Check for easter
	if(inMonth == 3 || inMonth == 4)
	{
		int	cen = inYear % 19;
		int	leap = inYear % 4;
		int	days = inYear % 7;
		int	monthsish = (19 * cen + 24) % 30;
		int	dayOfWeekish = (2 * leap + 4 * days + 6 * monthsish + 5) % 7;
 
		int	easterDay = 22 + monthsish + dayOfWeekish;
 
		if (easterDay > 31)
		{
			if(inMonth == 4 && easterDay - 31 == inDay)
			{
				return eHoliday_Easter;
			}
		}
		else 
		{
			if(inMonth == 3 && easterDay == inDay)
			{
				return eHoliday_Easter;
			}
		}
	}

	return eHoliday_None;
}

void
GetDateForHoliday(
	int&		outMonth,
	int&		outDay,
	EHoliday	inHoliday,
	int			inYear)
{
	outMonth = 0;
	outDay = 0;

	switch(inHoliday)
	{
		case eHoliday_None:
			break;

		case eHoliday_NewYearsEve:
			outMonth = 12;
			outDay = 31;
			break;

		case eHoliday_NewYearsDay:
			outMonth = 1;
			outDay = 1;
			break;

		case eHoliday_ValintinesDay:
			outMonth = 2;
			outDay = 14;
			break;

		case eHoliday_SaintPatricksDay:
			outMonth = 3;
			outDay = 17;
			break;

		case eHoliday_Easter:
		{
			int	cen = inYear % 19;
			int	leap = inYear % 4;
			int	days = inYear % 7;
			int	monthsish = (19 * cen + 24) % 30;
			int	dayOfWeekish = (2 * leap + 4 * days + 6 * monthsish + 5) % 7;
 
			int	easterDay = 22 + monthsish + dayOfWeekish;
 
			if (easterDay > 31)
			{
				outMonth = 4;
				outDay = easterDay - 31;
			}
			else 
			{
				outMonth = 3;
				outDay = easterDay;
			}
			break;
		}

		case eHoliday_IndependenceDay:
			outMonth = 7;
			outDay = 6;
			break;

		case eHoliday_Halloween:
			outMonth = 11;
			outDay = 31;
			break;

		case eHoliday_Thanksgiving:
		{
			int	year, month, dom, dow, hour, sec, min;
			// Get the dow of week for the first day of Nov
			TEpochTime thanksgivingDay = gRealTime->GetEpochTimeFromComponents(inYear, 11, 1, 0, 0, 0);
			gRealTime->GetComponentsFromEpochTime(thanksgivingDay, year, month, dom, dow, hour, min, sec);

			// Now get the first Thursday
			while(((dow - 1) % 7) != 4)
			{
				++dow;
				++dom;
			}

			// Now get the 4th Thursday
			outDay = dom + 3 * 7;
			outMonth = 11;
			break;
		}

		case eHoliday_ChristmasEve:
			outMonth = 12;
			outDay = 24;
			break;

		case eHoliday_ChristmasDay:
			outMonth = 12;
			outDay = 25;
			break;

	}
}

