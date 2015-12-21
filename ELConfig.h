#ifndef _ELCONFIG_H_
#define _ELCONFIG_H_

/*
	ELConfig.h
*/

#include <ELModule.h>

enum
{
	eConfigVar_NodeID,				// A unique number to identify this node in a networked environment such as a CAN bus
	eConfigVar_BlinkLED,			// If non-zero blink the LED during the main update loop as proof of life
	eConfigVar_DebugLevel,			// See ELAssert.h eDbgLevel_* enum values
	eConfigVar_FirstUserAvailable,	// User sketches can add additional config values here

	eConfigVar_Max = 16
};

class CModule_Config : public CModule
{
public:
	
	CModule_Config(
		);

	virtual void
	Setup(
		void);
	
	virtual void
	ResetState(
		void);
	
	uint8_t
	GetVal(
		uint8_t	inVar);

	void
	SetVal(
		uint8_t	inVar,
		uint8_t	inVal);

private:

	uint8_t	configVar[eConfigVar_Max];
	bool	setupComplete;
};

extern CModule_Config	gConfig;

#endif /* _ELCONFIG_H_ */

