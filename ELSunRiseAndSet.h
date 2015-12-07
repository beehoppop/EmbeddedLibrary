#ifndef _ELSUNRISEANDSET_H_
#define _ELSUNRISEANDSET_H_

const double	cSunOffset_SunsetSunRise		= -35.0/60.0;	// Use eSunRelativePosition_UpperLimb
const double	cSunOffset_CivilTwilight		= -6.0;			// Use eSunRelativePosition_Center
const double	cSunOffset_NauticalTwilight		= -12.0;		// Use eSunRelativePosition_Center
const double	cSunOffset_AstronomicalTwilight	= -18.0;		// Use eSunRelativePosition_Center

enum ESunRelativePosition
{
	eSunRelativePosition_Center,
	eSunRelativePosition_UpperLimb,
};

/***************************************************************************/
/* Note: year,month,date = calendar date, 1801-2099 only.             */
/*       Eastern longitude positive, Western longitude negative       */
/*       Northern latitude positive, Southern latitude negative       */
/*       The longitude value IS critical in this function!            */
/*       inAltit = the altitude which the Sun should cross            */
/*               Set to -35/60 degrees for rise/set, -6 degrees       */
/*               for civil, -12 degrees for nautical and -18          */
/*               degrees for astronomical twilight.                   */
/*         inUpperLimb: non-zero -> upper limb, zero -> center        */
/*               Set to non-zero (e.g. 1) when computing rise/set     */
/*               times, and to zero when computing start/end of       */
/*               twilight.                                            */
/*        outSunriseTime = where to store the rise time               */
/*        outSunsetTime  = where to store the set  time               */
/*                Both times are relative to the specified altitude,  */
/*                and thus this function can be used to compute       */
/*                various twilight times, as well as rise/set times   */
/* Return value:  0 = sun rises/sets this day, times stored at        */
/*                    *trise and *tset.                               */
/*               +1 = sun above the specified "horizon" 24 hours.     */
/*                    *trise set to time when the sun is at south,    */
/*                    minus 12 hours while *tset is set to the south  */
/*                    time plus 12 hours. "Day" length = 24 hours     */
/*               -1 = sun is below the specified "horizon" 24 hours   */
/*                    "Day" length = 0 hours, *trise and *tset are    */
/*                    both set to the time when the sun is at south.  */
/*                                                                    */
/**********************************************************************/
int
GetSunRiseAndSetHour(
	double&	outSunrise,
	double&	outSunset,
	int		inYear,
	int		inMonth,
	int		inDay,
	double	inLongitude,
	double	inLatitude,
	double	inSunOffset = cSunOffset_SunsetSunRise,
	int		inSunRelativePosition = eSunRelativePosition_UpperLimb);

double
GetSunriseHour(
	int		inYear,
	int		inMonth,
	int		inDay,
	double	inLongitude,
	double	inLatitude,
	double	inSunOffset = cSunOffset_SunsetSunRise,
	int		inSunRelativePosition = eSunRelativePosition_UpperLimb);

double
GetSunsetHour(
	int		inYear,
	int		inMonth,
	int		inDay,
	double	inLongitude,
	double	inLatitude,
	double	inSunOffset = cSunOffset_SunsetSunRise,
	int		inSunRelativePosition = eSunRelativePosition_UpperLimb);

/**********************************************************************/
/* Note: year,month,date = calendar date, 1801-2099 only.             */
/*       Eastern longitude positive, Western longitude negative       */
/*       Northern latitude positive, Southern latitude negative       */
/*       The longitude value is not critical. Set it to the correct   */
/*       longitude if you're picky, otherwise set to to, say, 0.0     */
/*       The latitude however IS critical - be sure to get it correct */
/*       inAltit = the altitude which the Sun should cross              */
/*               Set to -35/60 degrees for rise/set, -6 degrees       */
/*               for civil, -12 degrees for nautical and -18          */
/*               degrees for astronomical twilight.                   */
/*         inUpperLimb: non-zero -> upper limb, zero -> center         */
/*               Set to non-zero (e.g. 1) when computing day length   */
/*               and to zero when computing day+twilight length.      */
/**********************************************************************/
double
GetDayLength(
	int		inYear,
	int		inMonth,
	int		inDay,
	double	inLongitude,
	double	inLatitude,
	double	inSunOffset = cSunOffset_SunsetSunRise,
	int		inSunRelativePosition = eSunRelativePosition_UpperLimb);

#endif /* _ELSUNRISEANDSET_H_ */
