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

	A module's constructor can only initialize itself, it can not access other modules
	A module's Setup() method may reference other modules, any module included during a constructor will itself be constructed and added to the module list
*/

#include <EL.h>
#include <ELUtilities.h>

enum
{
	eMaxModuleSingletonCount = 16,
	eMaxModuleDynamicCount = 16,
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

	char const*		uid;	// The unique ID for the module

protected:
	
	// This is the initializer for the module
	CModule(
		uint16_t	inEEPROMSize = 0,		// This is the amount of eeprom space needed for the module
		uint16_t	inEEPROMVersion = 0,	// This is the version number of the eeprom (so the system can reinitialize eeprom when the version number changes)
		void*		inEEPROMData = NULL,	// A pointer to the local eeprom data storage
		uint32_t	inUpdateTimeUS = 0,		// The period between Update() calles
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
	
	// Call this at the end of a module's constructor after all other modules have been included so that the module's setup method may be called
	void
	DoneIncluding(
		void);

	uint16_t		eepromOffset;

private:

	uint16_t		eepromSize;
	uint16_t		eepromVersion;
	void*			eepromData;
	uint32_t		updateTimeUS;
	uint32_t		classSize;
	uint64_t		lastUpdateUS;
	bool			isSingleton;
	bool			enabled;
	bool			hasBeenSetup;

	void
	SetupIfNeeded(
		void);

	void
	UpdateIfNeeded(
		void);

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

void
StartingModuleConstruction(
	char const*	inClassName,
	uint32_t	inClassSize,
	bool		inSingleton);

#define MModuleSingleton_Declaration(inClassName)	\
	static void											\
	Include(										\
		void);

#define MModuleSingleton_ImplementationGlobal(inClassName, inGlobalVariable)		\
void																				\
inClassName::Include(																\
	void)																			\
{																					\
	static bool	initialized = false;												\
	if(initialized)																	\
	{																				\
		/* recursion in the constructor, this is allowed for singleton modules */	\
		return;																		\
	}																				\
	initialized = true;																\
	StartingModuleConstruction(#inClassName, sizeof(inClassName), true);			\
	inGlobalVariable = new inClassName();											\
}

#define MModuleSingleton_Implementation(inClassName)								\
void																				\
inClassName::Include(																\
	void)																			\
{																					\
	static bool	initialized = false;												\
	if(initialized)																	\
	{																				\
		/* recursion in the constructor, this is allowed for singleton modules */	\
		return;																		\
	}																				\
	initialized = true;																\
	StartingModuleConstruction(#inClassName, sizeof(inClassName), true);			\
	new inClassName();																\
}

#define MModule_Declaration(inClassName, ...)	\
	static inClassName*							\
	Include(									\
		__VA_ARGS__);

#define MModuleImplementation_Start(inClassName, ...)	\
inClassName*											\
inClassName::Include(									\
	__VA_ARGS__)										\
{

#define MModuleImplementation(inClassName, ...)												\
	static bool	initialized = false;														\
	if(initialized)																			\
	{																						\
		/* recursion in the constructor, this is not allowed for non singleton modules*/	\
		MAssert(0);																			\
		return NULL;																		\
	}																						\
	initialized = true;																		\
	StartingModuleConstruction(#inClassName, sizeof(inClassName), false);					\
	inClassName*	result = new inClassName(__VA_ARGS__);									\
	return result;																			\
}

#endif /* _ELMODULE_H_ */
