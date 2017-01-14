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
#include "ELScheduler.h"
#include "ELAssert.h"

MModuleImplementation_Start(CModule_Scheduler)
MModuleImplementation_FinishGlobal(CModule_Scheduler, gSchedulerModule)

CModule_Scheduler*	gSchedulerModule;

static char const* gHoliday[] = 
{
	"none",
	"NewYearsEve",
	"NewYearsDay",
	"ValintinesDay",
	"SaintPatricksDay",
	"Easter",
	"IndependenceDay",
	"Halloween",
	"Thanksgiving",
	"ChristmasEve",
	"ChristmasDay",
	"USPresidentialElection",
};

static char const* gMonth[] =
{
	"none",
	"January",
	"February",
	"March",
	"April",
	"May",
	"June",
	"July",
	"August",
	"September",
	"October",
	"November",
	"December"
};

static char const* gMonthAbrv[] =
{
	"none",
	"Jan",
	"Feb",
	"Mar",
	"Apr",
	"May",
	"Jun",
	"Jul",
	"Aug",
	"Sept",
	"Oct",
	"Nov",
	"Dec"
};

static char const* gDayOfWeek[] =
{
	"Sunday",
	"Monday",
	"Tuesday",
	"Wednesday",
	"Thursday",
	"Friday",
	"Saturday"
};

static char const* gDayOfWeekAbrv[] =
{
	"Sun",
	"Mon",
	"Tue",
	"Wed",
	"Thu",
	"Fri",
	"Sat"
};

TSchedulerPeriodRef*
CModule_Scheduler::CreatePeriod(
	char const*			inPeriodName,
	ISchedulerHandler*	inObject,
	TSchedulerMethod	inMethod,
	void*				inReference)
{
	return NULL;
}

void
CModule_Scheduler::DestroyPeriod(
	TSchedulerPeriodRef*	inPeriodRef)
{

}

bool
CModule_Scheduler::SchedulePeriod(
	TSchedulerPeriodRef*	inPeriodRef,
	CSchedulerPeriod const&	inEventData)
{
	return false;
}

bool
CModule_Scheduler::InPeriod(
	TSchedulerPeriodRef*	inPeriodRef,
	TEpochTime				inEpochTime,
	bool					inLocalTime)
{
	return false;
}

static int
MatchHoliday(
	TString<64>& inStr)
{
	for(size_t i = 0; i < MStaticArrayLength(gHoliday); ++i)
	{
		if(_stricmp(inStr, gHoliday[i]) == 0)
		{
			return (int)i;
		}
	}

	return -1;
}

#if 0
static int
MatchMonth(
	TString<64>& inStr)
{
	for(size_t i = 0; i < MStaticArrayLength(gMonth); ++i)
	{
		if(_stricmp(inStr, gMonth[i]) == 0)
		{
			return (int)i;
		}
	}
	for(size_t i = 0; i < MStaticArrayLength(gMonthAbrv); ++i)
	{
		if(_stricmp(inStr, gMonthAbrv[i]) == 0)
		{
			return (int)i;
		}
	}

	return -1;
}

static int
MatchWeekday(
	TString<64>& inStr)
{
	for(size_t i = 0; i < MStaticArrayLength(gDayOfWeek); ++i)
	{
		if(_stricmp(inStr, gDayOfWeek[i]) == 0)
		{
			return (int)i;
		}
	}

	for(size_t i = 0; i < MStaticArrayLength(gDayOfWeekAbrv); ++i)
	{
		if(_stricmp(inStr, gDayOfWeek[i]) == 0)
		{
			return (int)i;
		}
	}

	return -1;
}
#endif

static int8_t
MatchWeekdays(
	TString<64>& inStr)
{
	size_t	count = 0;
	char const*	cp = inStr;
	char const*	sp = cp;
	uint8_t	result = 0;

	for(;;)
	{
		char c = *cp++;

		if(c == '|' || c == 0)
		{
			for(size_t i = 0; i < MStaticArrayLength(gDayOfWeek); ++i)
			{
				if(_strnicmp(sp, gDayOfWeek[i], count) == 0)
				{
					result |= 1 << i;
					break;
				}
			}
			for(size_t i = 0; i < MStaticArrayLength(gDayOfWeekAbrv); ++i)
			{
				if(_strnicmp(sp, gDayOfWeekAbrv[i], count) == 0)
				{
					result |= 1 << i;
					break;
				}
			}
			sp = cp;
			count = 0;
		}
		else
		{
			++count;
		}

		if(c == 0)
		{
			break;
		}
	}

	return result;
}

bool
CModule_Scheduler::ParseStringToThreshold(
	CSchedulerThreshold&	outThreshold,
	char const*				inString)
{
	enum
	{
		eDateOrHoliday,
		eYear,
		eMonth,
		eDay,
		eDayOf,
		eTimeSunriseSunset,
		eHour,
		eMinute,
		eSecond,
		eLocalOrUTC,
	};

	memset(&outThreshold, -1, sizeof(outThreshold));

	TString<64>	token;
	int		state = eDateOrHoliday;

	char const*	cp = inString;

	for(;;)
	{
		char c = *cp++;

		if(c != 0 && (isalnum(c) || c == '|' || c == '-' || c == '+'))
		{
			token.Append(c);
		}
		else if(token.GetLength() > 0)
		{
			switch(state)
			{
				case eDateOrHoliday:
					if(token == "date")
					{
						outThreshold.dateType = eDateType_Date;
						state = eYear;
					}
					else if(token == "any")
					{
						outThreshold.dateType = eDateType_Date;
						outThreshold.year = eScheduler_Any;
						outThreshold.month = eScheduler_Any;
						outThreshold.daysOfWeek = eScheduler_Any;
						outThreshold.dayOfMonth = eScheduler_Any;
						state = eDayOf;
					}
					else
					{
						int	holiday = MatchHoliday(token);
						if(holiday >= 0)
						{
							outThreshold.dateType = eDateType_Holiday;
							outThreshold.holiday = holiday;
							state = eMonth;
						}
						else
						{
							outThreshold.dateType = eDateType_Date;
							outThreshold.year = eScheduler_Any;
							outThreshold.month = eScheduler_Any;
							state = eDay;
						}
					}
					break;

				case eYear:
					if(token.IsDigit())
					{
						outThreshold.year = (int8_t)strtol((char*)token, NULL, 10);
					}
					else if(token == "any")
					{
						outThreshold.year = eScheduler_Any;
					}
					else
					{
						SystemMsg("ERROR: Could not find year");
						return false;
					}
					state = eMonth;
					break;

				case eMonth:
					if(token.IsDigit())
					{
						outThreshold.month = (int8_t)strtol((char*)token, NULL, 10);
						state = eDay;
					}
					else if(token == "any")
					{
						outThreshold.month = eScheduler_Any;
						state = eDay;
					}
					else if(token == "entire")
					{
						outThreshold.month = eScheduler_Entire;
						state = eDay;
					}
					else if(token == "week")
					{
						if(outThreshold.dateType == eDateType_Date)
						{
							outThreshold.daysOfWeek = 0x7F;
						}
						else
						{
							outThreshold.daysOfWeek = eScheduler_Entire;
						}
						state = eTimeSunriseSunset;
					}
					else
					{
						SystemMsg("ERROR: Could not find month");
						return false;
					}
					break;

				case eDay:
					if(token.IsDigit())
					{
						outThreshold.dayOfMonth = (int8_t)strtol((char*)token, NULL, 10);
					}
					else if(token == "any")
					{
						outThreshold.daysOfWeek = eScheduler_Any;
					}
					else if(token == "entire")
					{
						outThreshold.dayOfMonth = eScheduler_Entire;
					}
					else if(token == "week")
					{
						outThreshold.daysOfWeek = 0x7F;
					}
					else
					{
						outThreshold.daysOfWeek = MatchWeekdays(token);
					}
					state = eTimeSunriseSunset;
					break;

				case eDayOf:
					if(token == "any")
					{
						outThreshold.daysOfWeek = 0x7F;
					}
					else if(token == "week")
					{
						outThreshold.daysOfWeek = 0x7F;
					}
					else
					{
						outThreshold.daysOfWeek = MatchWeekdays(token);
					}
					state = eTimeSunriseSunset;
					break;

				case eTimeSunriseSunset:
					if(token == "time")
					{
						outThreshold.timeType = eTimeType_Time;
						state = eHour;
					}
					else if(token == "sunrise")
					{
						outThreshold.timeType = eTimeType_Sunrise;
						state = eHour;
					}
					else if(token == "sunset")
					{
						outThreshold.timeType = eTimeType_Sunset;
						state = eHour;
					}
					else if(token.IsNumber())
					{
						outThreshold.hour = (int8_t)strtol((char*)token, NULL, 10);
						outThreshold.timeType = eTimeType_Time;
						state = eMinute;
					}
					else
					{
						SystemMsg("ERROR: Invalid time specification");
						return false;
					}
					break;

				case eHour:
					if(token.IsNumber())
					{
						outThreshold.hour = (int8_t)strtol((char*)token, NULL, 10);
					}
					else if(token == "any")
					{
						outThreshold.hour = eScheduler_Any;
					}
					else
					{
						SystemMsg("ERROR: Could not find hour");
						return false;
					}
					state = eMinute;
					break;

				case eMinute:
					if(token.IsNumber())
					{
						outThreshold.minute = (int8_t)strtol((char*)token, NULL, 10);
					}
					else if(token == "any")
					{
						outThreshold.minute = eScheduler_Any;
					}
					else
					{
						SystemMsg("ERROR: Could not find minute");
						return false;
					}
					state = eSecond;
					break;

				case eSecond:
					if(token.IsNumber())
					{
						outThreshold.second = (int8_t)strtol((char*)token, NULL, 10);
					}
					else if(token == "any")
					{
						outThreshold.second = eScheduler_Any;
					}
					else
					{
						SystemMsg("ERROR: Could not find second");
						return false;
					}
					state = eLocalOrUTC;
					break;
					
				case eLocalOrUTC:
					if(token == "local")
					{
						outThreshold.localTime = true;
					}
					else if(token == "utc")
					{
						outThreshold.localTime = false;
					}
					else
					{
						SystemMsg("ERROR: missing local or utc specification");
						return false;
					}

					return true;
			}

			token.Clear();
		}


		if(c == 0)
		{
			break;
		}
	}

	return true;
}

static void
AddField(
	TString<128>&	outString,
	int8_t			inField,
	char const**	inArray = NULL)
{
	if(inField == eScheduler_Any)
	{
		outString.Append("any");
	}
	else if(inField == 0)
	{
		outString.Append("entire");
	}
	else
	{
		if(inArray == NULL)
		{
			outString.AppendF("%d", inField);
		}
		else
		{
			outString.AppendF("%s", inArray[inField]);
		}
	}
}

void
CModule_Scheduler::ThresholdToString(
	TString<128>&	outString,
	CSchedulerThreshold const&	inThreshold)
{
	outString.Clear();

	switch(inThreshold.dateType)
	{
		case eDateType_Holiday:
			if(inThreshold.holiday >= 0 && inThreshold.holiday <= eHoliday_ChristmasDay)
			{
				outString += gHoliday[inThreshold.holiday];
				outString += " ";
			}
			else
			{
				return;
			}

			if(inThreshold.month <= 0)
			{
				outString += "month ";
			}
			else if(inThreshold.daysOfWeek <= 0)
			{
				outString += "week ";
			}

			break;

		case eDateType_Date:
			AddField(outString, inThreshold.year);
			outString += '-';
			AddField(outString, inThreshold.month, gMonth);
			outString += '-';
			if(inThreshold.dayOfMonth > 0)
			{
				outString.AppendF("%02d", inThreshold.dayOfMonth);
			}
			else
			{
				bool	first = true;
				for(int i = 0; i <= eDay_Saturday; ++i)
				{
					if(inThreshold.daysOfWeek & (1 << i))
					{
						if(first)
						{
							first = false;
						}
						else
						{
							outString.Append('|');
						}
						outString.Append(gDayOfWeekAbrv[i]);
					}
				}
				outString += ' ';
			}
			break;

		default:
			return;
	}

	switch(inThreshold.timeType)
	{
		case eTimeType_Sunrise:
			outString.Append("sunrise:");
			break;

		case eTimeType_Sunset:
			outString.Append("sunset:");
			break;

		case eTimeType_Time:
			outString.Append("time:");
			break;

		default:
			return;
	}

	outString.AppendF("%02d", inThreshold.hour);
	outString += ':';
	outString.AppendF("%02d", inThreshold.minute);
	outString += ':';
	outString.AppendF("%02d", inThreshold.second);
}
