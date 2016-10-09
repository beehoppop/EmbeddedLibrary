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

	SFE_TSL2561 illumination sensor library for Arduino
	Mike Grusin, SparkFun Electronics
	
	This library provides functions to access the TAOS TSL2561
	Illumination Sensor.
	
	Our example code uses the "beerware" license. You can do anything
	you like with this code. No really, anything. If you find it useful,
	buy me a beer someday.

	version 1.0 2013/09/20 MDG initial version
	Updated to Arduino 1.6.4 5/2015

	Luminsoity sensor code included from Sparkfun's TSL2561 library to avoid requiring
	the user to download and install that library on their own.
*/
#include <EL.h>
#include <ELModule.h>
#include <ELUtilities.h>
#include <ELCommand.h>
#include <ELLuminositySensor.h>
#include <ELAssert.h>

#define TSL2561_ADDR_0 0x29 // address with '0' shorted on board
#define TSL2561_ADDR   0x39 // default address
#define TSL2561_ADDR_1 0x49 // address with '1' shorted on board

// TSL2561 registers

#define TSL2561_CMD           0x80
#define TSL2561_CMD_CLEAR     0xC0
#define	TSL2561_REG_CONTROL   0x00
#define	TSL2561_REG_TIMING    0x01
#define	TSL2561_REG_THRESH_L  0x02
#define	TSL2561_REG_THRESH_H  0x04
#define	TSL2561_REG_INTCTL    0x06
#define	TSL2561_REG_ID        0x0A
#define	TSL2561_REG_DATA_0    0x0C
#define	TSL2561_REG_DATA_1    0x0E

CTSL2561Sensor::CTSL2561Sensor(
	uint8_t	inAddress,
	uint8_t	inGain,
	uint8_t	inTime)
	:
	integrationTimeMS(0),
	gain(inGain),
	time(inTime),
	i2cAddress(inAddress),
	error(0)
{
	isPresent = false;
	Wire.begin();
		
	uint8_t	sensorID;
	bool	success;

	success = ReadI2CByte(TSL2561_REG_ID, sensorID);
	if(success == false || sensorID != 0x50)
	{
		return;
	}

	SetupSensor();
}
	
void
CTSL2561Sensor::SetupSensor(
	void)
{
	switch (time)
	{
		case 0: integrationTimeMS = 14; break;
		case 1: integrationTimeMS = 101; break;
		case 2: integrationTimeMS = 402; break;
		default: integrationTimeMS = 0;
	}

	// Turn off sensing to config
	WriteI2CByte(TSL2561_REG_CONTROL,0x00);

	// Get timing byte
	unsigned char timing;
	if(ReadI2CByte(TSL2561_REG_TIMING, timing))
	{
		// Set gain (0 or 1)
		if (gain)
			timing |= 0x10;
		else
			timing &= ~0x10;

		// Set integration time (0 to 3)
		timing &= ~0x03;
		timing |= (time & 0x03);

		// Write modified timing byte back to device
		WriteI2CByte(TSL2561_REG_TIMING,timing);
	}

	// Write 0x03 to command byte (power on)
	WriteI2CByte(TSL2561_REG_CONTROL,0x03);

	isPresent = true;
}

bool
CTSL2561Sensor::GetLux(
	unsigned char	inGain, 
	unsigned int	inMS, 
	unsigned int	inCH0, 
	unsigned int	inCH1, 
	float&			outLux)
{
	// Convert raw data to lux
	// gain: 0 (1X) or 1 (16X), see setTiming()
	// ms: integration time in ms, from setTiming() or from manual integration
	// CH0, CH1: results from getData()
	// lux will be set to resulting lux calculation
	// returns true (1) if calculation was successful
	// RETURNS false (0) AND lux = 0.0 IF EITHER SENSOR WAS SATURATED (0XFFFF)

	float ratio, d0, d1;

	// Determine if either sensor saturated (0xFFFF)
	// If so, abandon ship (calculation will not be accurate)
	if ((inCH0 == 0xFFFF) || (inCH1 == 0xFFFF))
	{
		outLux = 0.0f;
		return false;
	}

	// Convert from unsigned integer to floating point
	d0 = (float)inCH0; 
	d1 = (float)inCH1;

	// We will need the ratio for subsequent calculations
	ratio = d1 / d0;

	// Normalize for integration time
	d0 *= 402.0f / (float)inMS;
	d1 *= 402.0f / (float)inMS;

	// Normalize for gain
	if (!inGain)
	{
		d0 *= 16;
		d1 *= 16;
	}

	// Determine lux per datasheet equations:
	
	if (ratio < 0.5)
	{
		outLux = 0.0304f * d0 - 0.062f * d0 * (float)pow(ratio, 1.4f);
		return true;
	}

	if (ratio < 0.61f)
	{
		outLux = 0.0224f * d0 - 0.031f * d1;
		return true;
	}

	if (ratio < 0.80)
	{
		outLux = 0.0128f * d0 - 0.0153f * d1;
		return true;
	}

	if (ratio < 1.30)
	{
		outLux = 0.00146f * d0 - 0.00112f * d1;
		return true;
	}

	outLux = 0.0f;

	return true;
}

bool
CTSL2561Sensor::IsPresent(
	void)
{
	return isPresent;
}


float
CTSL2561Sensor::GetActualLux(
	void)
{
	uint16_t data0, data1;

	ReadI2CUInt16(TSL2561_REG_DATA_0, data0);
	ReadI2CUInt16(TSL2561_REG_DATA_1,data1);

	float	lux;

	GetLux(gain, integrationTimeMS, data0, data1, lux);

	return lux;
}

float
CTSL2561Sensor::GetNormalizedBrightness(
	void)
{
	float lux = GetActualLux();

	if(lux < minBrightnessLux) lux = minBrightnessLux;
	if(lux > maxBrightnessLux) lux = maxBrightnessLux;
	
	return float((lux - minBrightnessLux) / (maxBrightnessLux - minBrightnessLux));
}

float
CTSL2561Sensor::GetNormalizedBrightness(
	float	inLux)
{
	if(inLux < minBrightnessLux) inLux = minBrightnessLux;
	if(inLux > maxBrightnessLux) inLux = maxBrightnessLux;
	
	return float((inLux - minBrightnessLux) / (maxBrightnessLux - minBrightnessLux));
}

void
CTSL2561Sensor::SetMinMaxLux(
	float	inMinLux,
	float	inMaxLux)
{
	minBrightnessLux = inMinLux;
	maxBrightnessLux = inMaxLux;
}

void
CTSL2561Sensor::GetActualRGB(
	float&	outR,
	float&	outG,
	float&	outB)
{

}

void
CTSL2561Sensor::GetNormalizedRGB(
	float&	outR,
	float&	outG,
	float&	outB)
{

}

void
CTSL2561Sensor::SetMinMaxRGB(
	float	inMinR,
	float	inMaxR,
	float	inMinG,
	float	inMaxG,
	float	inMinB,
	float	inMaxB)
{

}

bool 
CTSL2561Sensor::ReadI2CByte(
	uint8_t		inAddress, 
	uint8_t&	outValue)
{
	// Reads a byte from a TSL2561 address
	// Address: TSL2561 address (0 to 15)
	// Value will be set to stored byte
	// Returns true (1) if successful, false (0) if there was an I2C error
	// (Also see getError() above)

	// Set up command byte for read
	Wire.beginTransmission(i2cAddress);
	Wire.write((inAddress & 0x0F) | TSL2561_CMD);
	error = Wire.endTransmission();

	// Read requested byte
	if (error == 0)
	{
		Wire.requestFrom(i2cAddress, 1);
		if (Wire.available() == 1)
		{
			outValue = Wire.read();
			return true;
		}
	}

	return false;
}

bool
CTSL2561Sensor::WriteI2CByte(
	uint8_t	inAddress,
	uint8_t	inValue)
{
	// Write a byte to a TSL2561 address
	// Address: TSL2561 address (0 to 15)
	// Value: byte to write to address
	// Returns true (1) if successful, false (0) if there was an I2C error
	// (Also see getError() above)

	// Set up command byte for write
	Wire.beginTransmission(i2cAddress);
	Wire.write((inAddress & 0x0F) | TSL2561_CMD);
	// Write byte
	Wire.write(inValue);
	error = Wire.endTransmission();

	return error == 0;
}

bool
CTSL2561Sensor::ReadI2CUInt16(
	uint8_t		inAddress,
	uint16_t&	outValue)
{
	// Reads an unsigned integer (16 bits) from a TSL2561 address (low byte first)
	// Address: TSL2561 address (0 to 15), low byte first
	// Value will be set to stored unsigned integer
	// Returns true (1) if successful, false (0) if there was an I2C error
	// (Also see getError() above)

	char high, low;
	
	// Set up command byte for read
	Wire.beginTransmission(i2cAddress);
	Wire.write((inAddress & 0x0F) | TSL2561_CMD);
	error = Wire.endTransmission();

	// Read two bytes (low and high)
	if (error == 0)
	{
		Wire.requestFrom(i2cAddress, 2);
		if (Wire.available() == 2)
		{
			low = Wire.read();
			high = Wire.read();
			// Combine bytes into unsigned int
			outValue = word(high,low);
			return true;
		}
	}	

	return false;
}

bool 
CTSL2561Sensor::WriteI2CUInt16(
	uint8_t		inAddress, 
	uint16_t	inValue)
{
	// Write an unsigned integer (16 bits) to a TSL2561 address (low byte first)
	// Address: TSL2561 address (0 to 15), low byte first
	// Value: unsigned int to write to address
	// Returns true (1) if successful, false (0) if there was an I2C error
	// (Also see getError() above)
	// Split int into lower and upper bytes, write each byte
	if (WriteI2CByte(inAddress, lowByte(inValue)) 
		&& WriteI2CByte(inAddress + 1, highByte(inValue)))
	{
		return true;
	}

	return false;
}
