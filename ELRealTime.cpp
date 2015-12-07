
#include <Time.h>

#include <ELModule.h>
#include <ELUtilities.h>
#include <ELSerial.h>
#include <ELAssert.h>

#include "ELRealTime.h"

int			gDaysInMonth[12] = {31,28,31,30,31,30,31,31,30,31,30,31};

#define MIsLeapYear(inYear) (((inYear) & 3) == 0 && (((inYear) % 25) != 0 || ((inYear) & 15) == 0))

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
		int			inYear,			// 20xx
		int			inMonth,		// 1 to 12
		int			inDayOfMonth,	// 1 to 31
		int			inHour,			// 00 to 23
		int			inMin,			// 00 to 59
		int			inSec)
	{
		int TimeDate [7] = {inSec, inMin, inHour, 0, inDayOfMonth, inMonth, inYear};

		for(int i = 0; i < 7; ++i)
		{
			if(i == 3)
				continue;

			int b = TimeDate[i] / 10;
			int a = TimeDate[i] % 10;

			TimeDate[i] = a + (b << 4);
		  
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
		}

		gRealTime.SetDateAndTime(TimeDate[6], TimeDate[5], TimeDate[4], TimeDate[2], TimeDate[1], TimeDate[0], true);
	}

	int chipselect;
};

CRealTime::CRealTime(
	)	// If provider is NULL the user is responsible for calling SetDateAndTime and setting an alarm to periodically sync as needed
	:
	CModule(
		"rtc ",		// The module ID
		0,			// No eeprom size
		0,			// No eeprom version number
		1000000)	// Call the update method once a second
{
}

void
CRealTime::Update(
	uint32_t	inDeltaTimeUS)
{
	if(provider != NULL && gCurLocalMS - epocUTCTimeAtLastSet >= provierSyncPeriodMS)
	{
		provider->RequestSync();
	}

	// Do not fire any alarms off on the first update (or until curEpochTime is at least 1)
	if(lastUTCEpochTimeAlarmCheck == 0)
	{
		lastUTCEpochTimeAlarmCheck = GetEpochTime(true);
		return;
	}

	TELEpochTime	startLocalEpochTime = UTCToLocal(lastUTCEpochTimeAlarmCheck);
	TELEpochTime	endLocalEpochTime = GetEpochTime();

	bool	alarmTrigger[eAlarm_MaxActive];
	memset(alarmTrigger, 0, sizeof(alarmTrigger));

	// Compute which alarms need to fire off
	int	curYear = 0;
	int	curMonth = 0;
	int	curDayOfMonth = 0;
	int	curDayOfWeek = 0;
	int	curHour = 0;
	int	curMin = 0;
	int	curSec = 0;

	// Start with local time zone alarms
	GetComponentsFromEpochTime(
		startLocalEpochTime,
		curYear,
		curMonth,
		curDayOfMonth,
		curDayOfWeek,
		curHour,
		curMin,
		curSec);

	// We have to run through all of the seconds since the last update to make sure no alarms are missed
	for(TELEpochTime epocTimeItr = startLocalEpochTime; epocTimeItr <= endLocalEpochTime; ++epocTimeItr)
	{
		SAlarm* curAlarm = alarmArray;
		for(int alarmItr = 0; alarmItr < eAlarm_MaxActive; ++alarmItr, ++curAlarm)
		{
			if(curAlarm->name[0] == 0 || alarmTrigger[alarmItr] || curAlarm->utc)
			{
				continue;
			}

			// Check to see if its time to fire this alarm off
			if((curAlarm->year == eAlarm_Any || curAlarm->year == curYear)
				&& (curAlarm->month == eAlarm_Any || curAlarm->month == curMonth)
				&& (curAlarm->dayOfMonth == eAlarm_Any || curAlarm->dayOfMonth == curDayOfMonth)
				&& (curAlarm->dayOfWeek == eAlarm_Any || curAlarm->dayOfWeek == curDayOfWeek)
				&& (curAlarm->hour == eAlarm_Any || curAlarm->hour == curHour)
				&& (curAlarm->minute == eAlarm_Any || curAlarm->minute == curMin)
				&& (curAlarm->second == eAlarm_Any || curAlarm->second == curSec)
				)
			{
				alarmTrigger[alarmItr] = true;
			}
		}

		// Update the time
		++curSec;
		if(curSec >= 60)
		{
			curSec = 0;
			++curMin;

			if(curMin >= 60)
			{
				curMin = 0;
				++curHour;

				if(curHour >= 24)
				{
					curHour = 0;
					++curDayOfMonth;
					++curDayOfWeek;

					if(curDayOfWeek >= 8)
					{
						curDayOfWeek = 1;
					}

					int	daysInThisMonth = gDaysInMonth[curMonth - 1];
					if(curMonth == 2 && MIsLeapYear(curYear))
					{
						// leap year
						++daysInThisMonth;
					}

					if(curDayOfMonth > daysInThisMonth)
					{
						curDayOfMonth = 1;
						++curMonth;

						if(curMonth > 12)
						{
							curMonth = 1;
							++curYear;
						}
					}
				}
			}
		}
	}

	// Now run through all the UTC timers
	TELEpochTime	startUTCEpochTime = lastUTCEpochTimeAlarmCheck;
	TELEpochTime	endUTCEpochTime = GetEpochTime(true);

	GetComponentsFromEpochTime(
		startUTCEpochTime,
		curYear,
		curMonth,
		curDayOfMonth,
		curDayOfWeek,
		curHour,
		curMin,
		curSec);

	for(TELEpochTime epocTimeItr = startUTCEpochTime; epocTimeItr <= endUTCEpochTime; ++epocTimeItr)
	{
		SAlarm* curAlarm = alarmArray;
		for(int alarmItr = 0; alarmItr < eAlarm_MaxActive; ++alarmItr, ++curAlarm)
		{
			if(curAlarm->name[0] == 0 || alarmTrigger[alarmItr] || !curAlarm->utc)
			{
				continue;
			}

			// Check to see if its time to fire this alarm off
			if((curAlarm->year == eAlarm_Any || curAlarm->year == curYear)
				&& (curAlarm->month == eAlarm_Any || curAlarm->month == curMonth)
				&& (curAlarm->dayOfMonth == eAlarm_Any || curAlarm->dayOfMonth == curDayOfMonth)
				&& (curAlarm->dayOfWeek == eAlarm_Any || curAlarm->dayOfWeek == curDayOfWeek)
				&& (curAlarm->hour == eAlarm_Any || curAlarm->hour == curHour)
				&& (curAlarm->minute == eAlarm_Any || curAlarm->minute == curMin)
				&& (curAlarm->second == eAlarm_Any || curAlarm->second == curSec)
				)
			{
				alarmTrigger[alarmItr] = true;
			}
		}

		// Update the time
		++curSec;
		if(curSec >= 60)
		{
			curSec = 0;
			++curMin;

			if(curMin >= 60)
			{
				curMin = 0;
				++curHour;

				if(curHour >= 24)
				{
					curHour = 0;
					++curDayOfMonth;
					++curDayOfWeek;

					if(curDayOfWeek >= 8)
					{
						curDayOfWeek = 1;
					}

					int	daysInThisMonth = gDaysInMonth[curMonth - 1];
					if(curMonth == 2 && MIsLeapYear(curYear))
					{
						// leap year
						++daysInThisMonth;
					}

					if(curDayOfMonth > daysInThisMonth)
					{
						curDayOfMonth = 1;
						++curMonth;

						if(curMonth > 12)
						{
							curMonth = 1;
							++curYear;
						}
					}
				}
			}
		}
	}

	// Now call each alarm method
	SAlarm* curAlarm = alarmArray;
	for(int itr = 0; itr < eAlarm_MaxActive; ++itr, ++curAlarm)
	{
		if(alarmTrigger[itr])
		{
			(curAlarm->object->*curAlarm->method)(curAlarm->name, curAlarm->reference);
		}
	}

	lastUTCEpochTimeAlarmCheck = endUTCEpochTime + 1;

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
	STimeZoneRule&	inTimeZone)
{
	timeZoneInfo = inTimeZone;
}

void
CRealTime::SetProvider(
	IRealTimeDataProvider*	inProvider,				// If provider is NULL the user is responsible for calling SetDateAndTime and setting an alarm to periodically sync as needed
	uint32_t				inProviderSyncPeriod)	// In seconds, the period between refreshing the time with the given provider
{
	provider = inProvider;
	provierSyncPeriodMS = inProviderSyncPeriod * 1000;

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
	SetEpochTime(GetEpochTimeFromComponents(inYear, inMonth, inDayOfMonth, inHour, inMin, inSec), inUTC);
}

void 
CRealTime::SetEpochTime(
	TELEpochTime	inEpochTime,
	bool			inUTC)
{
	localMSAtLastSet = millis();

	if(inUTC)
	{
		epocUTCTimeAtLastSet = inEpochTime;
	}
	else
	{
		epocUTCTimeAtLastSet = LocalToUTC(inEpochTime);
	}

	lastUTCEpochTimeAlarmCheck = epocUTCTimeAtLastSet;
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

TELEpochTime 
CRealTime::GetEpochTime(
	bool	inUTC)
{
	TELEpochTime	result = epocUTCTimeAtLastSet + (millis() - localMSAtLastSet) / 1000;

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
	TELEpochTime	inEpochTime)
{
	TELEpochTime	daysEpochTime = inEpochTime / (60 * 60 * 24);
	int				year = 1970;
	TELEpochTime	days = 0;

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
	TELEpochTime	inEpochTime)
{
	TELEpochTime	daysEpochTime = inEpochTime / (60 * 60 * 24);
	int				year = 1970;
	TELEpochTime	days = 0;
	int				month;
	TELEpochTime	monthLength;

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
	TELEpochTime	inEpochTime)
{
	TELEpochTime	daysEpochTime = inEpochTime / (60 * 60 * 24);
	int				year = 1970;
	TELEpochTime	days = 0;
	int				month;
	TELEpochTime	monthLength;

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
	TELEpochTime	inEpochTime)
{
	TELEpochTime	daysEpochTime = inEpochTime / (60 * 60 * 24);

	return ((daysEpochTime + 4) % 7) + 1;  // Jan 1, 1970 was a Thursday (ie day 4 starting from 0 of the week);
}

int
CRealTime::GetHourFromEpoch(
	TELEpochTime	inEpochTime)
{
	return (inEpochTime / (60 * 60)) % 24;
}

int
CRealTime::GetMinuteFromEpoch(
	TELEpochTime	inEpochTime)
{
	return (inEpochTime / 60) % 60;
}

int
CRealTime::GetSecondFromEpoch(
	TELEpochTime	inEpochTime)
{
	return inEpochTime % 60;
}

TELEpochTime
CRealTime::GetEpochTimeFromComponents(
	int inYear,
	int inMonth, 
	int inDayOfMonth, 
	int inHour,
	int inMinute, 
	int inSecond)
{
	TELEpochTime result;

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
	TELEpochTime	inEpocTime,
	int&			outYear,		// 20xx
	int&			outMonth,		// 1 to 12
	int&			outDayOfMonth,	// 1 to 31
	int&			outDayOfWeek,	// 1 to 7
	int&			outHour,		// 00 to 23
	int&			outMinute,		// 00 to 59
	int&			outSecond)		// 00 to 59
{
	int				year;
	int				month;
	TELEpochTime	monthLength;
	TELEpochTime	remainingTime;
	TELEpochTime	days;

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

TELEpochTime
CRealTime::LocalToUTC(
	TELEpochTime	inLocalEpochTime)
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

TELEpochTime
CRealTime::UTCToLocal(
	TELEpochTime	inUTCEpochTime)
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
	TELEpochTime	inEpochTime,
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
	TRealTimeMethod	inMethod,
	void*					inReference,
	bool					inUTC)
{
	MReturnOnError(strlen(inAlarmName) >= eRealTime_MaxNameLength);

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
}

void
CRealTime::CancelAlarm(
	char const*	inAlarmName)
{
	SAlarm*	targetAlarm = FindAlarmByName(inAlarmName);
	MReturnOnError(targetAlarm == NULL);
	targetAlarm->name[0] = 0;
}

void
CRealTime::RegisterEvent(
	char const*			inEventName,
	uint64_t			inPeriodUS,
	bool				inOnlyOnce,
	IRealTimeHandler*	inObject,		// The object on which the method below lives
	TRealTimeMethod		inMethod,		// The method on the above object
	void*				inReference)	// The reference value passed into the above method
{
	MReturnOnError(strlen(inEventName) >= eRealTime_MaxNameLength);

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
	MReturnOnError(targetEvent == NULL);
	targetEvent->name[0] = 0;
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

TELEpochTime
CRealTime::ComputeEpochTimeForOffsetSpecifier(
	STimeZoneOffsetSpecifier const&	inSpecifier,
	int								inYear)
{
	TELEpochTime	result;

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

CRealTime	gRealTime;
