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


#include <ELModule.h>
#include <ELUtilities.h>
#include <ELSerial.h>
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

CModule_LuminositySensor	CModule_LuminositySensor::module;
CModule_LuminositySensor*	gLuminositySensor;

CModule_LuminositySensor::CModule_LuminositySensor(
	)
	:
	CModule(
		"lumn",
		sizeof(SLuminositySettings),
		1,
		500000,
		1)
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
CModule_LuminositySensor::GetNormalizedLux(
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
		WriteDataToEEPROM(&settings, eepromOffset, sizeof(settings));
	}
}

bool
CModule_LuminositySensor::SerialGetLux(
	int			inArgC,
	char const*	inArgv[])
{
	DebugMsg(eDbgLevel_Basic, "lux = %f\n", GetActualLux());

	return true;
}

void
CModule_LuminositySensor::Setup(
	void)
{
	LoadDataFromEEPROM(&settings, eepromOffset, sizeof(settings));
		
	if(settings.gain == 0xFF)
	{
		// data is uninitialized so give it some reasonable default values
		settings.gain = eGain_1X;
		settings.time = eIntegrationTime_402ms;
		settings.minBrightnessLux = 1.0;
		settings.maxBrightnessLux = 20000.0;
		WriteDataToEEPROM(&settings, eepromOffset, sizeof(settings));
	}

	light.begin();
		
	uint8_t	sensorID;
	bool	success;

	success = light.getID(sensorID);
	if(success == false || sensorID != 0x50)
	{
		SetEnabledState(false);
		return;
	}
		
	light.setTiming(settings.gain, settings.time, integrationTimeMS);
	light.setPowerUp();

	gSerialCmd->RegisterCommand("get_lux", this, static_cast<TSerialCmdMethod>(&CModule_LuminositySensor::SerialGetLux));
}

void
CModule_LuminositySensor::Update(
	uint32_t inDeltaTimeUS)
{
	unsigned int data0, data1;
  
	if(light.getData(data0,data1))
	{
		bool	good;

		good = light.getLux(settings.gain, integrationTimeMS, data0, data1, lux);
		if(good)
		{
			double adjLux = lux;
			if(adjLux < settings.minBrightnessLux) adjLux = settings.minBrightnessLux;
			if(adjLux > settings.maxBrightnessLux) adjLux = settings.maxBrightnessLux;

			normalized = float((adjLux - settings.minBrightnessLux) / (settings.maxBrightnessLux - settings.minBrightnessLux));

			//Serial.printf("%f %f\n", lux, normalized);
		}
	}
}
