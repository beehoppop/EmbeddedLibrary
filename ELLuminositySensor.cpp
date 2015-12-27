

#include <ELModule.h>
#include <ELUtilities.h>
#include <ELSerial.h>
#include <ELLuminositySensor.h>

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
CModule_LuminositySensor::SerialConfig(
	int		inArgC,
	char*	inArgv[])
{
	return false;
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

	gSerialCmd->RegisterCommand("lumin", this, static_cast<TSerialCmdMethod>(&CModule_LuminositySensor::SerialConfig));
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
