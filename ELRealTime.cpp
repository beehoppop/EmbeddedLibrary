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

#include <ELModule.h>
#include <ELUtilities.h>
#include <ELCommand.h>
#include <ELAssert.h>

#include "ELRealTime.h"

int			gDaysInMonth[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
CRealTime	CRealTime::module;
CRealTime*	gRealTime;

#if defined(WIN32)
STimeZoneRule gTimeZone = 
{
	"Pacific",
	2, 1, 3, 2, -7 * 60,
	1, 1, 11, 2, -8 * 60,
};

class CRealTimeDataProvider_DS3234 : public IRealTimeDataProvider
{
public:

	CRealTimeDataProvider_DS3234(
		int	inChipSelect)
	{
	}

	virtual void
	SetUTCDateAndTime(
		int			inYear,			// 20xx
		int			inMonth,		// 1 to 12
		int			inDayOfMonth,	// 1 to 31
		int			inHour,			// 00 to 23
		int			inMinute,		// 00 to 59
		int			inSecond)		// 00 to 59
	{

	}

	// This requests a syncronization from the provider which must call gRealTime->SetDateAndTime() to set the time, this allows the fetching of the date and time to be asyncronous
	virtual void
	RequestSync(
		void)
	{
		time_t	localTime = time(NULL) /*- 33 * 60*/;
		gRealTime->SetEpochTime((TEpochTime)localTime, true);
	}
};

#else

class CRealTimeDataProvider_DS3234 : public IRealTimeDataProvider
{
public:
	
	CRealTimeDataProvider_DS3234(
		int		inChipSelect,
		bool	inUseAltSPI)
		:
		spiSettings(SPI_CLOCK_DIV128, MSBFIRST, SPI_MODE3),
		useAltSPI(inUseAltSPI)
	{
		chipselect = inChipSelect;

		pinMode(chipselect, OUTPUT);

		if(useAltSPI)
		{
			SPI.setMISO(8);
			SPI.setMOSI(7);
			SPI.setSCK(14);
		}

		SPI.begin();
		SPI.beginTransaction(spiSettings);
		digitalWrite(chipselect, LOW);  
		//set control register 
		SPI.transfer(0x8E);
		SPI.transfer(0x60); //60= disable Oscillator and Battery SQ wave @1hz, temp compensation, Alarms disabled
		digitalWrite(chipselect, HIGH);
		SPI.endTransaction();
		SPI.end();

		if(useAltSPI)
		{
			SPI.setMISO(12);
			SPI.setMOSI(11);
			SPI.setSCK(13);
		}

		delay(10);
	}

	virtual void
	SetUTCDateAndTime(
		int			inYear,			// xxxx or xx
		int			inMonth,		// 1 to 12
		int			inDayOfMonth,	// 1 to 31
		int			inHour,			// 00 to 23
		int			inMin,			// 00 to 59
		int			inSec)
	{
		int	centuryBit;

		if(inYear >= 1970 && inYear <= 1999)
		{
			centuryBit = 0;
			inYear -= 1900;
		}
		else if(inYear >= 2000 && inYear <= 2069)
		{
			centuryBit = 1;
			inYear -= 2000;
		}
		else if(inYear >= 70 && inYear <= 99)
		{
			centuryBit = 0;
		}
		else if(inYear >= 0 && inYear <= 69)
		{
			centuryBit = 1;
		}
		else
		{
			// This is an invalid year so bail
			DebugMsg("SetUTCDateAndTime: Invalid year");
			return;
		}

		if(inMonth < 1 || inMonth > 12 || inDayOfMonth < 1 || inDayOfMonth > 31 || inHour < 0 || inHour > 23 || inMin < 0 || inMin > 59 || inSec < 0 || inSec > 59)
		{
			DebugMsg("SetUTCDateAndTime: Invalid date");
			return;
		}

		int TimeDate [7] = {inSec, inMin, inHour, 0, inDayOfMonth, inMonth, inYear};

		if(useAltSPI)
		{
			SPI.setMISO(8);
			SPI.setMOSI(7);
			SPI.setSCK(14);
		}
		SPI.begin();
		for(int i = 0; i < 7; ++i)
		{
			if(i == 3)
				continue;

			int b = TimeDate[i] / 10;
			int a = TimeDate[i] % 10;

			TimeDate[i] = a + (b << 4);

			if(i == 5 && centuryBit == 1)
			{
				TimeDate[i] |= 0x80;
			}
			
			SPI.beginTransaction(spiSettings);
			digitalWrite(chipselect, LOW);
			SPI.transfer(i + 0x80); 
			SPI.transfer(TimeDate[i]);        
			digitalWrite(chipselect, HIGH);
			SPI.endTransaction();
		}
		SPI.end();
		if(useAltSPI)
		{
			SPI.setMISO(12);
			SPI.setMOSI(11);
			SPI.setSCK(13);
		}
	}

	virtual void
	RequestSync(
		void)
	{
		int TimeDate [7]; //second,minute,hour,null,day,month,year

		DebugMsg("cs=%d", chipselect);

		if(useAltSPI)
		{
			SPI.setMISO(8);
			SPI.setMOSI(7);
			SPI.setSCK(14);
		}
		SPI.begin();
		for(int i = 0; i < 7; ++i)
		{
			if(i == 3)
				continue;

			SPI.beginTransaction(spiSettings);
			digitalWrite(chipselect, LOW);
			SPI.transfer(i + 0x00); 
			unsigned int n = SPI.transfer(0x00);        
			digitalWrite(chipselect, HIGH);
			SPI.endTransaction();

			int a = n & 0x0F;
			int b = (n & 0x70) >> 4;

			TimeDate[i] = a + b * 10;	

			if(i == 5 && (TimeDate[i] & 0x80))
			{
				// No actual need to do anything with the century bit since the year number dictates 1900 or 2000
				TimeDate[i] &= 0x1F;
			}
		}
		SPI.end();
		if(useAltSPI)
		{
			SPI.setMISO(12);
			SPI.setMOSI(11);
			SPI.setSCK(13);
		}

		if(TimeDate[6] >= 70 && TimeDate[6] <= 99)
		{
			TimeDate[6] += 1900;
		}
		else
		{
			TimeDate[6] += 2000;
		}

		DebugMsg("got %d %d %d %d %d %d\n", TimeDate[6], TimeDate[5], TimeDate[4], TimeDate[2], TimeDate[1], TimeDate[0]);

		if(TimeDate[5] < 1 || TimeDate[5] > 12 || TimeDate[4] < 1 || TimeDate[4] > 31 || TimeDate[2] < 0 || TimeDate[2] > 23 || TimeDate[1] < 0 || TimeDate[1] > 59 || TimeDate[0] < 0 || TimeDate[0] > 59)
		{
			DebugMsg(eDbgLevel_Basic, "RequestSync: Invalid date from hardware");
			return;
		}

		gRealTime->SetDateAndTime(TimeDate[6], TimeDate[5], TimeDate[4], TimeDate[2], TimeDate[1], TimeDate[0], true);
	}

	int			chipselect;
	SPISettings	spiSettings;
	bool		useAltSPI;
};
#endif

CRealTime::CRealTime(
	)
	:
	CModule(
		"rtc ",					// The module ID
		sizeof(STimeZoneRule),	// eeprom size
		1,						// eeprom version number
		&timeZoneInfo,
		1000000,				// Call the update method once a second
		1)
{
	gRealTime = this;
}

void
CRealTime::Setup(
	void)
{
	if((uint8_t)timeZoneInfo.abbrev[0] == 0xFF)
	{
		// no time zone is set so just zero it out
		memset(&timeZoneInfo, 0, sizeof(timeZoneInfo));
	}

	#if defined(WIN32)
		// For testing
		SetTimeZone(gTimeZone, false);
	#endif

	gCommand->RegisterCommand("time_set", this, static_cast<TCmdHandlerMethod>(&CRealTime::SerialSetTime));
	gCommand->RegisterCommand("time_get", this, static_cast<TCmdHandlerMethod>(&CRealTime::SerialGetTime));
	gCommand->RegisterCommand("timezone_set", this, static_cast<TCmdHandlerMethod>(&CRealTime::SerialSetTimeZone));
	gCommand->RegisterCommand("timezone_get", this, static_cast<TCmdHandlerMethod>(&CRealTime::SerialGetTimeZone));
	gCommand->RegisterCommand("rt_dump", this, static_cast<TCmdHandlerMethod>(&CRealTime::SerialDumpTable));
	gCommand->RegisterCommand("rt_set_mult", this, static_cast<TCmdHandlerMethod>(&CRealTime::SerialSetMultiplier));

	timeMultiplier = 1;

	SetEpochTime(0, false);
}

void
CRealTime::Update(
	uint32_t	inDeltaTimeUS)
{
	if(provider != NULL && (gCurLocalMS - localMSAtLastSet) / 1000 >= providerSyncPeriod && timeMultiplier == 1)
	{
		provider->RequestSync();
	}

	TEpochTime	curEpochTimeUTC = GetEpochTime(true);

	SAlarm* curAlarm = alarmArray;
	for(int alarmItr = 0; alarmItr < eAlarm_MaxActive; ++alarmItr, ++curAlarm)
	{
		if(curAlarm->name[0] == 0)
		{
			continue;
		}

		if(curAlarm->nextTriggerTimeUTC <= curEpochTimeUTC)
		{
			// this alarm is triggered
			DebugMsg(eDbgLevel_Medium, "Triggering alarm %s\n", curAlarm->name);
			curAlarm->nextTriggerTimeUTC = 0;
			bool	reschedule = (curAlarm->object->*curAlarm->method)(curAlarm->name, curAlarm->reference);
			if(reschedule)
			{
				ScheduleAlarm(curAlarm);
			}
			else if(curAlarm->nextTriggerTimeUTC == 0)
			{
				// if the alarm has not been rescheduled by the event handler method cancel it here so it does not fire repeatedly
				curAlarm->name[0] = 0;
			}
		}
	}

	// Now look for events to fire
	SEvent*	curEvent = eventArray;
	for(int itr = 0; itr < eEvent_MaxActive; ++itr, ++curEvent)
	{
		if(curEvent->name[0] == 0)
		{
			continue;
		}

		if(gCurLocalUS - curEvent->lastFireTime >= curEvent->periodUS)
		{
			curEvent->lastFireTime = gCurLocalUS;
			DebugMsg(eDbgLevel_Medium, "Triggering event %s\n", curEvent->name);
			(curEvent->object->*curEvent->method)(curEvent->name, curEvent->reference);
			if(curEvent->onceOnly)
			{
				curEvent->name[0] = 0;
			}
		}
	}
}

void
CRealTime::SetTimeZone(
	STimeZoneRule&	inTimeZone,
	bool			inWriteToEEPROM)
{
	timeZoneInfo = inTimeZone;

	if(inWriteToEEPROM)
	{
		EEPROMSave();
	}

	STimeChangeHandler*	curHandler = timeChangeHandlerArray;
	for(int i = 0; i < eTimeChangeHandler_MaxCount; ++i, ++curHandler)
	{
		if(curHandler->name[0] != 0 && curHandler->object != NULL)
		{
			(curHandler->object->*curHandler->method)(curHandler->name, true);
		}
	}
}

void
CRealTime::SetProvider(
	IRealTimeDataProvider*	inProvider,				// If provider is NULL the user is responsible for calling SetDateAndTime and setting an alarm to periodically sync as needed
	uint32_t				inProviderSyncPeriod)	// In seconds, the period between refreshing the time with the given provider
{
	provider = inProvider;
	providerSyncPeriod = inProviderSyncPeriod;

	if(provider != NULL)
	{
		provider->RequestSync();
	}
}

void
CRealTime::SetDateAndTime(
	int		inYear,			// 20xx
	int		inMonth,		// 1 to 12
	int		inDayOfMonth,	// 1 to 31
	int		inHour,			// 00 to 23
	int		inMin,			// 00 to 59
	int		inSec,			// 00 to 59
	bool	inUTC)
{
	TEpochTime	epochTime = GetEpochTimeFromComponents(inYear, inMonth, inDayOfMonth, inHour, inMin, inSec);

	SetEpochTime(epochTime, inUTC);
}

void 
CRealTime::SetEpochTime(
	TEpochTime	inEpochTime,
	bool		inUTC)
{
	TEpochTime	oldEpochTime = GetEpochTime(inUTC);

	localMSAtLastSet = gCurLocalMS;

	if(inUTC)
	{
		epocUTCTimeAtLastSet = inEpochTime;
	}
	else
	{
		epocUTCTimeAtLastSet = LocalToUTC(inEpochTime);
	}

	if(oldEpochTime != GetEpochTime(inUTC))
	{
		STimeChangeHandler*	curHandler = timeChangeHandlerArray;
		for(int i = 0; i < eTimeChangeHandler_MaxCount; ++i, ++curHandler)
		{
			if(curHandler->name[0] != 0 && curHandler->object != NULL)
			{
				(curHandler->object->*curHandler->method)(curHandler->name, false);
			}
		}
	}
}

void
CRealTime::GetDateAndTime(
	int&	outYear,
	int&	outMonth,
	int&	outDayOfMonth,
	int&	outDayOfWeek,
	int&	outHour,
	int&	outMinute,
	int&	outSecond,
	bool	inUTC)
{
	GetComponentsFromEpochTime(GetEpochTime(inUTC), outYear, outMonth, outDayOfMonth, outDayOfWeek, outHour, outMinute, outSecond);
}

TEpochTime 
CRealTime::GetEpochTime(
	bool	inUTC)
{
	TEpochTime	result = epocUTCTimeAtLastSet + (uint32_t)((gCurLocalMS - localMSAtLastSet) * timeMultiplier / 1000);

	if(!inUTC)
	{
		result = UTCToLocal(result);
	}

	return result;
}

int
CRealTime::GetYearNow(
	bool	inUTC)
{
	return GetYearFromEpoch(GetEpochTime(inUTC));
}

int
CRealTime::GetMonthNow(
	bool	inUTC)
{
	return GetMonthFromEpoch(GetEpochTime(inUTC));
}

int
CRealTime::GetDayOfMonthNow(
	bool	inUTC)
{
	return GetDayOfMonthFromEpoch(GetEpochTime(inUTC));
}

int 
CRealTime::GetDayOfWeekNow(
	bool	inUTC)
{
	return GetDayOfWeekFromEpoch(GetEpochTime(inUTC));
}

int
CRealTime::GetHourNow(
	bool	inUTC)
{
	return GetHourFromEpoch(GetEpochTime(inUTC));
}

int 
CRealTime::GetMinutesNow(
	bool	inUTC)
{
	return GetMinuteFromEpoch(GetEpochTime(inUTC));
}

int
CRealTime::GetSecondsNow(
	bool	inUTC)
{
	return GetSecondFromEpoch(GetEpochTime(inUTC));
}

int
CRealTime::GetYearFromEpoch(
	TEpochTime	inEpochTime)
{
	TEpochTime	daysEpochTime = inEpochTime / (60 * 60 * 24);
	int			year = 1970;
	TEpochTime	days = 0;

	for(;;)
	{
		days += MIsLeapYear(year) ? 366 : 365;
		if(days > daysEpochTime)
		{
			break;
		}

		year++;
	}

	return year;
}

int
CRealTime::GetMonthFromEpoch(
	TEpochTime	inEpochTime)
{
	TEpochTime	daysEpochTime = inEpochTime / (60 * 60 * 24);
	int				year = 1970;
	TEpochTime	days = 0;
	int				month;
	TEpochTime	monthLength;

	for(;;)
	{
		days += MIsLeapYear(year) ? 366 : 365;
		if(days > daysEpochTime)
		{
			break;
		}

		year++;
	}

	days -= MIsLeapYear(year) ? 366 : 365;
	daysEpochTime -= days;

	for(month = 0; month < 12; ++month)
	{
		monthLength = gDaysInMonth[month];

		if(month == 1 && MIsLeapYear(year))
		{
			++monthLength;
		}
    
		if(daysEpochTime >= monthLength)
		{
			daysEpochTime -= monthLength;
		}
		else
		{
			break;
		}
	}

	return month + 1;
}

int
CRealTime::GetDayOfMonthFromEpoch(
	TEpochTime	inEpochTime)
{
	TEpochTime	daysEpochTime = inEpochTime / (60 * 60 * 24);
	int				year = 1970;
	TEpochTime	days = 0;
	int				month;
	TEpochTime	monthLength;

	for(;;)
	{
		days += MIsLeapYear(year) ? 366 : 365;
		if(days > daysEpochTime)
		{
			break;
		}

		year++;
	}

	days -= MIsLeapYear(year) ? 366 : 365;
	daysEpochTime -= days;

	for(month = 0; month < 12; ++month)
	{
		monthLength = gDaysInMonth[month];

		if(month == 1 && MIsLeapYear(year))
		{
			++monthLength;
		}
    
		if(daysEpochTime >= monthLength)
		{
			daysEpochTime -= monthLength;
		}
		else
		{
			break;
		}
	}

	return daysEpochTime + 1;
}

int
CRealTime::GetDayOfWeekFromEpoch(
	TEpochTime	inEpochTime)
{
	TEpochTime	daysEpochTime = inEpochTime / (60 * 60 * 24);

	return ((daysEpochTime + 4) % 7) + 1;  // Jan 1, 1970 was a Thursday (ie day 4 starting from 0 of the week);
}

int
CRealTime::GetHourFromEpoch(
	TEpochTime	inEpochTime)
{
	return (inEpochTime / (60 * 60)) % 24;
}

int
CRealTime::GetMinuteFromEpoch(
	TEpochTime	inEpochTime)
{
	return (inEpochTime / 60) % 60;
}

int
CRealTime::GetSecondFromEpoch(
	TEpochTime	inEpochTime)
{
	return inEpochTime % 60;
}

TEpochTime
CRealTime::GetEpochTimeFromComponents(
	int inYear,
	int inMonth, 
	int inDayOfMonth, 
	int inHour,
	int inMinute, 
	int inSecond)
{
	TEpochTime result;

	// Add in years
	result = (inYear - 1970) * 60 * 60 * 24 * 365;

	// Add in leap days
	for(int year = 1970; year < inYear; year++)
	{
		if(MIsLeapYear(year))
		{
			result += 60 * 60 * 24;
		}
	}
  
	// add in month
	for(int i = 1; i < inMonth; ++i)
	{
		result += 60 * 60 * 24 * gDaysInMonth[i - 1];
		if((i == 2) && MIsLeapYear(inYear))
		{ 
			result += 60 * 60 * 24;
		}
	}

	// Add in days, hours, minutes, seconds
	result += (inDayOfMonth - 1) * 60 * 60 * 24;
	result += inHour * 60 * 60;
	result += inMinute * 60;
	result += inSecond;

	return result; 
}

void
CRealTime::GetComponentsFromEpochTime(
	TEpochTime	inEpocTime,
	int&		outYear,		// 20xx
	int&		outMonth,		// 1 to 12
	int&		outDayOfMonth,	// 1 to 31
	int&		outDayOfWeek,	// 1 to 7
	int&		outHour,		// 00 to 23
	int&		outMinute,		// 00 to 59
	int&		outSecond)		// 00 to 59
{
	int				year;
	int				month;
	TEpochTime	monthLength;
	TEpochTime	remainingTime;
	TEpochTime	days;

	remainingTime = inEpocTime;

	outSecond = remainingTime % 60;
	remainingTime /= 60;

	outMinute = remainingTime % 60;
	remainingTime /= 60;

	outHour = remainingTime % 24;
	remainingTime /= 24;

	outDayOfWeek = ((remainingTime + 4) % 7) + 1;  // Jan 1, 1970 was a Thursday (ie day 4 starting from 0 of the week)
  
	year = 1970;  
	days = 0;
	for(;;)
	{
		days += MIsLeapYear(year) ? 366 : 365;
		if(days > remainingTime)
		{
			break;
		}

		year++;
	}
	outYear = year;
  
	days -= MIsLeapYear(year) ? 366 : 365;
	remainingTime -= days;
  
	days = 0;
	monthLength = 0;
	for(month = 0; month < 12; ++month)
	{
		monthLength = gDaysInMonth[month];

		if(month == 1 && MIsLeapYear(year))
		{
			++monthLength;
		}
    
		if(remainingTime >= monthLength)
		{
			remainingTime -= monthLength;
		}
		else
		{
			break;
		}
	}

	outMonth = month + 1;
	outDayOfMonth = remainingTime + 1;
}

TEpochTime
CRealTime::LocalToUTC(
	TEpochTime	inLocalEpochTime)
{
	if(GetYearFromEpoch(inLocalEpochTime) != GetYearFromEpoch(dstStartLocal))
	{
		ComputeDSTStartAndEnd(GetYearFromEpoch(inLocalEpochTime));
	}

	if(InDST(inLocalEpochTime, false))
	{
		return inLocalEpochTime - timeZoneInfo.dstStart.offsetMins * 60;
	}

	return inLocalEpochTime - timeZoneInfo.stdStart.offsetMins * 60;
}

TEpochTime
CRealTime::UTCToLocal(
	TEpochTime	inUTCEpochTime)
{
	if(GetYearFromEpoch(inUTCEpochTime) != GetYearFromEpoch(dstStartUTC))
	{
		ComputeDSTStartAndEnd(GetYearFromEpoch(inUTCEpochTime));
	}

	if(InDST(inUTCEpochTime, true))
	{
		return inUTCEpochTime + timeZoneInfo.dstStart.offsetMins * 60;
	}

	return inUTCEpochTime + timeZoneInfo.stdStart.offsetMins * 60;
}
	
void
CRealTime::LocalToUTC(
	int&	ioYear,
	int&	ioMonth,
	int&	ioDayOfMonth,
	int&	ioHour,
	int&	ioMinute,
	int&	ioSecond)
{
	TEpochTime	localTime = GetEpochTimeFromComponents(ioYear, ioMonth, ioDayOfMonth, ioHour, ioMinute, ioSecond);
	int	dow;
	GetComponentsFromEpochTime(LocalToUTC(localTime), ioYear, ioMonth, ioDayOfMonth, dow, ioHour, ioMinute, ioSecond);
}
	
void
CRealTime::UTCToLocal(
	int&	ioYear,
	int&	ioMonth,
	int&	ioDayOfMonth,
	int&	ioHour,
	int&	ioMinute,
	int&	ioSecond)
{
	TEpochTime	utcTime = GetEpochTimeFromComponents(ioYear, ioMonth, ioDayOfMonth, ioHour, ioMinute, ioSecond);
	int	dow;
	GetComponentsFromEpochTime(UTCToLocal(utcTime), ioYear, ioMonth, ioDayOfMonth, dow, ioHour, ioMinute, ioSecond);
}

bool
CRealTime::InDST(
	TEpochTime	inEpochTime,
	bool			inUTC)
{
	if(inUTC)
	{
		if (GetYearFromEpoch(inEpochTime) != GetYearFromEpoch(dstStartUTC))
		{
			ComputeDSTStartAndEnd(GetYearFromEpoch(inEpochTime));
		}

		if (stdStartUTC > dstStartUTC)
		{
			return inEpochTime >= dstStartUTC && inEpochTime < stdStartUTC;
		}

		return !(inEpochTime >= stdStartUTC && inEpochTime < dstStartUTC);
	}
	else
	{
		if (GetYearFromEpoch(inEpochTime) != GetYearFromEpoch(dstStartLocal))
		{
			ComputeDSTStartAndEnd(GetYearFromEpoch(inEpochTime));
		}

		if (stdStartLocal > dstStartLocal)
		{
			return inEpochTime >= dstStartLocal && inEpochTime < stdStartLocal;
		}

		return !(inEpochTime >= stdStartLocal && inEpochTime < dstStartLocal);
	}

	return false;
}

void
CRealTime::RegisterAlarm(
	char const*	inAlarmName,
	int			inYear,			// 20xx or eAlarm_Any
	int			inMonth,		// 1 to 12 or eAlarm_Any
	int			inDayOfMonth,	// 1 to 31 or eAlarm_Any
	int			inDayOfWeek,	// 1 to 7 or eAlarm_Any
	int			inHour,			// 00 to 23 or eAlarm_Any
	int			inMin,			// 00 to 59 or eAlarm_Any
	int			inSec,			// 00 to 59 or eAlarm_Any
	IRealTimeHandler*	inObject,
	TRealTimeAlarmMethod	inMethod,
	void*				inReference,
	bool				inUTC)
{
	MReturnOnError(strlen(inAlarmName) == 0 || strlen(inAlarmName) >= eRealTime_MaxNameLength);

	SAlarm*	targetAlarm = FindAlarmByName(inAlarmName);

	if(targetAlarm == NULL)
	{
		targetAlarm = FindAlarmFirstEmpty();

		MReturnOnError(targetAlarm == NULL);
	}

	strncpy(targetAlarm->name, inAlarmName, sizeof(targetAlarm->name));

	targetAlarm->year = inYear;
	targetAlarm->month = inMonth;
	targetAlarm->dayOfMonth = inDayOfMonth;
	targetAlarm->dayOfWeek = inDayOfWeek;
	targetAlarm->hour = inHour;
	targetAlarm->minute = inMin;
	targetAlarm->second = inSec;
	targetAlarm->object = inObject;
	targetAlarm->method = inMethod;
	targetAlarm->reference = inReference;
	targetAlarm->utc = inUTC;

	ScheduleAlarm(targetAlarm);
}

void
CRealTime::CancelAlarm(
	char const*	inAlarmName)
{
	SAlarm*	targetAlarm = FindAlarmByName(inAlarmName);
	if(targetAlarm != NULL)
	{
		targetAlarm->name[0] = 0;
	}
}

void
CRealTime::RegisterEvent(
	char const*			inEventName,
	uint64_t			inPeriodUS,
	bool				inOnlyOnce,
	IRealTimeHandler*	inObject,		// The object on which the method below lives
	TRealTimeEventMethod	inMethod,		// The method on the above object
	void*				inReference)	// The reference value passed into the above method
{
	MReturnOnError(strlen(inEventName) == 0 || strlen(inEventName) >= eRealTime_MaxNameLength);

	SEvent*	targetEvent = FindEventByName(inEventName);

	if(targetEvent == NULL)
	{
		targetEvent = FindEventFirstEmpty();

		MReturnOnError(targetEvent == NULL);
	}

	strncpy(targetEvent->name, inEventName, sizeof(targetEvent->name));

	targetEvent->periodUS = inPeriodUS;
	targetEvent->onceOnly = inOnlyOnce;
	targetEvent->object = inObject;
	targetEvent->method = inMethod;
	targetEvent->reference = inReference;
	targetEvent->lastFireTime = gCurLocalUS;
}

void
CRealTime::CancelEvent(
	char const*	inEventName)
{
	SEvent*	targetEvent = FindEventByName(inEventName);
	if(targetEvent != NULL)
	{
		targetEvent->name[0] = 0;
	}
}

void
CRealTime::RegisterTimeChangeHandler(
	char const*				inName,
	IRealTimeHandler*		inObject,
	TRealTimeChangeMethod	inMethod)
{
	MReturnOnError(strlen(inName) == 0 || strlen(inName) >= eRealTime_MaxNameLength);

	STimeChangeHandler*	targetHandler = FindTimeChangeHandlerByName(inName);

	if(targetHandler == NULL)
	{
		targetHandler = FindTimeChangeHandlerFirstEmpty();

		MReturnOnError(targetHandler == NULL);
	}

	strncpy(targetHandler->name, inName, sizeof(targetHandler->name));
	targetHandler->object = inObject;
	targetHandler->method = inMethod;
}

void
CRealTime::CancelTimeChangeHandler(
	char const*	inName)
{
	STimeChangeHandler*	targetHandler = FindTimeChangeHandlerByName(inName);
	if(targetHandler != NULL)
	{
		targetHandler->name[0] = 0;
	}
}

IRealTimeDataProvider*
CRealTime::CreateDS3234Provider(
	uint8_t	inChipSelectPin,
	bool	inUseAltSPI)
{
	static CRealTimeDataProvider_DS3234*	ds3234Provider = NULL;

	if(ds3234Provider == NULL)
	{
		ds3234Provider = new CRealTimeDataProvider_DS3234(inChipSelectPin, inUseAltSPI);
	}

	return ds3234Provider;
}

CRealTime::SAlarm*
CRealTime::FindAlarmByName(
	char const*	inName)
{
	for(int i = 0; i < eAlarm_MaxActive; ++i)
	{
		if(strcmp(inName, alarmArray[i].name) == 0)
		{
			return alarmArray + i;
		}
	}

	return NULL;
}

CRealTime::SAlarm*
CRealTime::FindAlarmFirstEmpty(
	void)
{
	for(int i = 0; i < eAlarm_MaxActive; ++i)
	{
		if(alarmArray[i].name[0] == 0)
		{
			return alarmArray + i;
		}
	}

	return NULL;
}

CRealTime::SEvent*
CRealTime::FindEventByName(
	char const*	inName)
{
	for(int i = 0; i < eEvent_MaxActive; ++i)
	{
		if(strcmp(inName, eventArray[i].name) == 0)
		{
			return eventArray + i;
		}
	}

	return NULL;
}

CRealTime::SEvent*
CRealTime::FindEventFirstEmpty(
	void)
{
	for(int i = 0; i < eEvent_MaxActive; ++i)
	{
		if(eventArray[i].name[0] == 0)
		{
			return eventArray + i;
		}
	}

	return NULL;
}

CRealTime::STimeChangeHandler*
CRealTime::FindTimeChangeHandlerByName(
	char const*	inName)
{
	for(int i = 0; i < eTimeChangeHandler_MaxCount; ++i)
	{
		if(strcmp(inName, timeChangeHandlerArray[i].name) == 0)
		{
			return timeChangeHandlerArray + i;
		}
	}

	return NULL;
}

CRealTime::STimeChangeHandler*
CRealTime::FindTimeChangeHandlerFirstEmpty(
	void)
{
	for(int i = 0; i < eTimeChangeHandler_MaxCount; ++i)
	{
		if(timeChangeHandlerArray[i].name[0] == 0)
		{
			return timeChangeHandlerArray + i;
		}
	}

	return NULL;
}

void
CRealTime::ComputeDSTStartAndEnd(
	int	inYear)
{
	dstStartLocal = ComputeEpochTimeForOffsetSpecifier(timeZoneInfo.dstStart, inYear);
	stdStartLocal = ComputeEpochTimeForOffsetSpecifier(timeZoneInfo.stdStart, inYear);
	dstStartUTC = dstStartLocal - timeZoneInfo.stdStart.offsetMins * 60;
	stdStartUTC = stdStartLocal - timeZoneInfo.dstStart.offsetMins * 60;
}

TEpochTime
CRealTime::ComputeEpochTimeForOffsetSpecifier(
	STimeZoneOffsetSpecifier const&	inSpecifier,
	int								inYear)
{
	TEpochTime	result;

    uint8_t modifiedMonth = inSpecifier.month;
	uint8_t	modifiedWeek = inSpecifier.week;

    if(modifiedWeek == 0)
	{
		// Last week
		++modifiedMonth;
        if(modifiedMonth > 12)
		{
            modifiedMonth = 1;
            ++inYear;
        }
        modifiedWeek = 1;	// Treat as first week then subtract a week at the end
    }

	result = GetEpochTimeFromComponents(inYear, modifiedMonth, 1, inSpecifier.hour, 0, 0);

    result += (7 * (modifiedWeek - 1) + (inSpecifier.dayOfWeek - GetDayOfWeekFromEpoch(result) + 7) % 7) * 60 * 60 * 24;
   
    if (inSpecifier.week == 0)
	{
		result -= 7 * 60 * 60 * 24;
	}

    return result;
}

void
CRealTime::ScheduleAlarm(
	SAlarm*	inAlarm)
{
	// Reschedule it if possible
	int	year = inAlarm->year;
	int	month = inAlarm->month;
	int	day = inAlarm->dayOfMonth;
	int	dow = inAlarm->dayOfWeek;
	int	hour = inAlarm->hour;
	int	min = inAlarm->minute;
	int	sec = inAlarm->second;

	if(GetNextDateTime(year, month, day, dow, hour, min, sec, inAlarm->utc))
	{
		inAlarm->nextTriggerTimeUTC = GetEpochTimeFromComponents(year, month, day, hour, min, sec);
		if(!inAlarm->utc)
		{
			inAlarm->nextTriggerTimeUTC = LocalToUTC(inAlarm->nextTriggerTimeUTC);
		}
		DebugMsg(eDbgLevel_Medium, "%s scheduled for %02d/%02d/%04d %02d:%02d:%02d", inAlarm->name, month, day, year, hour, min, sec);
	}
	else
	{
		DebugMsg(eDbgLevel_Basic, "ScheduleAlarm: %s could not be scheduled", inAlarm->name);
		DebugMsg(eDbgLevel_Medium, "  target was %02d/%02d/%04d %02d:%02d:%02d", inAlarm->month, inAlarm->dayOfMonth, inAlarm->year, inAlarm->hour, inAlarm->minute, inAlarm->second);
		GetComponentsFromEpochTime(GetEpochTime(inAlarm->utc), year, month, day, dow, hour, min, sec);
		DebugMsg(eDbgLevel_Medium, "  now is %02d/%02d/%04d %02d:%02d:%02d", month, day, year, hour, min, sec);
		inAlarm->name[0] = 0;
	}
}

uint8_t
CRealTime::SerialSetTime(
	IOutputDirector*	inOutput,
	int					inArgC,
	char const*			inArgV[])
{
	// year month day hour min sec in UTC

	if(inArgC != 8)
	{
		inOutput->printf("Wrong number of arguments got %d expected 6\n", inArgC - 1);
		return eCmd_Failed;
	}

	int	year = atoi(inArgV[1]);
	if(year < 1970 || year > 2099)
	{
		inOutput->printf("year out of range");
		return eCmd_Failed;
	}

	int	month = atoi(inArgV[2]);
	if(month < 1 || month > 12)
	{
		inOutput->printf("month out of range");
		return eCmd_Failed;
	}

	int	day = atoi(inArgV[3]);
	if(day < 1 || day > 31)
	{
		inOutput->printf("day of month out of range");
		return eCmd_Failed;
	}

	int	hour = atoi(inArgV[4]);
	if(hour < 0 || hour > 23)
	{
		inOutput->printf("hour out of range");
		return eCmd_Failed;
	}

	int	min = atoi(inArgV[5]);
	if(min < 0 || min > 59)
	{
		inOutput->printf("minutes out of range");
		return eCmd_Failed;
	}

	int	sec = atoi(inArgV[6]);
	if(min < 0 || min > 59)
	{
		inOutput->printf("seconds out of range");
		return eCmd_Failed;
	}
	
	bool	utc = strcmp(inArgV[7], "utc") == 0;

	if(!utc)
	{
		LocalToUTC(year, month, day, hour, min, sec);
	}

	if(provider != NULL)
	{
		provider->SetUTCDateAndTime(year, month, day, hour, min, sec);
	}

	SetDateAndTime(year, month, day, hour, min, sec, true);

	return eCmd_Succeeded;
}

uint8_t
CRealTime::SerialGetTime(
	IOutputDirector*	inOutput,
	int					inArgC,
	char const*			inArgV[])
{
	int	year;
	int	month;
	int	day;
	int	hour;
	int	min;
	int	sec;
	int	dow;

	bool	utc = inArgC == 2 && strcmp(inArgV[1], "utc") == 0;

	if(timeMultiplier == 1 && provider != NULL)
	{
		provider->RequestSync();
	}

	GetDateAndTime(year, month, day, dow, hour, min, sec, utc);

	inOutput->printf("%02d/%02d/%04d %02d:%02d:%02d %s\n", month, day, year, hour, min, sec, utc ? "utc" : "local");

	return eCmd_Succeeded;
}

uint8_t
CRealTime::SerialSetTimeZone(
	IOutputDirector*	inOutput,
	int					inArgC,
	char const*			inArgV[])
{
	STimeZoneRule	newTimeZone;
	
	// name dstStartYear dstStartDayOfWeek dstStartMonth dstStartHour dstOffsetMin stdStartYear stdStartDayOfWeek stdStartMonth stdStartHour stdOffsetMin

	if(inArgC != 12)
	{
		inOutput->printf("Wrong number of arguments got %d expected 11\n", inArgC - 1);
		return eCmd_Failed;
	}

	if(strlen(inArgV[1]) > eRealTime_MaxNameLength)
	{
		inOutput->printf("max name length is 15, got %d\n", strlen(inArgV[1]));
		return eCmd_Failed;
	}
	strncpy(newTimeZone.abbrev, inArgV[1], sizeof(newTimeZone.abbrev));
		
	newTimeZone.dstStart.week = atoi(inArgV[2]);
	if(newTimeZone.dstStart.week < 0 || newTimeZone.dstStart.week > 4)
	{
		inOutput->printf("dst start week out of range");
		return eCmd_Failed;
	}

	newTimeZone.dstStart.dayOfWeek = atoi(inArgV[3]);
	if(newTimeZone.dstStart.dayOfWeek < 1 || newTimeZone.dstStart.dayOfWeek > 7)
	{
		inOutput->printf("dst start day of week out of range");
		return eCmd_Failed;
	}

	newTimeZone.dstStart.month = atoi(inArgV[4]);
	if(newTimeZone.dstStart.month < 1 || newTimeZone.dstStart.month > 12)
	{
		inOutput->printf("dst start month out of range");
		return eCmd_Failed;
	}

	newTimeZone.dstStart.hour = atoi(inArgV[5]);
	if(newTimeZone.dstStart.hour < 0 || newTimeZone.dstStart.hour > 23)
	{
		inOutput->printf("dst start hour out of range");
		return eCmd_Failed;
	}

	newTimeZone.dstStart.offsetMins = atoi(inArgV[6]);
	if(newTimeZone.dstStart.offsetMins < -12 * 60 || newTimeZone.dstStart.offsetMins > 14 * 60)
	{
		inOutput->printf("dst start offset out of range");
		return eCmd_Failed;
	}
		
	newTimeZone.stdStart.week = atoi(inArgV[7]);
	if(newTimeZone.stdStart.week < 0 || newTimeZone.stdStart.week > 4)
	{
		inOutput->printf("std start week out of range");
		return eCmd_Failed;
	}

	newTimeZone.stdStart.dayOfWeek = atoi(inArgV[8]);
	if(newTimeZone.stdStart.dayOfWeek < 1 || newTimeZone.stdStart.dayOfWeek > 7)
	{
		inOutput->printf("std start day of week out of range");
		return eCmd_Failed;
	}

	newTimeZone.stdStart.month = atoi(inArgV[9]);
	if(newTimeZone.stdStart.month < 1 || newTimeZone.stdStart.month > 12)
	{
		inOutput->printf("std start month out of range");
		return eCmd_Failed;
	}

	newTimeZone.stdStart.hour = atoi(inArgV[10]);
	if(newTimeZone.stdStart.hour < 0 || newTimeZone.stdStart.hour > 23)
	{
		inOutput->printf("std start hour out of range");
		return eCmd_Failed;
	}

	newTimeZone.stdStart.offsetMins = atoi(inArgV[11]);
	if(newTimeZone.stdStart.offsetMins < -12 * 60 || newTimeZone.stdStart.offsetMins > 14 * 60)
	{
		inOutput->printf("std start offset out of range");
		return eCmd_Failed;
	}

	SetTimeZone(newTimeZone, true);

	return eCmd_Succeeded;
}

uint8_t
CRealTime::SerialGetTimeZone(
	IOutputDirector*	inOutput,
	int					inArgC,
	char const*			inArgV[])
{
	inOutput->printf("%s %d %d %d %d %d %d %d %d %d %d\n", 
		timeZoneInfo.abbrev, 
		timeZoneInfo.dstStart.week, timeZoneInfo.dstStart.dayOfWeek, timeZoneInfo.dstStart.month, timeZoneInfo.dstStart.hour, timeZoneInfo.dstStart.offsetMins, 
		timeZoneInfo.stdStart.week, timeZoneInfo.stdStart.dayOfWeek, timeZoneInfo.stdStart.month, timeZoneInfo.stdStart.hour, timeZoneInfo.stdStart.offsetMins
		);

	return eCmd_Succeeded;
}

uint8_t
CRealTime::SerialDumpTable(
	IOutputDirector*	inOutput,
	int					inArgC,
	char const*			inArgV[])
{
	SAlarm* curAlarm = alarmArray;
	for(int alarmItr = 0; alarmItr < eAlarm_MaxActive; ++alarmItr, ++curAlarm)
	{
		if(curAlarm->name[0] == 0)
		{
			continue;
		}

		int	 year, month, day, dow, hour, min, sec;

		GetComponentsFromEpochTime(curAlarm->nextTriggerTimeUTC, year, month, day, dow, hour, min, sec);

		inOutput->printf("%s will alarm at %02d/%02d/%02d %02d:%02d:%02d UTC", curAlarm->name, month, day, year, hour, min, sec);
	}

	return eCmd_Succeeded;
}

uint8_t
CRealTime::SerialSetMultiplier(
	IOutputDirector*	inOutput,
	int					inArgC,
	char const*			inArgV[])
{
	if(inArgC != 2)
	{
		return false;
	}

	// Reset the internal state variables
	SetEpochTime(GetEpochTime(true), true);

	timeMultiplier = atoi(inArgV[1]);

	return eCmd_Succeeded;
}

enum
{
	eYear,
	eMonth,
	eDay,
	eHour,
	eMin,
	eSec,
	eTimeCompCount,
};

static bool
IncrementComp(
	int*	ioComponents,
	bool*	inAnyComponents,
	int		inStart)
{
	for(int i = inStart; i >= eYear; --i)
	{
		if(inAnyComponents[i])
		{
			++ioComponents[i];

			switch(i)
			{
				case eYear:
					return true;

				case eMonth:
					if(ioComponents[i] <= 12)
					{
						return true;
					}
					ioComponents[i] = 1;
					break;

				case eDay:
				{
					int	daysThisMonth = gDaysInMonth[ioComponents[eMonth] - 1];
					if(ioComponents[eMonth] == 2 && MIsLeapYear(ioComponents[eYear]))
					{
						++daysThisMonth;
					}
					if(ioComponents[i] <= daysThisMonth)
					{
						return true;
					}
					ioComponents[i] = 1;
					break;
				}

				case eHour:
					if(ioComponents[i] < 24)
					{
						return true;
					}
					ioComponents[i] = 0;
					break;

				case eMin:
				case eSec:
					if(ioComponents[i] < 60)
					{
						return true;
					}
					ioComponents[i] = 0;
			}
		}
	}

	return false;
}

bool
CRealTime::GetNextDateTimeFromTime(
	TEpochTime	inTime,
	int&		ioYear,			// xxxx 4 digit year or eAlarm_Any
	int&		ioMonth,		// 1 to 12 or eAlarm_Any
	int&		ioDay,			// 1 to 31 or eAlarm_Any
	int&		ioDayOfWeek,	// 1 to 7 or eAlarm_Any
	int&		ioHour,			// 00 to 23 or eAlarm_Any
	int&		ioMin,			// 00 to 59 or eAlarm_Any
	int&		ioSec)			// 00 to 59 or eAlarm_Any
{
	int		curComponents[eTimeCompCount];
	int		ioComponents[eTimeCompCount] = {ioYear, ioMonth, ioDay, ioHour, ioMin, ioSec};
	bool	anyComponents[eTimeCompCount];
	int		compDOW;

	GetComponentsFromEpochTime(inTime, curComponents[eYear], curComponents[eMonth], curComponents[eDay], compDOW, curComponents[eHour], curComponents[eMin], curComponents[eSec]);
	
	int	i;
	for(i = 0; i < eTimeCompCount; ++i)
	{
		anyComponents[i] = ioComponents[i] == eAlarm_Any;
		if(anyComponents[i])
		{
			ioComponents[i] = curComponents[i];
		}
	}

	for(i = 0; i < eTimeCompCount; ++i)
	{
		if(ioComponents[i] < curComponents[i])
		{
			if(IncrementComp(ioComponents, anyComponents, i - 1) == false)
			{
				return false;
			}

			for(int j = i + 1; j < eTimeCompCount; ++j)
			{
				if(anyComponents[j])
				{
					ioComponents[j] = (j == eMonth || j == eDay) ? 1 : 0;
				}
			}

			break;
		}
		else if(ioComponents[i] > curComponents[i])
		{
			for(int j = i + 1; j < eTimeCompCount; ++j)
			{
				if(anyComponents[j])
				{
					ioComponents[j] = (j == eMonth || j == eDay) ? 1 : 0;
				}
			}

			break;
		}
	}

	compDOW = GetDayOfWeekFromEpoch(GetEpochTimeFromComponents(ioComponents[eYear], ioComponents[eMonth], ioComponents[eDay], 0, 0, 0));
	if(ioDayOfWeek == eAlarm_Any)
	{
		ioDayOfWeek = compDOW;
	}
	else
	{
		while(ioDayOfWeek != compDOW && ioComponents[eYear] <= 2069)
		{
			if(IncrementComp(ioComponents, anyComponents, eDay) == false)
			{
				return false;
			}

			compDOW = GetDayOfWeekFromEpoch(GetEpochTimeFromComponents(ioComponents[eYear], ioComponents[eMonth], ioComponents[eDay], 0, 0, 0));
		}

		if(ioDayOfWeek != compDOW)
		{
			return false;
		}
	}

	ioYear = ioComponents[eYear];
	ioMonth = ioComponents[eMonth];
	ioDay = ioComponents[eDay];
	ioHour = ioComponents[eHour];
	ioMin = ioComponents[eMin];
	ioSec = ioComponents[eSec];

	return true;
}

bool
CRealTime::GetNextDateTime(
	int&	ioYear,
	int&	ioMonth,
	int&	ioDay,
	int&	ioDayOfWeek,
	int&	ioHour,
	int&	ioMin,
	int&	ioSec,
	bool	inUTC)
{
	return GetNextDateTimeFromTime(GetEpochTime(inUTC), ioYear, ioMonth, ioDay, ioDayOfWeek, ioHour, ioMin, ioSec);
}

int
CRealTime::CompareDateTimeWithNow(
	int		inYear,
	int		inMonth,
	int		inDay,
	int		inHour,
	int		inMin,
	int		inSec,
	bool	inUTC)
{
	TEpochTime targetTime = GetEpochTimeFromComponents(inYear, inMonth, inDay, inHour, inMin, inSec);
	TEpochTime curTime = GetEpochTime(inUTC);

	if(targetTime < curTime)
	{
		return -1;
	}

	if(targetTime > curTime)
	{
		return 1;
	}

	return 0;
}
