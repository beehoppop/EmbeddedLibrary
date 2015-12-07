#ifndef _EL_H_
#define _EL_H_

#if defined(ARDUINO) && ARDUINO >= 100
	#include "arduino.h"
	#include <EEPROM.h>
	#include <SPI.h>
#elif defined(WIN32)
	#include "ArduinoSimulator.h"
#else
	#include "WProgram.h"
#endif

#endif /* _EL_H_ */
