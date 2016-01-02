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
#include <ELSerial.h>
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
		int	inChipSelect)
	{
		chipselect = inChipSelect;

		pinMode(chipselect, OUTPUT);

		SPI.begin();
		SPI.setBitOrder(MSBFIRST); 
		SPI.setDataMode(SPI_MODE1); 

		//set control register 
		digitalWrite(chipselect, LOW);  
		SPI.transfer(0x8E);
		SPI.transfer(0x60); //60= disable Osciallator and Battery SQ wave @1hz, temp compensation, Alarms disabled
		digitalWrite(chipselect, HIGH);
		
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
			return;
		}

		int TimeDate [7] = {inSec, inMin, inHour, 0, inDayOfMonth, inMonth, inYear};

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
		  
			digitalWrite(chipselect, LOW);
			SPI.transfer(i + 0x80); 
			SPI.transfer(TimeDate[i]);        
			digitalWrite(chipselect, HIGH);
		}
	}

	virtual void
	RequestSync(
		void)
	{
		int TimeDate [7]; //second,minute,hour,null,day,month,year

		for(int i = 0; i < 7; ++i)
		{
			if(i == 3)
				continue;

			digitalWrite(chipselect, LOW);
			SPI.transfer(i + 0x00); 
			unsigned int n = SPI.transfer(0x00);        
			digitalWrite(chipselect, HIGH);

			int a = n & B00001111;
			int b = (n & B01110000) >> 4;

			TimeDate[i] = a + b * 10;	

			if(i == 5 && (TimeDate[i] & 0x80))
			{
				// No actual need to do anything with the century bit since the year number dictates 1900 or 2000
				TimeDate[i] &= 0x1F;
			}
		}

		if(TimeDate[6] >= 70 && TimeDate[6] <= 99)
		{
			TimeDate[6] += 1900;
		}
		else
		{
			TimeDate[6] += 2000;
		}

		gRealTime->SetDateAndTime(TimeDate[6], TimeDate[5], TimeDate[4], TimeDate[2], TimeDate[1], TimeDate[0], true);
	}

	int chipselect;
};
#endif

CRealTime::CRealTime(
	)
	:
	CModule(
		"rtc ",					// The module ID
		sizeof(STimeZoneRule),	// eeprom size
		1,						// eeprom version number
		1000000,				// Call the update method once a second
		1)
{
	gRealTime = this;
}

void
CRealTime::Setup(
	void)
{
	LoadDataFromEEPROM(&timeZoneInfo, eepromOffset, sizeof(timeZoneInfo));

	if((uint8_t)timeZoneInfo.abbrev[0] == 0xFF)
	{
		// no time zone is set so just zero it out
		memset(&timeZoneInfo, 0, sizeof(timeZoneInfo));
	}

	#if defined(WIN32)
		// For testing
		SetTimeZone(gTimeZone, false);
	#endif

	gSerialCmd->RegisterCommand("set_time", this, static_cast<TSerialCmdMethod>(&CRealTime::SerialSetTime));
	gSerialCmd->RegisterCommand("get_time", this, static_cast<TSerialCmdMethod>(&CRealTime::SerialGetTime));
	gSerialCmd->RegisterCommand("set_timezone", this, static_cast<TSerialCmdMethod>(&CRealTime::SerialSetTimeZone));
	gSerialCmd->RegisterCommand("get_timezone", this, static_cast<TSerialCmdMethod>(&CRealTime::SerialGetTimeZone));
	gSerialCmd->RegisterCommand("realtime_dump", this, static_cast<TSerialCmdMethod>(&CRealTime::SerialDumpTable));
	gSerialCmd->RegisterCommand("set_time_mult", this, static_cast<TSerialCmdMethod>(&CRealTime::SerialSetMultiplier));

	timeMultiplier = 1.0f;
}

void
CRealTime::Update(
	uint32_t	inDeltaTimeUS)
{
	if(provider != NULL && (gCurLocalMS - localMSAtLastSet) / 1000 >= providerSyncPeriod && timeMultiplier == 1.0f)
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
			bool	reschedule = (curAlarm->object->*curAlarm->method)(curAlarm->name, curAlarm->reference);
			if(reschedule)
			{
				ScheduleAlarm(curAlarm);
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
		WriteDataToEEPROM(&timeZoneInfo, eepromOffset, sizeof(timeZoneInfo));
	}

	RecomputeAlarms();
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
	localMSAtLastSet = gCurLocalMS;
	
	if(inUTC)
	{
		epocUTCTimeAtLastSet = inEpochTime;
	}
	else
	{
		epocUTCTimeAtLastSet = LocalToUTC(inEpochTime);
	}

	RecomputeAlarms();
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

IRealTimeDataProvider*
CRealTime::CreateDS3234Provider(
	uint8_t	inChipSelectPin)
{
	static CRealTimeDataProvider_DS3234*	ds3234Provider = NULL;

	if(ds3234Provider == NULL)
	{
		ds3234Provider = new CRealTimeDataProvider_DS3234(inChipSelectPin);
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
CRealTime::RecomputeAlarms(
	void)
{
	DebugMsg(eDbgLevel_Medium, "Recomputing alarms\n");

	SAlarm* curAlarm = alarmArray;
	for(int alarmItr = 0; alarmItr < eAlarm_MaxActive; ++alarmItr, ++curAlarm)
	{
		if(curAlarm->name[0] == 0)
		{
			continue;
		}

		ScheduleAlarm(curAlarm);
	}
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
		DebugMsg(eDbgLevel_Basic, "%s could not be scheduled", inAlarm->name);
		inAlarm->name[0] = 0;
	}
}

bool
CRealTime::SerialSetTime(
	int			inArgC,
	char const*	inArgv[])
{
	// year month day hour min sec in UTC

	if(inArgC != 8)
	{
		Serial.printf("Wrong number of arguments got %d expected 6\n", inArgC - 1);
		return false;
	}

	int	year = atoi(inArgv[1]);
	if(year < 1970 || year > 2099)
	{
		Serial.printf("year out of range");
		return false;
	}

	int	month = atoi(inArgv[2]);
	if(month < 1 || month > 12)
	{
		Serial.printf("month out of range");
		return false;
	}

	int	day = atoi(inArgv[3]);
	if(day < 1 || day > 31)
	{
		Serial.printf("day of month out of range");
		return false;
	}

	int	hour = atoi(inArgv[4]);
	if(hour < 0 || hour > 23)
	{
		Serial.printf("hour out of range");
		return false;
	}

	int	min = atoi(inArgv[5]);
	if(min < 0 || min > 59)
	{
		Serial.printf("minutes out of range");
		return false;
	}

	int	sec = atoi(inArgv[6]);
	if(min < 0 || min > 59)
	{
		Serial.printf("seconds out of range");
		return false;
	}
	
	bool	utc = strcmp(inArgv[7], "utc") == 0;

	if(provider != NULL)
	{
		provider->SetUTCDateAndTime(year, month, day, hour, min, sec);
	}

	SetDateAndTime(year, month, day, hour, min, sec, utc);

	return true;
}

bool
CRealTime::SerialGetTime(
	int			inArgC,
	char const*	inArgv[])
{
	int	year;
	int	month;
	int	day;
	int	hour;
	int	min;
	int	sec;
	int	dow;

	GetDateAndTime(year, month, day, dow, hour, min, sec, false);

	Serial.printf("%02d/%02d/%04d %02d:%02d:%02d local\n", month, day, year, hour, min, sec);

	return true;
}

bool
CRealTime::SerialSetTimeZone(
	int			inArgC,
	char const*	inArgv[])
{
	STimeZoneRule	newTimeZone;
	
	// name dstStartYear dstStartDayOfWeek dstStartMonth dstStartHour dstOffsetMin stdStartYear stdStartDayOfWeek stdStartMonth stdStartHour stdOffsetMin

	if(inArgC != 12)
	{
		Serial.printf("Wrong number of arguments got %d expected 11\n", inArgC - 1);
		return false;
	}

	if(strlen(inArgv[1]) > eRealTime_MaxNameLength)
	{
		Serial.printf("max name length is 15, got %d\n", strlen(inArgv[1]));
		return false;
	}
	strncpy(newTimeZone.abbrev, inArgv[1], sizeof(newTimeZone.abbrev));
		
	newTimeZone.dstStart.week = atoi(inArgv[2]);
	if(newTimeZone.dstStart.week < 0 || newTimeZone.dstStart.week > 4)
	{
		Serial.printf("dst start week out of range");
		return false;
	}

	newTimeZone.dstStart.dayOfWeek = atoi(inArgv[3]);
	if(newTimeZone.dstStart.dayOfWeek < 1 || newTimeZone.dstStart.dayOfWeek > 7)
	{
		Serial.printf("dst start day of week out of range");
		return false;
	}

	newTimeZone.dstStart.month = atoi(inArgv[4]);
	if(newTimeZone.dstStart.month < 1 || newTimeZone.dstStart.month > 12)
	{
		Serial.printf("dst start month out of range");
		return false;
	}

	newTimeZone.dstStart.hour = atoi(inArgv[5]);
	if(newTimeZone.dstStart.hour < 0 || newTimeZone.dstStart.hour > 23)
	{
		Serial.printf("dst start hour out of range");
		return false;
	}

	newTimeZone.dstStart.offsetMins = atoi(inArgv[6]);
	if(newTimeZone.dstStart.offsetMins < -12 * 60 || newTimeZone.dstStart.offsetMins > 14 * 60)
	{
		Serial.printf("dst start offset out of range");
		return false;
	}
		
	newTimeZone.stdStart.week = atoi(inArgv[7]);
	if(newTimeZone.stdStart.week < 0 || newTimeZone.stdStart.week > 4)
	{
		Serial.printf("std start week out of range");
		return false;
	}

	newTimeZone.stdStart.dayOfWeek = atoi(inArgv[8]);
	if(newTimeZone.stdStart.dayOfWeek < 1 || newTimeZone.stdStart.dayOfWeek > 7)
	{
		Serial.printf("std start day of week out of range");
		return false;
	}

	newTimeZone.stdStart.month = atoi(inArgv[9]);
	if(newTimeZone.stdStart.month < 1 || newTimeZone.stdStart.month > 12)
	{
		Serial.printf("std start month out of range");
		return false;
	}

	newTimeZone.stdStart.hour = atoi(inArgv[10]);
	if(newTimeZone.stdStart.hour < 0 || newTimeZone.stdStart.hour > 23)
	{
		Serial.printf("std start hour out of range");
		return false;
	}

	newTimeZone.stdStart.offsetMins = atoi(inArgv[11]);
	if(newTimeZone.stdStart.offsetMins < -12 * 60 || newTimeZone.stdStart.offsetMins > 14 * 60)
	{
		Serial.printf("std start offset out of range");
		return false;
	}

	SetTimeZone(newTimeZone, true);

	return true;
}

bool
CRealTime::SerialGetTimeZone(
	int			inArgC,
	char const*	inArgv[])
{
	Serial.printf("%s %d %d %d %d %d %d %d %d %d %d\n", 
		timeZoneInfo.abbrev, 
		timeZoneInfo.dstStart.week, timeZoneInfo.dstStart.dayOfWeek, timeZoneInfo.dstStart.month, timeZoneInfo.dstStart.hour, timeZoneInfo.dstStart.offsetMins, 
		timeZoneInfo.stdStart.week, timeZoneInfo.stdStart.dayOfWeek, timeZoneInfo.stdStart.month, timeZoneInfo.stdStart.hour, timeZoneInfo.stdStart.offsetMins
		);

	return true;
}

bool
CRealTime::SerialDumpTable(
	int			inArgC,
	char const*	inArgv[])
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

		DebugMsg(eDbgLevel_Basic, "%s will alarm at %02d/%02d/%02d %02d:%02d:%02d UTC", curAlarm->name, month, day, year, hour, min, sec);
	}

	return true;
}

bool
CRealTime::SerialSetMultiplier(
	int			inArgC,
	char const*	inArgv[])
{
	if(inArgC != 2)
	{
		return false;
	}

	timeMultiplier = atoi(inArgv[1]);

	return true;
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
					if(ioComponents[i] < daysThisMonth)
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
	int		curComponents[eTimeCompCount];
	int		ioComponents[eTimeCompCount] = {ioYear, ioMonth, ioDay, ioHour, ioMin, ioSec};
	bool	anyComponents[eTimeCompCount];
	int		compDOW;

	GetComponentsFromEpochTime(GetEpochTime(inUTC), curComponents[eYear], curComponents[eMonth], curComponents[eDay], compDOW, curComponents[eHour], curComponents[eMin], curComponents[eSec]);
	
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

	if(i == eTimeCompCount && ioComponents[eSec] == curComponents[eSec])
	{
		// ensure we return a time in the future and not now
		if(IncrementComp(ioComponents, anyComponents, eSec) == false)
		{
			return false;
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
