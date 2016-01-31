#ifndef _ELMODULE_H_
#define _ELMODULE_H_
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

	This is the foundation of the EmbeddedLibrary. A module is a unit of functionality that manages eeprom storage, periodic updates, and initialization.
	Modules are meant to encapsulate units of functionality that sketches can use depending on their needs.
*/

#include <EL.h>

enum
{
	eMaxModuleCount = 32,
};

class CModule
{
public:
	
	// Return true if the module is enabled (ie its Update() method is called)
	virtual bool
	GetEnabledState(
		void);

	// Set the enabled state for the module
	virtual void
	SetEnabledState(
		bool	inEnabled);

	uint32_t		uid;	// The unique ID for the module

protected:
	
	// This is the initializer for the module
	CModule(
		char const*	inUID,					// This is the 4 character unique ID for the module
		uint16_t	inEEPROMSize = 0,		// This is the amount of eeprom space needed for the module
		uint16_t	inEEPROMVersion = 0,	// This is the version number of the eeprom (so the system can reinitialize eeprom when the version number changes)
		void*		inEEPROMData = NULL,	// A pointer to the local eeprom data storage
		uint32_t	inUpdateTimeUS = 0,		// The period between Update() calles
		uint8_t		inPriority = 0,			// This is the priority for initializing the module, higher priority modules are initialized before lower priority ones)
		bool		inEnabled = true);		// This is the initial enabled state for the module
	
	// Override this to setup the initial state of the module
	virtual void
	Setup(
		void);

	// Override this to get a periodic update call
	virtual void
	Update(
		uint32_t	inDeltaTimeUS);	// The time in us since the last update call

	// Override this to properly tear down the module
	virtual void
	TearDown(
		void);
	
	// Override this to reset the state of your module
	virtual void
	ResetState(
		void);
	
	// Override this to initialize eeprom data to a default value
	virtual void
	EEPROMInitialize(
		void);

	// This saves the eeprom data into the eeprom
	void
	EEPROMSave(
		void);
	
	uint16_t		eepromOffset;

private:

	uint16_t		eepromSize;
	uint16_t		eepromVersion;
	void*			eepromData;
	uint32_t		updateTimeUS;
	uint64_t		lastUpdateUS;
	uint8_t			priority;
	bool			enabled;
	bool			hasBeenSetup;

	// This is called from the sketch's setup() function in the .ino file
	static void
	SetupAll(
		char const*	inVersionStr,
		bool		inFlashLED);

	// This is called from the sketch's loop() function in the .ino file
	static void
	LoopAll(
		void);

	// This is called to tear down all modules
	static void
	TearDownAll(
		void);

	// This is called to reset all module state
	static void
	ResetAllState(
		void);
	
	// friend definitions for SetupAll() and LoopAll()
	friend void
	setup(
		void);

	friend void
	loop(
		void);
};

extern uint64_t		gCurLocalMS;	// The accumulated ms since boot, its 64-bit so it will never overflow
extern uint64_t		gCurLocalUS;	// The accumulated us since boot, its 64-bit so it will never overflow

#endif /* _ELMODULE_H_ */
