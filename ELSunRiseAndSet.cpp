/*
	Author: Brent Pease

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

	SUNRISET.C - computes Sun rise/set times, start/end of twilight, and
				 the length of the day at any date and latitude

	Written as DAYLEN.C, 1989-08-16

	Modified to SUNRISET.C, 1992-12-01

	(c) Paul Schlyter, 1989, 1992

	Released to the public domain by Paul Schlyter, December 1992

	Ported to EmbeddedLibrary and generally restructured by Brent Pease 2015

*/

#include <stdio.h>
#include <math.h>

#include "ELUtilities.h"
#include "ELSunRiseAndSet.h"
#include "ELAssert.h"


/* A macro to compute the number of days elapsed since 2000 Jan 0.0 */
/* (which is equal to 1999 Dec 31, 0h UT)                           */

#define days_since_2000_Jan_0(y,m,d) (367L*(y)-((7*((y)+(((m)+9)/12)))/4)+((275*(m))/9)+(d)-730530L)

/* Some conversion factors between radians and degrees */

#ifndef PI
 #define PI        3.1415926535897932384
#endif

#define RADEG     ( 180.0 / PI )
#define DEGRAD    ( PI / 180.0 )

/* The trigonometric functions in degrees */

#define sind(x)  sin((x)*DEGRAD)
#define cosd(x)  cos((x)*DEGRAD)
#define tand(x)  tan((x)*DEGRAD)

#define atand(x)    (RADEG*atan(x))
#define asind(x)    (RADEG*asin(x))
#define acosd(x)    (RADEG*acos(x))
#define atan2d(y,x) (RADEG*atan2(y,x))

#define INV360    ( 1.0 / 360.0 )

CSunRiseAndSetModule	CSunRiseAndSetModule::module;
CSunRiseAndSetModule*	gSunRiseAndSet;

CSunRiseAndSetModule::CSunRiseAndSetModule(
	)
	:
	CModule("SRAS", sizeof(double) * 2, 1)
{
	gSunRiseAndSet = this;
}
	
void
CSunRiseAndSetModule::Setup(
	void)
{
	LoadDataFromEEPROM(&lon, eepromOffset, sizeof(double));
	LoadDataFromEEPROM(&lat, eepromOffset + sizeof(double), sizeof(double));

	gSerialCmd->RegisterCommand("set_lonlat", this, static_cast<TSerialCmdMethod>(&CSunRiseAndSetModule::SerialSetLonLat));
	gSerialCmd->RegisterCommand("get_lonlat", this, static_cast<TSerialCmdMethod>(&CSunRiseAndSetModule::SerialGetLonLat));
}

void
CSunRiseAndSetModule::SetLongitudeAndLatitude(
	double	inLongitude,
	double	inLatitude,
	bool	inSaveInEEPROM)
{
	lon = inLongitude;
	lat = inLatitude;
	if(inSaveInEEPROM)
	{
		WriteDataToEEPROM(&lon, eepromOffset, sizeof(double));
		WriteDataToEEPROM(&lat, eepromOffset + sizeof(double), sizeof(double));
	}
}

void
CSunRiseAndSetModule::GetLongitudeAndLatitude(
	double&	outLongitude,
	double&	outLatitude)
{
	outLongitude = lon;
	outLatitude = lat;
}

void
CSunRiseAndSetModule::RegisterSunriseEvent(
	char const*					inEventName,
	int							inYear,			// The specific year for the event or eAlarm_Any
	int							inMonth,		// The specific month for the event or eAlarm_Any
	int							inDay,			// The specific day for the event or eAlarm_Any
	int							inDOW,			// The specific day of week or eAlarm_Any
	ISunRiseAndSetEventHandler*	inCmdHandler,
	TSunRiseAndSetEventMethod	inMethod,
	double						inSunOffset,
	int							inSunRelativePosition,
	bool						inUTC)
{
	MReturnOnError(strlen(inEventName) == 0 || strlen(inEventName) > eRealTime_MaxNameLength);

	SEvent*	newEvent = FindEvent(inEventName);
	if(newEvent == NULL)
	{
		newEvent = FindFirstFreeEvent();
		MReturnOnError(newEvent == NULL);
	}

	strcpy(newEvent->name, inEventName);
	newEvent->year = inYear;
	newEvent->month = inMonth;
	newEvent->day = inDay;
	newEvent->dow = inDOW;
	newEvent->cmdHandler = inCmdHandler;
	newEvent->method = inMethod;
	newEvent->sunOffset = inSunOffset;
	newEvent->sunRelativePosition = inSunRelativePosition;
	newEvent->utc = inUTC;
	newEvent->sunRise = true;

	ScheduleNextEvent(newEvent);
}

void
CSunRiseAndSetModule::RegisterSunsetEvent(
	char const*					inEventName,
	int							inYear,			// The specific year for the event or eAlarm_Any
	int							inMonth,		// The specific month for the event or eAlarm_Any
	int							inDay,			// The specific day for the event or eAlarm_Any
	int							inDOW,			// The specific day of week or eAlarm_Any
	ISunRiseAndSetEventHandler*	inCmdHandler,
	TSunRiseAndSetEventMethod	inMethod,
	double						inSunOffset,
	int							inSunRelativePosition,
	bool						inUTC)
{
	MReturnOnError(strlen(inEventName) == 0 || strlen(inEventName) > eRealTime_MaxNameLength);

	SEvent*	newEvent = FindEvent(inEventName);
	if(newEvent == NULL)
	{
		newEvent = FindFirstFreeEvent();
		MReturnOnError(newEvent == NULL);
	}

	strcpy(newEvent->name, inEventName);
	newEvent->year = inYear;
	newEvent->month = inMonth;
	newEvent->day = inDay;
	newEvent->dow = inDOW;
	newEvent->cmdHandler = inCmdHandler;
	newEvent->method = inMethod;
	newEvent->sunOffset = inSunOffset;
	newEvent->sunRelativePosition = inSunRelativePosition;
	newEvent->utc = inUTC;
	newEvent->sunRise = false;

	ScheduleNextEvent(newEvent);
}

void
CSunRiseAndSetModule::CancelEvent(
	char const*	inEventName)
{
	SEvent*	targetEvent = FindEvent(inEventName);
	if(targetEvent == NULL)
	{
		return;
	}

	gRealTime->CancelAlarm(inEventName);
	targetEvent->name[0] = 0;
}

bool
CSunRiseAndSetModule::SerialSetLonLat(
	int		inArgC,
	char*	inArgv[])
{
	if(inArgC != 3)
	{
		return false;
	}

	double lon = atof(inArgv[1]);
	double lat = atof(inArgv[2]);

	SetLongitudeAndLatitude(lon, lat, true);

	return true;
}

bool
CSunRiseAndSetModule::SerialGetLonLat(
	int		inArgC,
	char*	inArgv[])
{
	Serial.printf("%03.03f %03.03f\n", lon, lat);
	return true;
}

CSunRiseAndSetModule::SEvent*
CSunRiseAndSetModule::FindEvent(
	char const*	inName)
{
	for(int itr = 0; itr < eMaxSunRiseSetEvents; ++itr)
	{
		if(strcmp(eventList[itr].name, inName) == 0)
		{
			return eventList + itr;
		}
	}

	return NULL;
}

CSunRiseAndSetModule::SEvent*
CSunRiseAndSetModule::FindFirstFreeEvent(
	void)
{
	for(int itr = 0; itr < eMaxSunRiseSetEvents; ++itr)
	{
		if(eventList[itr].name[0] == 0)
		{
			return eventList + itr;
		}
	}

	return NULL;
}

void
CSunRiseAndSetModule::ScheduleNextEvent(
	SEvent*	inEvent)
{
	// Get the target utc date
	int	targetYear = inEvent->year;
	int	targetMonth = inEvent->month;
	int	targetDay = inEvent->day;
	int	targetDOW = inEvent->dow;
	int	targetHour = eAlarm_Any;
	int	targetMin = eAlarm_Any;
	int	targetSec = eAlarm_Any;

	if(gRealTime->GetNextDateTime(targetYear, targetMonth, targetDay, targetDOW, targetHour, targetMin, targetSec, inEvent->utc) == false)
	{
		// there is not a next time so just don't schedule
		return;
	}

	if(!inEvent->utc)
	{
		// need to convert this to UTC
		TEpochTime	epochTime = gRealTime->GetEpochTimeFromComponents(targetYear, targetMonth, targetDay, targetHour, targetMin, targetSec);
		epochTime = gRealTime->LocalToUTC(epochTime);
		gRealTime->GetComponentsFromEpochTime(epochTime, targetYear, targetMonth, targetDay, targetDOW, targetHour, targetMin, targetSec);
	}

	// Get the event hour for the target date
	double		eventHour;
	if(inEvent->sunRise)
	{
		eventHour = GetSunriseHour(targetYear, targetMonth, targetDay, lon, lat, inEvent->sunOffset, inEvent->sunRelativePosition);
	}
	else
	{
		eventHour = GetSunsetHour(targetYear, targetMonth, targetDay, lon, lat, inEvent->sunOffset, inEvent->sunRelativePosition);
	}

	// Get the event epoch time by taking the target date at midnight utc and adding the event hour
	TEpochTime	utcEpochEventTime = gRealTime->GetEpochTimeFromComponents(targetYear, targetMonth, targetDay, 0, 0, 0) + (TEpochTime)(eventHour * 60.0 * 60.0);

	gRealTime->GetComponentsFromEpochTime(utcEpochEventTime, targetYear, targetMonth, targetDay, targetDOW, targetHour, targetMin, targetSec);

	// if this event was in the past and any component is eAlarm_Any then reschedule given the computed hour, min, sec of the event
	if(utcEpochEventTime < gRealTime->GetEpochTime(true))
	{
		if(inEvent->year == eAlarm_Any || inEvent->month == eAlarm_Any || inEvent->day == eAlarm_Any || inEvent->dow == eAlarm_Any)
		{
			targetYear = inEvent->year;
			targetMonth = inEvent->month;
			targetDay = inEvent->day;
			targetDOW = inEvent->dow;

			// Get the next date and time given the computed hour, min, sec of the event
			if(gRealTime->GetNextDateTime(targetYear, targetMonth, targetDay, targetDOW, targetHour, targetMin, targetSec, true) == false)
			{
				// only in extreme corner cases should this fail...
				return;
			}

			// Since we will have a new date we need to compute the event again
			if(inEvent->sunRise)
			{
				eventHour = GetSunriseHour(targetYear, targetMonth, targetDay, lon, lat, inEvent->sunOffset, inEvent->sunRelativePosition);
			}
			else
			{
				eventHour = GetSunsetHour(targetYear, targetMonth, targetDay, lon, lat, inEvent->sunOffset, inEvent->sunRelativePosition);
			}

			utcEpochEventTime = gRealTime->GetEpochTimeFromComponents(targetYear, targetMonth, targetDay, 0, 0, 0) + (TEpochTime)(eventHour * 60.0 * 60.0);
			gRealTime->GetComponentsFromEpochTime(utcEpochEventTime, targetYear, targetMonth, targetDay, targetDOW, targetHour, targetMin, targetSec);
		}
		else
		{
			// don't try to schedule something in the past
			return;
		}
	}

	// Set the alarm to fire
	gRealTime->RegisterAlarm(
		inEvent->name,
		targetYear,
		targetMonth,
		targetDay,
		eAlarm_Any,
		targetHour,
		targetMin,
		targetSec,
		this,
		static_cast<TRealTimeMethod>(&CSunRiseAndSetModule::RealTimeAlarmHandler),
		inEvent,
		true);
}

void
CSunRiseAndSetModule::RealTimeAlarmHandler(
	char const*	inName,
	void*		inReference)
{
	SEvent*	targetEvent = (SEvent*)inReference;

	((targetEvent->cmdHandler)->*(targetEvent->method))(targetEvent->name);

	 ScheduleNextEvent(targetEvent);
}

/******************************************************************/
/* This function reduces any angle to within the first revolution */
/* by subtracting or adding even multiples of 360.0 until the     */
/* result is >= 0.0 and < 360.0                                   */
/******************************************************************/

/*****************************************/
/* Reduce angle to within 0..360 degrees */
/*****************************************/
static double
Revolution(
	double	inX)
{
	return (inX - 360.0 * floor(inX * INV360));
}

/*********************************************/
/* Reduce angle to within +180..+180 degrees */
/*********************************************/
static double
Rev180(
	double inX)
{
      return (inX - 360.0 * floor(inX * INV360 + 0.5));
}

/*******************************************************************/
/* This function computes GMST0, the Greenwich Mean Sidereal Time  */
/* at 0h UT (i.e. the sidereal time at the Greenwhich meridian at  */
/* 0h UT).  GMST is then the sidereal time at Greenwich at any     */
/* time of the day.  I've generalized GMST0 as well, and define it */
/* as:  GMST0 = GMST - UT  --  this allows GMST0 to be computed at */
/* other times than 0h UT as well.  While this sounds somewhat     */
/* contradictory, it is very practical:  instead of computing      */
/* GMST like:                                                      */
/*                                                                 */
/*  GMST = (GMST0) + UT * (366.2422/365.2422)                      */
/*                                                                 */
/* where (GMST0) is the GMST last time UT was 0 hours, one simply  */
/* computes:                                                       */
/*                                                                 */
/*  GMST = GMST0 + UT                                              */
/*                                                                 */
/* where GMST0 is the GMST "at 0h UT" but at the current moment!   */
/* Defined in this way, GMST0 will increase with about 4 min a     */
/* day.  It also happens that GMST0 (in degrees, 1 hr = 15 degr)   */
/* is equal to the Sun's mean longitude plus/minus 180 degrees!    */
/* (if we neglect aberration, which amounts to 20 seconds of arc   */
/* or 1.33 seconds of time)                                        */
/*                                                                 */
/*******************************************************************/

static double
GMST0(
	double inD )
{
      double sidtim0;

      /* Sidtime at 0h UT = L (Sun's mean longitude) + 180.0 degr  */
      /* L = M + w, as defined in sunpos().  Since I'm too lazy to */
      /* add these numbers, I'll let the C compiler do it for me.  */
      /* Any decent C compiler will add the constants at compile   */
      /* time, imposing no runtime or code overhead.               */
      sidtim0 = Revolution( ( 180.0 + 356.0470 + 282.9404 ) +
                          ( 0.9856002585 + 4.70935E-5 ) * inD );
      
	  return sidtim0;
}


/* This function computes the Sun's position at any instant */
/******************************************************/
/* Computes the Sun's ecliptic longitude and distance */
/* at an instant given in d, number of days since     */
/* 2000 Jan 0.0.  The Sun's ecliptic latitude is not  */
/* computed, since it's always very near 0.           */
/******************************************************/
static void
SunPos(
	double	inDay, 
	double*	outLon, 
	double*	outRadius)
{
      double M,         /* Mean anomaly of the Sun */
             w,         /* Mean longitude of perihelion */
                        /* Note: Sun's mean longitude = M + w */
             e,         /* Eccentricity of Earth's orbit */
             E,         /* Eccentric anomaly */
             x, y,      /* x, y coordinates in orbit */
             v;         /* True anomaly */

      /* Compute mean elements */
      M = Revolution( 356.0470 + 0.9856002585 * inDay );
      w = 282.9404 + 4.70935E-5 * inDay;
      e = 0.016709 - 1.151E-9 * inDay;

      /* Compute true longitude and radius vector */
      E = M + e * RADEG * sind(M) * ( 1.0 + e * cosd(M) );
            x = cosd(E) - e;
      y = sqrt( 1.0 - e*e ) * sind(E);
      *outRadius = sqrt( x*x + y*y );              /* Solar distance */
      v = atan2d( y, x );                  /* True anomaly */
      *outLon = v + w;                        /* True solar longitude */
      if ( *outLon >= 360.0 )
            *outLon -= 360.0;                   /* Make it 0..360 degrees */
}

/******************************************************/
/* Computes the Sun's equatorial coordinates RA, Decl */
/* and also its distance, at an instant given in d,   */
/* the number of days since 2000 Jan 0.0.             */
/******************************************************/
static void 
SunRADec(
	double	inDay,
	double*	outRA,
	double*	outDec,
	double*	outRadius)
{
      double lon, obl_ecl, x, y, z;

      /* Compute Sun's elliptical coordinates */
      SunPos( inDay, &lon, outRadius );

      /* Compute ecliptic rectangular coordinates (z=0) */
      x = *outRadius * cosd(lon);
      y = *outRadius * sind(lon);

      /* Compute obliquity of ecliptic (inclination of Earth's axis) */
      obl_ecl = 23.4393 - 3.563E-7 * inDay;

      /* Convert to equatorial rectangular coordinates - x is unchanged */
      z = y * sind(obl_ecl);
      y = y * cosd(obl_ecl);

      /* Convert to spherical coordinates */
      *outRA = atan2d( y, x );
      *outDec = atan2d( z, sqrt(x*x + y*y) );

}

double 
CSunRiseAndSetModule::GetDayLength(
	int		inYear, 
	int		inMonth, 
	int		inDay, 
	double	inLon, 
	double	inLat,
	double	inAltit, 
	int		inUpperLimb)
{
	double  d,  /* Days since 2000 Jan 0.0 (negative before) */
	obl_ecl,    /* Obliquity (inclination) of Earth's axis */
	sr,         /* Solar distance, astronomical units */
	slon,       /* True solar longitude */
	sin_sdecl,  /* Sine of Sun's declination */
	cos_sdecl,  /* Cosine of Sun's declination */
	sradius,    /* Sun's apparent radius */
	t;          /* Diurnal arc */

	/* Compute d of 12h local mean solar time */
	d = days_since_2000_Jan_0(inYear,inMonth,inDay) + 0.5 - inLon/360.0;

	/* Compute obliquity of ecliptic (inclination of Earth's axis) */
	obl_ecl = 23.4393 - 3.563E-7 * d;

	/* Compute Sun's ecliptic longitude and distance */
	SunPos( d, &slon, &sr );

	/* Compute sine and cosine of Sun's declination */
	sin_sdecl = sind(obl_ecl) * sind(slon);
	cos_sdecl = sqrt( 1.0 - sin_sdecl * sin_sdecl );

	/* Compute the Sun's apparent radius, degrees */
	sradius = 0.2666 / sr;

	/* Do correction to upper limb, if necessary */
	if ( inUpperLimb )
		inAltit -= sradius;

	/* Compute the diurnal arc that the Sun traverses to reach */
	/* the specified altitude altit: */
	{
		double cost;
		cost = ( sind(inAltit) - sind(inLat) * sin_sdecl ) /
				( cosd(inLat) * cos_sdecl );
		if ( cost >= 1.0 )
				t = 0.0;                      /* Sun always below altit */
		else if ( cost <= -1.0 )
				t = 24.0;                     /* Sun always above altit */
		else  t = (2.0/15.0) * acosd(cost); /* The diurnal arc, hours */
	}

	return t;
}

int 
CSunRiseAndSetModule::GetSunRiseAndSetHour(
	double&	outSunriseTime,
	double&	outSunsetTime,
	int		inYear, 
	int		inMonth, 
	int		inDay, 
	double	inLon, 
	double	inLat,
    double	inAltit,
	int		inUpperLimb)
{
      double  d,  /* Days since 2000 Jan 0.0 (negative before) */
      sr,         /* Solar distance, astronomical units */
      sRA,        /* Sun's Right Ascension */
      sdec,       /* Sun's declination */
      sradius,    /* Sun's apparent radius */
      t,          /* Diurnal arc */
      tsouth,     /* Time when Sun is at south */
      sidtime;    /* Local sidereal time */

      int rc = 0; /* Return cde from function - usually 0 */

      /* Compute d of 12h local mean solar time */
      d = days_since_2000_Jan_0(inYear,inMonth,inDay) + 0.5 - inLon/360.0;

      /* Compute the local sidereal time of this moment */
      sidtime = Revolution( GMST0(d) + 180.0 + inLon );

      /* Compute Sun's RA, Decl and distance at this moment */
      SunRADec( d, &sRA, &sdec, &sr );

      /* Compute time when Sun is at south - in hours UT */
      tsouth = 12.0 - Rev180(sidtime - sRA)/15.0;

      /* Compute the Sun's apparent radius in degrees */
      sradius = 0.2666 / sr;

      /* Do correction to upper limb, if necessary */
      if ( inUpperLimb )
            inAltit -= sradius;

      /* Compute the diurnal arc that the Sun traverses to reach */
      /* the specified altitude altit: */
      {
            double cost;
            cost = ( sind(inAltit) - sind(inLat) * sind(sdec) ) /
                  ( cosd(inLat) * cosd(sdec) );
            if ( cost >= 1.0 )
                  rc = -1, t = 0.0;       /* Sun always below altit */
            else if ( cost <= -1.0 )
                  rc = +1, t = 12.0;      /* Sun always above altit */
            else
                  t = acosd(cost)/15.0;   /* The diurnal arc, hours */
      }

      /* Store rise and set times - in hours UT */
      outSunriseTime = tsouth - t;
      outSunsetTime  = tsouth + t;

      return rc;
}

double
CSunRiseAndSetModule::GetSunriseHour(
	int		inYear,
	int		inMonth,
	int		inDay,
	double	inLon,
	double	inLat,
	double	inAltit,
	int		inUpperLimb)
{
	double  d,  /* Days since 2000 Jan 0.0 (negative before) */
	sr,         /* Solar distance, astronomical units */
	sRA,        /* Sun's Right Ascension */
	sdec,       /* Sun's declination */
	sradius,    /* Sun's apparent radius */
	t,          /* Diurnal arc */
	tsouth,     /* Time when Sun is at south */
	sidtime;    /* Local sidereal time */

	/* Compute d of 12h local mean solar time */
	d = days_since_2000_Jan_0(inYear,inMonth,inDay) + 0.5 - inLon/360.0;

	/* Compute the local sidereal time of this moment */
	sidtime = Revolution( GMST0(d) + 180.0 + inLon );

	/* Compute Sun's RA, Decl and distance at this moment */
	SunRADec( d, &sRA, &sdec, &sr );

	/* Compute time when Sun is at south - in hours UT */
	tsouth = 12.0 - Rev180(sidtime - sRA)/15.0;

	/* Compute the Sun's apparent radius in degrees */
	sradius = 0.2666 / sr;

	/* Do correction to upper limb, if necessary */
	if ( inUpperLimb )
		inAltit -= sradius;

	/* Compute the diurnal arc that the Sun traverses to reach */
	/* the specified altitude altit: */
	{
		double cost;
		cost = ( sind(inAltit) - sind(inLat) * sind(sdec) ) /
				( cosd(inLat) * cosd(sdec) );
		if ( cost >= 1.0 )
				t = 0.0;       /* Sun always below altit */
		else if ( cost <= -1.0 )
				t = 12.0;      /* Sun always above altit */
		else
				t = acosd(cost)/15.0;   /* The diurnal arc, hours */
	}

	/* Store rise and set times - in hours UT */
	return tsouth - t;
}

double
CSunRiseAndSetModule::GetSunsetHour(
	int		inYear,
	int		inMonth,
	int		inDay,
	double	inLon,
	double	inLat,
	double	inAltit,
	int		inUpperLimb)
{
	double  d,  /* Days since 2000 Jan 0.0 (negative before) */
	sr,         /* Solar distance, astronomical units */
	sRA,        /* Sun's Right Ascension */
	sdec,       /* Sun's declination */
	sradius,    /* Sun's apparent radius */
	t,          /* Diurnal arc */
	tsouth,     /* Time when Sun is at south */
	sidtime;    /* Local sidereal time */

	/* Compute d of 12h local mean solar time */
	d = days_since_2000_Jan_0(inYear,inMonth,inDay) + 0.5 - inLon/360.0;

	/* Compute the local sidereal time of this moment */
	sidtime = Revolution( GMST0(d) + 180.0 + inLon );

	/* Compute Sun's RA, Decl and distance at this moment */
	SunRADec( d, &sRA, &sdec, &sr );

	/* Compute time when Sun is at south - in hours UT */
	tsouth = 12.0 - Rev180(sidtime - sRA)/15.0;

	/* Compute the Sun's apparent radius in degrees */
	sradius = 0.2666 / sr;

	/* Do correction to upper limb, if necessary */
	if ( inUpperLimb )
		inAltit -= sradius;

	/* Compute the diurnal arc that the Sun traverses to reach */
	/* the specified altitude altit: */
	{
		double cost;
		cost = ( sind(inAltit) - sind(inLat) * sind(sdec) ) /
				( cosd(inLat) * cosd(sdec) );
		if ( cost >= 1.0 )
				t = 0.0;       /* Sun always below altit */
		else if ( cost <= -1.0 )
				t = 12.0;      /* Sun always above altit */
		else
				t = acosd(cost)/15.0;   /* The diurnal arc, hours */
	}

	/* Store rise and set times - in hours UT */
	return tsouth + t;
}
