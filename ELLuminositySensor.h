#ifndef _FHLUMINOSITYSENSOR_H_
#define _FHLUMINOSITYSENSOR_H_
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

#if defined(WIN32)
// for testing
	class SFE_TSL2561
	{
	public:
		
		void
		begin(
			void)
		{

		}

		bool
		getData(
			unsigned int&	outData1,
			unsigned int&	outData2)
		{
			return true;
		}

		bool
		getLux(
			int				inGain,
			int				inIntTime,
			unsigned int	inData1,
			unsigned int	inData2,
			double&			outLux)
		{
			return true;
		}

		bool
		getID(
			uint8_t&	outSensorID)
		{
			outSensorID = 0x50;
			return true;
		}

		void
		setTiming(
			int	inGain,
			int	inTime,
			int	inIntegration)
		{

		}

		void
		setPowerUp(
			void)
		{

		}
	};
#else
	#include <SparkFunTSL2561.h>
#endif

class CModule_LuminositySensor : public CModule, public ISerialCmdHandler
{
public:

	float
	GetActualLux(
		void);

	float
	GetNormalizedLux(
		void);

	void
	SetMinMaxLux(
		float	inMinLux,
		float	inMaxLux,
		bool	inUpdateEEPROM);

private:

	struct SLuminositySettings
	{
		uint8_t		gain;
		uint8_t		time;

		double		minBrightnessLux;
		double		maxBrightnessLux;
	};

	CModule_LuminositySensor(
		);

	virtual void
	Setup(
		void);

	virtual void
	Update(
		uint32_t inDeltaTimeUS);

	bool
	SerialConfig(
		int		inArgC,
		char*	inArgv[]);
		
	unsigned int		integrationTimeMS;
	SLuminositySettings	settings;
	SFE_TSL2561			light;

	double lux;
	double	normalized;

	static CModule_LuminositySensor	module;
};

extern CModule_LuminositySensor*	gLuminositySensor;

#endif /* _FHLUMINOSITYSENSOR_H_ */
