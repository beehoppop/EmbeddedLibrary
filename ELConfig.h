/*
	ELConfig.h
*/

#ifndef _ELCONFIG_H_
#define _ELCONFIG_H_

#include "ELModule.h"

enum
{
	eConfigVar_BlinkLED,

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

