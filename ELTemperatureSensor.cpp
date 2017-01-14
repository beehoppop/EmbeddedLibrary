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

	
*/
#include "EL.h"
#include "ELTemperatureSensor.h"

#define MCP9808_I2CADDR_DEFAULT        0x18
#define MCP9808_REG_CONFIG             0x01

#define MCP9808_REG_CONFIG_SHUTDOWN    0x0100
#define MCP9808_REG_CONFIG_CRITLOCKED  0x0080
#define MCP9808_REG_CONFIG_WINLOCKED   0x0040
#define MCP9808_REG_CONFIG_INTCLR      0x0020
#define MCP9808_REG_CONFIG_ALERTSTAT   0x0010
#define MCP9808_REG_CONFIG_ALERTCTRL   0x0008
#define MCP9808_REG_CONFIG_ALERTSEL    0x0004
#define MCP9808_REG_CONFIG_ALERTPOL    0x0002
#define MCP9808_REG_CONFIG_ALERTMODE   0x0001

#define MCP9808_REG_UPPER_TEMP         0x02
#define MCP9808_REG_LOWER_TEMP         0x03
#define MCP9808_REG_CRIT_TEMP          0x04
#define MCP9808_REG_AMBIENT_TEMP       0x05
#define MCP9808_REG_MANUF_ID           0x06
#define MCP9808_REG_DEVICE_ID          0x07

class CMCP9808 : public ITemperatureSensor
{
public:
	
	CMCP9808(
		uint8_t	inI2CAddr)
	{
		i2cAddr = inI2CAddr;
	}

	virtual void
	Destroy(
		void)
	{

	}

	virtual bool
	Initialize(
		void)
	{
		Wire.begin();
		Wire.setDefaultTimeout(200000); // 200ms

		if(Read16(MCP9808_REG_MANUF_ID) != 0x0054)
		{
			return false;
		}
		if(Read16(MCP9808_REG_DEVICE_ID) != 0x0400)
		{
			return false;
		}

		return true;
	}

	virtual void
	SetSensorState(
		bool	inOn)
	{
		uint16_t confShutdown;
		uint16_t confRegister = Read16(MCP9808_REG_CONFIG);

		if (inOn)
		{
			confShutdown = confRegister & ~MCP9808_REG_CONFIG_SHUTDOWN ;
			Write16(MCP9808_REG_CONFIG, confShutdown);
		}
		else
		{
			confShutdown = confRegister | MCP9808_REG_CONFIG_SHUTDOWN ;
			Write16(MCP9808_REG_CONFIG, confShutdown);
		}
	}

	virtual float
	ReadTempF(
		void)
	{
		return ReadTempC() * (9.0f / 5.0f) + 32.0f;
	}

	virtual float
	ReadTempC(
		void)
	{
		uint16_t rawTemp = Read16(MCP9808_REG_AMBIENT_TEMP);

		float result = (float)(rawTemp & 0x0FFF);
		
		result /=  16.0;

		if (rawTemp & 0x1000)
		{
			result -= 256;
		}

		return result;
	}

	virtual int16_t
	ReadTempC16ths(
		void)
	{
		uint16_t	t = Read16(MCP9808_REG_AMBIENT_TEMP);
		uint16_t	result;

		result = t & 0xFFF;
		if (t & 0x1000)
		{
			result |= 0xF000;
		}

		return (int16_t)result;
	}

	void
	Write16(
		uint8_t		inReg,
		uint16_t	inValue) 
	{
		Wire.beginTransmission(i2cAddr);
		Wire.write(inReg);
		Wire.write(inValue >> 8);
		Wire.write(inValue & 0xFF);
		Wire.endTransmission();
	}

	uint16_t
	Read16(
		uint8_t	inReg) 
	{
		uint16_t val;

		Wire.beginTransmission(i2cAddr);
		Wire.write(inReg);
		Wire.endTransmission();
  
		Wire.requestFrom((uint8_t)i2cAddr, (uint8_t)2);
		val = Wire.read() << 8;
		val |= Wire.read(); 

		return val;  
	}

	uint8_t	i2cAddr;
};

ITemperatureSensor*
CreateMCP9808(
	uint8_t	inI2CAddr)
{
	return new CMCP9808(inI2CAddr);
}
