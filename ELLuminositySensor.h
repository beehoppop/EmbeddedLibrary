#ifndef _FHLUMINOSITYSENSOR_H_
#define _FHLUMINOSITYSENSOR_H_
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

	Get luminosity values from the SparkFun TSL2561 or equivalent

*/

class CModule_LuminositySensor : public CModule, public ISerialCmdHandler
{
public:

	float
	GetActualLux(
		void);

	float
	GetNormalizedBrightness(
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

		float		minBrightnessLux;
		float		maxBrightnessLux;
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
	SerialCmdGetLux(
		int			inArgC,
		char const*	inArgv[]);

	bool
	SerialCmdConfig(
		int			inArgC,
		char const*	inArgv[]);
	
	void
	SetupSensor(
		void);

	bool
	GetLux(
		unsigned char	inGain, 
		unsigned int	inMS, 
		unsigned int	inCH0, 
		unsigned int	inCH1, 
		double&			outLux);

	bool 
	ReadI2CByte(
		uint8_t		inAddress, 
		uint8_t&	outValue);

	bool
	WriteI2CByte(
		uint8_t	inAddress,
		uint8_t	inValue);

	bool
	ReadI2CUInt16(
		uint8_t		inAddress,
		uint16_t&	outValue);

	bool 
	WriteI2CUInt16(
		uint8_t		inAddress, 
		uint16_t	inValue);
		
	unsigned int		integrationTimeMS;
	SLuminositySettings	settings;

	double lux;
	double	normalized;

	char _i2c_address;
	byte _error;

	static CModule_LuminositySensor	module;
};

extern CModule_LuminositySensor*	gLuminositySensor;

#endif /* _FHLUMINOSITYSENSOR_H_ */
