
#include "ELSunRiseAndSet.h"

/* +++Date last modified: 05-Jul-1997 */
/* Updated comments, 05-Aug-2013 */

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
GetDayLength(
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
GetSunRiseAndSetHour(
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
GetSunriseHour(
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
GetSunsetHour(
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
