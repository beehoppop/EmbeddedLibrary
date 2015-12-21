#ifndef _ELMODULE_H_
#define _ELMODULE_H_

/*
	ELModule.h
*/

#include <EL.h>

enum
{
	eMaxModuleCount = 16,
};

class CModule
{
public:
	
	CModule(
		char const*	inUID,
		uint16_t	inEEPROMSize,
		uint16_t	inEEPROMVersion,
		uint32_t	inUpdateTimeUS = 0,
		uint8_t		inPriority = 0,
		bool		inEnabled = true);

	virtual void
	Setup(
		void);

	virtual void
	TearDown(
		void);

	virtual bool
	GetEnabledState(
		void);

	virtual void
	SetEnabledState(
		bool	inEnabled);

	virtual void
	Update(
		uint32_t	inDeltaTimeUS);
	
	virtual void
	ResetState(
		void);
	
	virtual void
	EEPROMInitialize(
		void);

	//
	// static methods
	//

	static void
	SetupAll(
		bool	inFlashLED);

	static void
	TearDownAll(
		void);

	static void
	ResetAllState(
		void);

	static void
	LoopAll(
		void);

	uint32_t		uid;

protected:
	
	uint16_t		eepromSize;
	uint16_t		eepromOffset;
	uint16_t		eepromVersion;
	uint32_t		updateTimeUS;
	uint64_t		lastUpdateUS;
	uint8_t			priority;
	bool			enabled;
};

extern uint64_t		gCurLocalMS;
extern uint64_t		gCurLocalUS;

#endif /* _ELMODULE_H_ */
