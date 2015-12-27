#ifndef _FHLUMINOSITYSENSOR_H_
#define _FHLUMINOSITYSENSOR_H_

#if defined(WIN32)
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
