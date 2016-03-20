#ifndef _ELCONFIG_H_
#define _ELCONFIG_H_
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

	Global configuration of 8-bit values stored in eeprom
*/

#include <ELModule.h>
#include <ELCommand.h>
#include <ELOutput.h>

enum
{
	eConfigVar_Max = 16,

	eConfigVar_MaxNameLength = 15,
};

class CModule_Config : public CModule, public ICmdHandler
{
public:
	
	CModule_Config(
		);
	
	// Return the value given the config var
	uint8_t
	GetVal(
		uint8_t	inVar);

	// Set the config var
	void
	SetVal(
		uint8_t	inVar,
		uint8_t	inVal);

	int
	RegisterConfigVar(
		char const*	inName);
	
	int	nodeIDIndex;		// config index for the node ID config var
	int	debugLevelIndex;	// config index for the debug level config var

private:

	virtual void
	Setup(
		void);

	uint8_t
	SetConfig(
		IOutputDirector*	inOutput,
		int					inArgC,
		char const*			inArgV[]);

	uint8_t
	GetConfig(
		IOutputDirector*	inOutput,
		int					inArgC,
		char const*			inArgV[]);

	int
	GetVarFromStr(
		char const*	inStr);

	void
	SetupFinished(
		void);

	struct SConfigVar
	{
		char	name[eConfigVar_MaxNameLength];
		uint8_t	value;
	};

	SConfigVar	configVars[eConfigVar_Max];
	bool		configVarUsed[eConfigVar_Max];

	friend class CModule;
};

extern CModule_Config*	gConfig;

#endif /* _ELCONFIG_H_ */
