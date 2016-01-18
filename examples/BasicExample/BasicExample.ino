/*
	Author: Brent Pease

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

	This is the main ino for the BasicExample example sketch in the EmbeddedLibrary library
*/

#include <EEPROM.h>
#include <Wire.h>
#include <SPI.h>
#include <FlexCAN.h>

#include <ELModule.h>
#include <ELDigitalIO.h>
#include <ELCommand.h>
#include <ELRealTime.h>
#include <ELAssert.h>

class CExampleModule : public CModule, public IDigitalIOEventHandler, public ICmdHandler, public IRealTimeHandler
{
	CExampleModule(
		)
		:
		CModule(
			"expl",		// This is the 4 character name of the module, every module has a unique 4 character code.
			0,			// This is the amount of EEPROM space this module needs
			0,			// This is the version number of the EEPROM data format. If this changes the system will re-initialize the EEPROM data for you
			NULL,		// This is a pointer to the local memory to store the EEPROM data
			30000)		// This is the period for calling the Update function
	{
	}

	virtual void
	Setup(
		void)
	{
		// this will be called after the other system level modules have been initialized
		Serial.printf("In Setup\n");

		// Register a serial port command.
		gCommand->RegisterCommand("hello", this, static_cast<TCmdHandlerMethod>(&CExampleModule::SerialCmdHello));

		// Register a digital io event handler on pin 1
		gDigitalIO->RegisterEventHandler(1, false, this, static_cast<TDigitalIOEventMethod>(&CExampleModule::PinActivated), NULL);

		// Register an event to fire every 5 seconds
		gRealTime->RegisterEvent("MyEvent", 5 * 1000000, false, this, static_cast<TRealTimeEventMethod>(&CExampleModule::MyPeriodicEvent), NULL);

		updateCount = 0;
	}

	virtual void
	Update(
		uint32_t	inDeltaTimeUS)
	{
		// this is called apprx every 30ms as specified in the module constructor above
		++updateCount;
	}

	bool
	SerialCmdHello(
		IOutputDirector*	inOutput,
		int					inArgC,
		char const*			inArgv[])
	{
		inOutput->printf("Hello World\n");

		return true;	// Returning true means command was processed successfully, returning false will print an error to the proper output
	}

	void
	PinActivated(
		uint8_t		inPin,
		EPinEvent	inEvent,
		void*		inReference)
	{
		Serial.printf("pin=%d event=%d\n", inPin, inEvent);
	}

	void
	MyPeriodicEvent(
		char const*	inName,
		void*		inRef)
	{
		DebugMsg(eDbgLevel_Basic, "event %s has fired", inName);
		DebugMsg(eDbgLevel_Basic, "updateCount = %d", updateCount);
	}

	int	updateCount;

	static CExampleModule	module;
};
CExampleModule	CExampleModule::module;

void
setup(
	void)
{
	CModule::SetupAll("v0.3", true);	// Passing in true blinks the led when the blink_led config var is set to one
}

void
loop(
	void)
{
	CModule::LoopAll();
}
