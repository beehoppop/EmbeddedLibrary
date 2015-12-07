/*
	ELUtilities.h
*/

#ifndef _ELUTILITIES_H_
#define _ELUTILITIES_H_

#include "EL.h"
bool
IsStrDigit(
	char const*	inStr);

bool
IsStrAlpha(
	char const*	inStr);

void
LoadDataFromEEPROM(
	void*		inDst,
	uint16_t	inStartAddress,
	uint16_t	inSize);

void
WriteDataToEEPROM(
	void*		inSrc,
	uint16_t	inStartAddress,
	uint16_t	inSize);

char const*
StringizeUInt32(
	uint32_t	inValue);

#endif /* _ELUTILITIES_H_ */

