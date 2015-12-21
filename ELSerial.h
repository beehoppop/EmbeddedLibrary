#ifndef _ELSERIAL_H_
#define _ELSERIAL_H_

// TCSerial.h

#include "ELModule.h"

enum
{
	eSerial_MaxNameLen = 16,
	eSerial_MaxCommands = 64,
	eSerial_MaxCommandArgs = 64,
};

class ISerialCmdHandler
{
public:
};

typedef bool
(ISerialCmdHandler::*TSerialCmdMethod)(
	int		inArgC,
	char*	inArgv[]);

class CModule_SerialCmd : public CModule
{
public:
	
	CModule_SerialCmd(
		);

	virtual void
	Update(
		uint32_t	inDeltaTimeUS);

	void
	RegisterCommand(
		char const*			inCmdName,
		ISerialCmdHandler*	inCmdHandler,
		TSerialCmdMethod	inMethod);

private:
	
	struct SCommand
	{
		char				name[eSerial_MaxNameLen];
		ISerialCmdHandler*	handler;
		TSerialCmdMethod	method;
	};

	bool
	ProcessSerialMsg(
		char*	inMsg);

	int			handlerCount;
	SCommand	commandList[eSerial_MaxCommands];

	char	charBuffer[256];
	int		curIndex;
};

extern CModule_SerialCmd	gSerialCmd;

#endif /* _ELSERIAL_H_ */


