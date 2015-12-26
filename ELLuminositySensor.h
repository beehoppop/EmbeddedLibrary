#ifndef _FHLUMINOSITYSENSOR_H_
#define _FHLUMINOSITYSENSOR_H_

#include <SparkFunTSL2561.h>

class CModule_LuminositySensor : public CModule, public ISerialCmdHandler
{
public:

	float
	GetActualLux(
		void);

	float
	GetNormalizedLux(
		void);


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
