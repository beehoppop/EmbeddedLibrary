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

	Get luminosity values from devices that support it.
	
	the SparkFun TSL2561 or equivalent

*/


enum ELuminositySensor
{
	eGain_1X = 0,
	eGain_16X = 1,

	eIntegrationTime_13_7ms = 0,
	eIntegrationTime_101ms = 1,
	eIntegrationTime_402ms = 2,
	eIntegrationTime_Manual = 3,

};

class ILuminosity
{
public:
	
	virtual bool
	IsPresent(
		void) = 0;

	virtual float
	GetActualLux(
		void) = 0;

	virtual float
	GetNormalizedBrightness(
		void) = 0;

	virtual float
	GetNormalizedBrightness(
		float	inLux) = 0;

	virtual void
	SetMinMaxLux(
		float	inMinLux,
		float	inMaxLux) = 0;

	virtual void
	GetActualRGB(
		float&	outR,
		float&	outG,
		float&	outB) = 0;

	virtual void
	GetNormalizedRGB(
		float&	outR,
		float&	outG,
		float&	outB) = 0;

	virtual void
	SetMinMaxRGB(
		float	inMinR,
		float	inMaxR,
		float	inMinG,
		float	inMaxG,
		float	inMinB,
		float	inMaxB) = 0;

};

class CTSL2561Sensor : public ILuminosity
{
public:

	CTSL2561Sensor(
		uint8_t	inAddress,
		uint8_t	inGain,
		uint8_t	inTime);

	virtual bool
	IsPresent(
		void);

	virtual float
	GetActualLux(
		void);

	virtual float
	GetNormalizedBrightness(
		void);

	virtual float
	GetNormalizedBrightness(
		float inLux);

	virtual void
	SetMinMaxLux(
		float	inMinLux,
		float	inMaxLux);

	virtual void
	GetActualRGB(
		float&	outR,
		float&	outG,
		float&	outB);

	virtual void
	GetNormalizedRGB(
		float&	outR,
		float&	outG,
		float&	outB);

	virtual void
	SetMinMaxRGB(
		float	inMinR,
		float	inMaxR,
		float	inMinG,
		float	inMaxG,
		float	inMinB,
		float	inMaxB);

private:
	
	void
	SetupSensor(
		void);

	bool
	GetLux(
		unsigned char	inGain, 
		unsigned int	inMS, 
		unsigned int	inCH0, 
		unsigned int	inCH1, 
		float&			outLux);

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
		
	unsigned int	integrationTimeMS;

	float		minBrightnessLux;
	float		maxBrightnessLux;

	uint8_t		gain;
	uint8_t		time;

	char i2cAddress;
	byte error;
	bool	isPresent;
};

#endif /* _FHLUMINOSITYSENSOR_H_ */
