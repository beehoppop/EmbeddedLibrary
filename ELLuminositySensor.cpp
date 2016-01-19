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

enum ELuminositySensor
{
	eGain_1X = 0,
	eGain_16X = 1,

	eIntegrationTime_13_7ms = 0,
	eIntegrationTime_101ms = 1,
	eIntegrationTime_402ms = 2,
	eIntegrationTime_Manual = 3,

};

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

CModule_LuminositySensor	CModule_LuminositySensor::module;
CModule_LuminositySensor*	gLuminositySensor;

CModule_LuminositySensor::CModule_LuminositySensor(
	)
	:
	CModule(
		"lumn",
		sizeof(SLuminositySettings),
		1,
		&settings,
		500000,
		1,
		false)
{
	gLuminositySensor = this;
}

float
CModule_LuminositySensor::GetActualLux(
	void)
{
	return (float)lux;
}

float
CModule_LuminositySensor::GetNormalizedBrightness(
	void)
{
	return (float)normalized;
}

void
CModule_LuminositySensor::SetMinMaxLux(
	float	inMinLux,
	float	inMaxLux,
	bool	inUpdateEEPROM)
{
	settings.minBrightnessLux = inMinLux;
	settings.maxBrightnessLux = inMaxLux;
	if(inUpdateEEPROM)
	{
		EEPROMSave();
	}
}

uint8_t
CModule_LuminositySensor::SerialCmdGetLux(
	IOutputDirector*	inOutput,
	int					inArgC,
	char const*			inArgV[])
{
	inOutput->printf("lux = %f\n", GetActualLux());

	return eCmd_Succeeded;
}

uint8_t
CModule_LuminositySensor::SerialCmdConfig(
	IOutputDirector*	inOutput,
	int					inArgC,
	char const*			inArgV[])
{
	settings.gain = atoi(inArgV[1]);
	settings.time = atoi(inArgV[2]);
	EEPROMSave();

	SetupSensor();

	return eCmd_Succeeded;
}
	
void
CModule_LuminositySensor::SetupSensor(
	void)
{
	switch (settings.time)
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
		if (settings.gain)
			timing |= 0x10;
		else
			timing &= ~0x10;

		// Set integration time (0 to 3)
		timing &= ~0x03;
		timing |= (settings.time & 0x03);

		// Write modified timing byte back to device
		WriteI2CByte(TSL2561_REG_TIMING,timing);
	}

	// Write 0x03 to command byte (power on)
	WriteI2CByte(TSL2561_REG_CONTROL,0x03);
}

void
CModule_LuminositySensor::Setup(
	void)
{
	_i2c_address = TSL2561_ADDR; // just assume default address for now
	Wire.begin();
		
	uint8_t	sensorID;
	bool	success;

	success = ReadI2CByte(TSL2561_REG_ID, sensorID);
	if(success == false || sensorID != 0x50)
	{
		SetEnabledState(false);
		return;
	}
		
	gCommand->RegisterCommand("lumin_get_lux", this, static_cast<TCmdHandlerMethod>(&CModule_LuminositySensor::SerialCmdGetLux));
	gCommand->RegisterCommand("lumin_config", this, static_cast<TCmdHandlerMethod>(&CModule_LuminositySensor::SerialCmdConfig));

	SetupSensor();
}

void
CModule_LuminositySensor::Update(
	uint32_t inDeltaTimeUS)
{

	uint16_t data0, data1;

	ReadI2CUInt16(TSL2561_REG_DATA_0, data0);
	ReadI2CUInt16(TSL2561_REG_DATA_1,data1);

	bool	good;

	good = GetLux(settings.gain, integrationTimeMS, data0, data1, lux);
	if(good)
	{
		double adjLux = lux;
		if(adjLux < settings.minBrightnessLux) adjLux = settings.minBrightnessLux;
		if(adjLux > settings.maxBrightnessLux) adjLux = settings.maxBrightnessLux;

		normalized = float((adjLux - settings.minBrightnessLux) / (settings.maxBrightnessLux - settings.minBrightnessLux));

		//Serial.printf("%f %f\n", lux, normalized);
	}
}

bool
CModule_LuminositySensor::GetLux(
	unsigned char	inGain, 
	unsigned int	inMS, 
	unsigned int	inCH0, 
	unsigned int	inCH1, 
	double&			outLux)
{
	// Convert raw data to lux
	// gain: 0 (1X) or 1 (16X), see setTiming()
	// ms: integration time in ms, from setTiming() or from manual integration
	// CH0, CH1: results from getData()
	// lux will be set to resulting lux calculation
	// returns true (1) if calculation was successful
	// RETURNS false (0) AND lux = 0.0 IF EITHER SENSOR WAS SATURATED (0XFFFF)

	double ratio, d0, d1;

	// Determine if either sensor saturated (0xFFFF)
	// If so, abandon ship (calculation will not be accurate)
	if ((inCH0 == 0xFFFF) || (inCH1 == 0xFFFF))
	{
		outLux = 0.0;
		return false;
	}

	// Convert from unsigned integer to floating point
	d0 = inCH0; 
	d1 = inCH1;

	// We will need the ratio for subsequent calculations
	ratio = d1 / d0;

	// Normalize for integration time
	d0 *= (402.0/inMS);
	d1 *= (402.0/inMS);

	// Normalize for gain
	if (!inGain)
	{
		d0 *= 16;
		d1 *= 16;
	}

	// Determine lux per datasheet equations:
	
	if (ratio < 0.5)
	{
		outLux = 0.0304 * d0 - 0.062 * d0 * pow(ratio, 1.4);
		return true;
	}

	if (ratio < 0.61)
	{
		outLux = 0.0224 * d0 - 0.031 * d1;
		return true;
	}

	if (ratio < 0.80)
	{
		outLux = 0.0128 * d0 - 0.0153 * d1;
		return true;
	}

	if (ratio < 1.30)
	{
		outLux = 0.00146 * d0 - 0.00112 * d1;
		return true;
	}

	outLux = 0.0;

	return true;
}

bool 
CModule_LuminositySensor::ReadI2CByte(
	uint8_t		inAddress, 
	uint8_t&	outValue)
{
	// Reads a byte from a TSL2561 address
	// Address: TSL2561 address (0 to 15)
	// Value will be set to stored byte
	// Returns true (1) if successful, false (0) if there was an I2C error
	// (Also see getError() above)

	// Set up command byte for read
	Wire.beginTransmission(_i2c_address);
	Wire.write((inAddress & 0x0F) | TSL2561_CMD);
	_error = Wire.endTransmission();

	// Read requested byte
	if (_error == 0)
	{
		Wire.requestFrom(_i2c_address, 1);
		if (Wire.available() == 1)
		{
			outValue = Wire.read();
			return true;
		}
	}

	return false;
}

bool
CModule_LuminositySensor::WriteI2CByte(
	uint8_t	inAddress,
	uint8_t	inValue)
{
	// Write a byte to a TSL2561 address
	// Address: TSL2561 address (0 to 15)
	// Value: byte to write to address
	// Returns true (1) if successful, false (0) if there was an I2C error
	// (Also see getError() above)

	// Set up command byte for write
	Wire.beginTransmission(_i2c_address);
	Wire.write((inAddress & 0x0F) | TSL2561_CMD);
	// Write byte
	Wire.write(inValue);
	_error = Wire.endTransmission();

	return _error == 0;
}

bool
CModule_LuminositySensor::ReadI2CUInt16(
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
	Wire.beginTransmission(_i2c_address);
	Wire.write((inAddress & 0x0F) | TSL2561_CMD);
	_error = Wire.endTransmission();

	// Read two bytes (low and high)
	if (_error == 0)
	{
		Wire.requestFrom(_i2c_address, 2);
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
CModule_LuminositySensor::WriteI2CUInt16(
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
