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

*/
#include <ELModule.h>
#include <ELAssert.h>
#include <ELServo_AX12.h>

enum
{
	eAXModelNumberL			= 0,
	eAXModelNumberH			= 1,
	eAXVersion				= 2,
	eAXID					= 3,
	eAXBaudRate				= 4,
	eAXReturnDelayTime		= 5,
	eAXCWAngleLimit			= 6,
	eAXCCWAngleLimit		= 8,
	eAXSystemData2			= 10,
	eAXTemperatureLimit		= 11,
	eAXTDownVoltageLimit	= 12,
	eAXTUpVoltageLimit		= 13,
	eAXMaxTorque			= 14,
	eAXReturnLevel			= 16,
	eAXAlarmLED				= 17,
	eAXAlarmShutdown		= 18,
	eAXOperatingMode		= 19,
	eAXDownCalibration		= 20,
	eAXUpCalibration		= 22,
	eAXTorqueEnable			= 24,
	eAXLED					= 25,
	eAXCWComplianceMargin	= 26,
	eAXCCWComplianceMargin	= 27,
	eAXCWComplianceSlope	= 28,
	eAXCCWComplianceSlope	= 29,
	eAXGoalPosition			= 30,
	eAXGoalSpeed			= 32,
	eAXTorqueLimit			= 34,
	eAXPresentPosition		= 36,
	eAXPresentSpeed			= 38,
	eAXPresentLoad			= 40,
	eAXPresentVoltage		= 42,
	eAXPresenttemperature	= 43,
	eAXRegisteredInstruction	= 44,
	eAXPauseTime			= 45,
	eAXMoving				= 46,
	eAXLock					= 47,
	eAXPunch				= 48,

	eAXInstr_Ping		= 1,
	eAXInstr_ReadData	= 2,
	eAXInstr_WriteData	= 3,
	eAXInstr_RegWrite	= 4,
	eAXInstr_Action		= 5,
	eAXInstr_Reset		= 6,
	eAXInstr_SyncWrite	= 131,

	eSerialReadTimeoutMS = 2000,
	eSerialIsBeingMovedTimeoutMS = 1000,

};

/** Status Return Levels **/
#define AX_RETURN_NONE              0
#define AX_RETURN_READ              1
#define AX_RETURN_ALL               2

/** Instruction Set **/

/** Error Levels **/
#define ERR_NONE                    0
#define ERR_VOLTAGE                 1
#define ERR_ANGLE_LIMIT             2
#define ERR_OVERHEATING             4
#define ERR_RANGE                   8
#define ERR_CHECKSUM                16
#define ERR_OVERLOAD                32
#define ERR_INSTRUCTION             64

#define MReturnStatusOnWrite	1
#define MLogRegisterWrites		1

const float	cMaxAnglePosition = 150.0f / 180.0f;

class CServoAX12 : public IServo, public CModule
{
public:
	MModule_Declaration(
		CServoAX12,
		int				inID,
		HardwareSerial*	inSerialPort,
		int				inDirectionPin)

	CServoAX12(
		int				inID,
		HardwareSerial*	inSerialPort,
		int				inDirectionPin)
	:
	CModule(0, 0, NULL, 20000, true),
	servoID(inID),
	dirPin(inDirectionPin),
	serialPort(inSerialPort)
	{
		pinMode(dirPin, OUTPUT);
		DirectionEnable(true);

		servoHandler = NULL;
		handlerMethod = NULL;
		handlerRefcon = 0;

		travelingToGoal = false;
		isBeingMoved = false;
		goalPositionValue = 0;
		curPositionValue = 0;
		curSpeedValue = 0;

		lastMoveTime = 0;

		serialPort->begin(1000000);

		// Set the return delay time to be 2us
		WriteRegisterByte(eAXReturnLevel, MReturnStatusOnWrite ? 2 : 1);

		WriteRegisterByte(eAXReturnDelayTime, 1);
		WriteRegisterByte(eAXAlarmLED, 1);
		WriteRegisterByte(eAXAlarmShutdown, 1);

		//SetGoalPosition(1.0f, 1.0f);
		SetContinuousRotation(true);
		//SetTorque(0.1f);
		SetContinuousSpeed(-0.1f);
	}

	virtual bool
	IsSelfMoving(
		void)
	{
		return travelingToGoal;
	}

	virtual bool
	IsBeingMoved(
		void)
	{
		if(travelingToGoal)
		{
			return false;
		}

		return isBeingMoved;
	}

	virtual float
	GetDistanceFromGoal(
		void)
	{
		if(!travelingToGoal)
		{
			return 0;
		}

		return (float)(curPositionValue - goalPositionValue) * (cMaxAnglePosition * 2 / 1024.0f);
	}

	virtual float
	GetPosition(
		void)
	{
		int	value = ReadRegisterWord(eAXPresentPosition);

		return ConvertAXValueToPosition(value);
	}

	virtual float
	GetSpeed(
		void)
	{
		int	value = ReadRegisterWord(eAXPresentPosition);

		return (float)value / -511.5f + 1.0f + (1 / 1023.0f);
	}

	virtual void
	GetPositionLimits(
		float&	outCounterclockwise,
		float&	outClockwise)
	{
		outCounterclockwise = -cMaxAnglePosition;
		outClockwise = cMaxAnglePosition;
	}
	
	virtual void
	SetContinuousRotation(
		bool	inContinuousRotation,
		bool	inImmediate = false)
	{
		if(inContinuousRotation)
		{
			WriteRegisterWord(eAXCCWAngleLimit, 0);
			WriteRegisterWord(eAXCWAngleLimit, 0);
		}
		else
		{
			WriteRegisterWord(eAXCCWAngleLimit, 1023);
			WriteRegisterWord(eAXCWAngleLimit, 0);
		}
	}

	virtual void
	SetTorque(
		float	inTorque,
		bool	inImmediate = false)
	{
		if(inTorque == 0)
		{
			WriteRegisterWord(eAXTorqueEnable, 0);
			WriteRegisterWord(eAXTorqueLimit, 0);
		}
		else
		{
			WriteRegisterWord(eAXTorqueEnable, 1);
			WriteRegisterWord(eAXTorqueLimit, (int)(inTorque * 1023.0f));
		}
	}

	virtual void
	SetContinuousSpeed(
		float	inSpeed,
		bool	inImmediate = false)
	{
		int	movingSpeed = int(fabs(inSpeed) * 1023.0f);
		if(inSpeed < 0)
		{
			movingSpeed |= 1 << 10;
		}

		SystemMsg("speed = %d", movingSpeed);

		WriteRegisterWord(eAXGoalSpeed, movingSpeed);
	}

	virtual void
	SetGoalPosition(
		float	inPosition,
		float	inSpeed,
		bool	inImmediate = false)
	{
		goalPositionValue = ConvertPositionToAXValue(inPosition);
		int goalSpeed = 1 + (int)(inSpeed * 1022.0f);
		WriteRegisterWord(eAXGoalSpeed, goalSpeed);
		WriteRegisterWord(eAXGoalPosition, goalPositionValue);
		travelingToGoal = true;
	}

	virtual void
	Flush(
		void)
	{

	}

	virtual void
	RegisterCallback(
		IServoHandler*			inObject,
		TCallbackHandlerMethod	inMethod,
		uint32_t				inRefCon)
	{
		servoHandler = inObject;
		handlerMethod = inMethod;
		handlerRefcon = inRefCon;
	}

	virtual void
	Update(
		uint32_t inDeltaTimeUS)
	{
		#if 0
		int newPositionValue = ReadRegisterWord(eAXPresentPosition);
		curSpeedValue = ReadRegisterWord(eAXPresentSpeed);

		if(travelingToGoal)
		{
			if(newPositionValue == goalPositionValue)
			{
				travelingToGoal = false;

				// issue callback
				if(servoHandler != NULL && servoHandler != NULL)
				{
					(servoHandler->*handlerMethod)(handlerRefcon, eCallbackEvent_ReachedGoal);
				}
			}
			isBeingMoved = false;
		}
		else
		{
			if(curPositionValue != newPositionValue)
			{
				isBeingMoved = true;
				lastMoveTime = millis();
			}
			else if(millis() - lastMoveTime >= eSerialIsBeingMovedTimeoutMS)
			{
				isBeingMoved = false;
			}
		}

		curPositionValue = newPositionValue;
		#endif
	}

	void
	DirectionEnable(
		bool	inTransmit)	// false is receive
	{
		digitalWriteFast(dirPin, inTransmit ? true : false);
	}

	void
	TransmitPacket(
		uint8_t		inInstruction,
		int			inParamCount,
		uint8_t*	inParamBytes)
	{
		// Ensure the serial port incoming buffer is clear before we transmit
		DirectionEnable(false);
		while(serialPort->read() >= 0)
		{
			SystemMsg("Got Garbage");
			delayMicroseconds(10);
		}
		DirectionEnable(true);

		MReturnOnError(inParamCount > 3);

		uint8_t	bytes[32];
		bytes[0] = 0xFF;
		bytes[1] = 0xFF;
		bytes[2] = servoID;
		bytes[3] = inParamCount + 2;
		bytes[4] = inInstruction;
		memcpy(bytes + 5, inParamBytes, inParamCount);
		uint8_t	checksum = 0;
		for(int i = 2; i < inParamCount + 5; ++i)
		{
			checksum += bytes[i];
		}
		bytes[inParamCount + 5] = ~checksum;

		serialPort->write(bytes, inParamCount + 6);
		serialPort->flush();
		//DirectionEnable(false);
	}

	int
	ReadSerialPortByte(
		bool	inFirstByte = false)
	{
		uint32_t	startTime = millis();
		int	FFcount = 0;

		while((millis() - startTime) <= eSerialReadTimeoutMS)
		{
			int recvByte = serialPort->read();
			
			if(recvByte < 0)
			{
				continue;
			}

			Serial.printf("got %02x\n", recvByte);

			if(inFirstByte)
			{
				// Don't return the first byte until we have received two 0xFF values in a row, allow other values to clear out any cruft that might have been left from previous transactions
				if(recvByte == 0xFF)
				{
					++FFcount;
				}
				else if(FFcount == 2)
				{
					return recvByte;
				}
				else
				{
					FFcount = 0;
				}
				continue;
			}

			return recvByte;
		}

		Serial.printf("TIMEOUT\n");

		return -1;
	}

	void
	ReadPacket(
		uint8_t&	outError,
		uint8_t&	outLength,
		uint8_t*	outParamBytes)
	{
		DirectionEnable(false);
		
		outError = 0xFF;
		outLength = 0;

		int	recvId = ReadSerialPortByte(true);
		MReturnOnError(recvId < 0 || servoID != recvId);

		int	length = ReadSerialPortByte();
		MReturnOnError(length < 2 || length > 4);
		outLength = length - 2;

		int	error = ReadSerialPortByte();
		MReturnOnError(error < 0);

		for(int i = 0; i < length - 2; ++i)
		{
			outParamBytes[i] = ReadSerialPortByte();
			MReturnOnError(outParamBytes[i] < 0);
		}

		int	checksum = ReadSerialPortByte();
		MReturnOnError(checksum < 0);

		uint8_t	axChecksum = (uint8_t)recvId + (uint8_t)length + (uint8_t)error;
		for(int i = 0; i < length - 2; ++i)
		{
			axChecksum += outParamBytes[i];
		}
		MReturnOnError((uint8_t)~axChecksum != (uint8_t)checksum);

		DirectionEnable(true);

		outError = error;
	}

	void
	WriteRegisterByte(
		int	inAddr,
		int	inValue)
	{
		#if MLogRegisterWrites
		SystemMsg("AX %d write 0x%x to 0x%x", servoID,  inValue, inAddr);
		#endif

		uint8_t	bytes[2];

		bytes[0] = (uint8_t)inAddr;
		bytes[1] = (uint8_t)inValue;

		TransmitPacket(eAXInstr_WriteData, 2, bytes);

		#if MReturnStatusOnWrite
		uint8_t	error;
		uint8_t	length;
		uint8_t	params[8];
		ReadPacket(error, length, params);
		#endif
	}

	void
	WriteRegisterWord(
		int	inAddr,
		int	inValue)
	{
		#if MLogRegisterWrites
		SystemMsg("AX %d write 0x%x to 0x%x", servoID,  inValue, inAddr);
		#endif

		uint8_t	bytes[3];

		bytes[0] = (uint8_t)inAddr;
		bytes[1] = (uint8_t)(inValue & 0xFF);
		bytes[2] = (uint8_t)(inValue >> 8);

		TransmitPacket(eAXInstr_WriteData, 3, bytes);

		#if MReturnStatusOnWrite
		uint8_t	error;
		uint8_t	length;
		uint8_t	params[8];
		ReadPacket(error, length, params);

		if(error != 0)
		{
			SystemMsg("error = %d", error);
		}
		#endif
	}

	int
	ReadRegisterByte(
		int	inAddr)
	{
		uint8_t	bytes[2];

		bytes[0] = (uint8_t)inAddr;
		bytes[1] = 1;

		TransmitPacket(eAXInstr_ReadData, 2, bytes);

		uint8_t	error;
		uint8_t	length;
		uint8_t	returnBytes[4];

		ReadPacket(error, length, returnBytes);
		MReturnOnError(error != 0 || length != 1, -1);

		return returnBytes[0];
	}

	int
	ReadRegisterWord(
		int	inAddr)
	{
		uint8_t	bytes[2];

		bytes[0] = (uint8_t)inAddr;
		bytes[1] = 1;

		TransmitPacket(eAXInstr_ReadData, 2, bytes);

		uint8_t	error;
		uint8_t	length;
		uint8_t	returnBytes[4];

		ReadPacket(error, length, returnBytes);
		MReturnOnError(error != 0 || length != 1, -1);

		return (int)returnBytes[0] | ((int)returnBytes[1] << 8);
	}

	int
	ConvertPositionToAXValue(
		float	inPosition)
	{
		inPosition = MPin(-cMaxAnglePosition, inPosition, cMaxAnglePosition);

		if(inPosition >= 0.0f)
		{
			return (int)ReMapValue(inPosition, 0, cMaxAnglePosition, 511.0f, 0);
		}

		return (int)ReMapValue(inPosition, -cMaxAnglePosition, 0, 1023.0f, 511.0f);
	}

	float
	ConvertAXValueToPosition(
		int	inValue)
	{
		/*
			The AX12 has 1024 distinct positions, 0x1ff is center, 0 is max clockwise, and 0x3ff is max counterclockwise, this means there are 511 positions clockwise and 512 positions counterclockwise
		*/

		if(inValue <= 511)
		{
			return ReMapValue((float)inValue, 511.0f, 0, 0, cMaxAnglePosition);
		}

		return ReMapValue((float)inValue, 1023.0f, 511.0f, -cMaxAnglePosition, 0);
	}

	IServoHandler*			servoHandler;
	TCallbackHandlerMethod	handlerMethod;
	uint32_t				handlerRefcon;

	uint32_t	lastMoveTime;

	int	goalPositionValue;
	int	curPositionValue;
	int	curSpeedValue;

	bool	travelingToGoal;
	bool	isBeingMoved;

	int	servoID;
	int	dirPin;
	HardwareSerial*	serialPort;
};

MModuleImplementation_Start(CServoAX12,
	int				inID,
	HardwareSerial*	inSerialPort,
	int				inDirectionPin)
MModuleImplementation_Finish(CServoAX12, inID, inSerialPort, inDirectionPin)


IServo*
CreateAX12Servo(
	int				inID,
	HardwareSerial*	inSerialPort,
	int				inDirectionPin)
{
	return CServoAX12::Include(inID, inSerialPort, inDirectionPin);
}

