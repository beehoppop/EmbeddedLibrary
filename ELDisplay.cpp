

/*
This is the core graphics library for all our displays, providing a common
set of graphics primitives (points, lines, circles, etc.).  It needs to be
paired with a hardware-specific library for each display device we carry
(to handle the lower-level functions).

Adafruit invests time and resources providing this open source code, please
support Adafruit & open-source hardware by purchasing products from Adafruit!

Copyright (c) 2013 Adafruit Industries.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

- Redistributions of source code must retain the above copyright notice,
  this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
*/


#include "ELDisplay.h"
#include "ELAssert.h"
#include "ELModule.h"

CModule_Display*	gDisplayModule;
SDisplayColor	gColorWhite(255, 255, 255);
SDisplayColor	gColorBlack(0, 0, 0);
SDisplayColor	gColorRed(255, 0, 0);
SDisplayColor	gColorGreen(0, 255, 0);
SDisplayColor	gColorBlue(0, 0, 255);

#ifdef WIN32
#include "ELDisplay_ILI9341Win.h"
#else
#include "ELDisplay_ILI9341.h"
#endif

SPlacement
SPlacement::Outside(
	uint8_t	inSide,
	uint8_t	inAlignment)
{
	return SPlacement(ePlace_Outside, inSide, inAlignment);
}

SPlacement
SPlacement::Inside(
	uint8_t	inHoriz,
	uint8_t	inVert)
{
	return SPlacement(ePlace_Inside, inHoriz, inVert);
}

IDisplayDriver*
CreateILI9341Driver(
	EDisplayOrientation	inDisplayOrientation,
	uint8_t	inCS,
	uint8_t	inDC,
	uint8_t	inMOSI,
	uint8_t	inClk,
	uint8_t	inMISO)
{
	static IDisplayDriver*	displayDriver = NULL;

	if(displayDriver == NULL)
	{
		displayDriver = new CDisplayDriver_ILI9341(inDisplayOrientation, inCS, inDC, inMOSI, inClk, inMISO);
	}

	return displayDriver;
}

ITouchDriver*
CreateXPT2046Driver(
	uint8_t	inChipselect)
{

	static ITouchDriver*	touchDriver = NULL;

	if(touchDriver == NULL)
	{
		touchDriver = new ITouchDriver_XPT2046(inChipselect);
	}

	return touchDriver;
}

static uint32_t fetchbit(const uint8_t *p, uint32_t index)
{
	if (p[index >> 3] & (1 << (7 - (index & 7)))) return 1;
	return 0;
}

static uint32_t fetchbits_unsigned(const uint8_t *p, uint32_t index, uint32_t required)
{
	uint32_t val = 0;
	do {
		uint8_t b = p[index >> 3];
		uint32_t avail = 8 - (index & 7);
		if (avail <= required) {
			val <<= avail;
			val |= b & ((1 << avail) - 1);
			index += avail;
			required -= avail;
		} else {
			b >>= avail - required;
			val <<= required;
			val |= b & ((1 << required) - 1);
			break;
		}
	} while (required);
	return val;
}

static uint32_t fetchbits_signed(const uint8_t *p, uint32_t index, uint32_t required)
{
	uint32_t val = fetchbits_unsigned(p, index, required);
	if (val & (1 << (required - 1))) {
		return (int32_t)val - (1 << required);
	}
	return (int32_t)val;
}

CDisplayRegion::CDisplayRegion(
	SPlacement		inPlacement,
	CDisplayRegion*	inParent)
	:
	parent(inParent),
	firstChild(NULL),
	nextChild(NULL),
	placement(inPlacement),
	curRect(0, 0, 0, 0),
	oldRect(0, 0, 0, 0),
	width(0),
	height(0),
	borderLeft(0),
	borderRight(0),
	borderTop(0),
	borderBottom(0),
	fixedSize(false)
{
	touchObject = NULL;

	if(parent != NULL)
	{
		nextChild = inParent->firstChild;
		inParent->firstChild = this;
	}
}
	
CDisplayRegion::CDisplayRegion(
	SDisplayRect const&	inRect,
	CDisplayRegion*		inParent)
	:
	parent(inParent),
	firstChild(NULL),
	nextChild(NULL),
	placement(SPlacement::Inside(eInside_Horiz_Left, eInside_Vert_Top)),
	curRect(inRect),
	oldRect(inRect),
	width(0),
	height(0),
	borderLeft(0),
	borderRight(0),
	borderTop(0),
	borderBottom(0),
	fixedSize(false)
{
	touchObject = NULL;

	if(parent != NULL)
	{
		nextChild = inParent->firstChild;
		inParent->firstChild = this;
	}
}

int16_t
CDisplayRegion::GetWidth(
	void)
{
	return width;
}

int16_t
CDisplayRegion::GetHeight(
	void)
{
	return height;
}

void
CDisplayRegion::AddToGraph(
	CDisplayRegion*	inParent)
{
	MReturnOnError(parent != NULL);
	parent = inParent;
	nextChild = inParent->firstChild;
	inParent->firstChild = this;
}

void
CDisplayRegion::RemoveFromGraph(
	void)
{
	MReturnOnError(parent == NULL);

	CDisplayRegion*	prevRegion = NULL;
	CDisplayRegion*	curRegion = parent->firstChild;

	while(curRegion != NULL)
	{
		if(curRegion == this)
		{
			break;
		}

		prevRegion = curRegion;
		curRegion = curRegion->nextChild;
	}

	if(prevRegion == NULL)
	{
		parent->firstChild = parent->firstChild->nextChild;
	}
	else
	{
		prevRegion->nextChild = nextChild;
	}

	parent = NULL;
}

void
CDisplayRegion::SetPlacement(
	SPlacement	inPlacement)
{
	placement = inPlacement;
}

void
CDisplayRegion::SetBorder(
	int16_t	inLeft,
	int16_t	inRight,
	int16_t	inTop,
	int16_t	inBottom)
{
	borderLeft = inLeft;
	borderRight = inRight;
	borderTop = inTop;
	borderBottom = inBottom;
}

void
CDisplayRegion::SetTouchHandler(
	ITouchHandler*		inHandler,
	TTouchHandlerMethod	inMethod,
	void*				inRefCon)
{
	touchObject = inHandler;
	touchMethod = inMethod;
	touchRefCon = inRefCon;
}
	
void
CDisplayRegion::ProcessTouch(
	ETouchEvent				inTouchEvent,
	SDisplayPoint const&	inTouchLoc)
{
	if((curRect && inTouchLoc) && touchObject != NULL)
	{
		(touchObject->*touchMethod)(inTouchEvent, inTouchLoc, touchRefCon);
	}

	// Children can be outside of the bounds so recurse on all children regardless of the point being contained in the parent
	CDisplayRegion*	curRegion = firstChild;

	while(curRegion != NULL)
	{
		curRegion->ProcessTouch(inTouchEvent, inTouchLoc);
		curRegion = curRegion->nextChild;
	}
}

void
CDisplayRegion::StartUpdate(
	void)
{
	oldRect = curRect;

	if(parent != NULL)
	{
		curRect.Reset();
	}

	CDisplayRegion*	curRegion = firstChild;

	while(curRegion != NULL)
	{
		curRegion->StartUpdate();
		curRegion = curRegion->nextChild;
	}
}

void
CDisplayRegion::UpdateDimensions(
	void)
{
	if(firstChild != NULL)
	{
		CDisplayRegion*	curRegion = firstChild;

		// update all of the children
		while(curRegion != NULL)
		{
			curRegion->UpdateDimensions();
			curRegion = curRegion->nextChild;
		}

		if(fixedSize == false)
		{
			// compute the width and height of this region
			int16_t	maxWidth  = 0;
			int16_t	maxHeight = 0;
			int16_t	curWidth;
			int16_t	curHeight;

			SumRegionList(curWidth, curHeight, SPlacement::Inside(eInside_Horiz_Left, eSecondary_Any), firstChild);
			if(curHeight > maxHeight)
			{
				maxHeight = curHeight;
			}

			SumRegionList(curWidth, curHeight, SPlacement::Inside(eInside_Horiz_Center, eSecondary_Any), firstChild);
			if(curHeight > maxHeight)
			{
				maxHeight = curHeight;
			}

			SumRegionList(curWidth, curHeight, SPlacement::Inside(eInside_Horiz_Right, eSecondary_Any), firstChild);
			if(curHeight > maxHeight)
			{
				maxHeight = curHeight;
			}

			SumRegionList(curWidth, curHeight, SPlacement::Inside(ePrimary_Any, eInside_Vert_Top), firstChild);
			if(curWidth > maxWidth)
			{
				maxWidth = curWidth;
			}

			SumRegionList(curWidth, curHeight, SPlacement::Inside(ePrimary_Any, eInside_Vert_Center), firstChild);
			if(curWidth > maxWidth)
			{
				maxWidth = curWidth;
			}

			SumRegionList(curWidth, curHeight, SPlacement::Inside(ePrimary_Any, eInside_Vert_Bottom), firstChild);
			if(curWidth > maxWidth)
			{
				maxWidth = curWidth;
			}

			width = maxWidth;
			height = maxHeight;
		}
	}
}
	
void
CDisplayRegion::UpdateOrigins(
	void)
{
	if(parent != NULL)
	{
		SDisplayRect const&	parentRect = parent->curRect;

		if(placement.GetPlace() == ePlace_Outside)
		{
			switch(placement.GetOutsideSide())
			{
				case eOutside_SideTop:
				case eOutside_SideBottom:
					
					if(placement.GetOutsideSide() == eOutside_SideTop)
					{
						curRect.topLeft.y = parentRect.topLeft.y - height;
						curRect.bottomRight.y = parentRect.topLeft.y;
					}
					else
					{
						curRect.topLeft.y = parentRect.bottomRight.y;
						curRect.bottomRight.y = parentRect.bottomRight.y + height;
					}

					switch(placement.GetOutsideAlign())
					{
						case eOutside_AlignLeft:
							curRect.topLeft.x = parentRect.topLeft.x;
							curRect.bottomRight.x = parentRect.topLeft.x + width;
							break;

						case eOutside_AlignCenter:
							curRect.topLeft.x = parentRect.topLeft.x + ((parentRect.GetWidth() - width + 1) >> 1);
							curRect.bottomRight.x = curRect.topLeft.x + width;
							break;

						case eOutside_AlignRight:
							curRect.topLeft.x = parentRect.bottomRight.x - width;
							curRect.bottomRight.x = parentRect.bottomRight.x;
							break;

					}

					break;

				case eOutside_SideLeft:
				case eOutside_SideRight:
					if(placement.GetOutsideSide() == eOutside_SideLeft)
					{
						curRect.topLeft.x = parentRect.topLeft.x - width;
						curRect.bottomRight.x = parentRect.topLeft.x;
					}
					else
					{
						curRect.topLeft.x = parentRect.bottomRight.x;
						curRect.bottomRight.x = parentRect.bottomRight.x + width;
					}

					switch(placement.GetOutsideAlign())
					{
						case eOutside_AlignTop:
							curRect.topLeft.y = parentRect.topLeft.y;
							curRect.bottomRight.y = parentRect.topLeft.y + height;
							break;

						case eOutside_AlignCenter:
							curRect.topLeft.y = parentRect.topLeft.y + ((parentRect.GetHeight() - height + 1) >> 1);
							curRect.bottomRight.y = curRect.topLeft.y + height;
							break;

						case eOutside_AlignBottom:
							curRect.topLeft.y = parentRect.bottomRight.y - height;
							curRect.bottomRight.y = parentRect.bottomRight.y;
							break;

					}
					break;
			}
		}
		else
		{
			switch(placement.GetInsideHoriz())
			{
				case eInside_Horiz_Left:
					curRect.topLeft.x = parentRect.topLeft.x;
					curRect.bottomRight.x = parentRect.topLeft.x + width;
					break;

				case eInside_Horiz_Center:
					curRect.topLeft.x = parentRect.topLeft.x + ((parentRect.GetWidth() - width + 1) >> 1);
					curRect.bottomRight.x = curRect.topLeft.x + width;
					break;

				case eInside_Horiz_Right:
					curRect.topLeft.x = parentRect.bottomRight.x - width;
					curRect.bottomRight.x = parentRect.bottomRight.x;
					break;

			}

			switch(placement.GetInsideVert())
			{
				case eInside_Vert_Top:
					curRect.topLeft.y = parentRect.topLeft.y;
					curRect.bottomRight.y = parentRect.topLeft.y + height;
					break;

				case eInside_Vert_Center:
					curRect.topLeft.y = parentRect.topLeft.y + ((parentRect.GetHeight() - height + 1) >> 1);
					curRect.bottomRight.y = curRect.topLeft.y + height;
					break;

				case eInside_Vert_Bottom:
					curRect.topLeft.y = parentRect.bottomRight.y - height;
					curRect.bottomRight.y = parentRect.bottomRight.y;
					break;

			}
		}
	}
	CDisplayRegion*	curRegion = firstChild;

	while(curRegion != NULL)
	{
		curRegion->UpdateOrigins();
		curRegion = curRegion->nextChild;
	}
}

void
CDisplayRegion::EraseOldRegions(
	void)
{
	if(firstChild == NULL)
	{
		if(curRect != oldRect)
		{
			if(curRect && oldRect)
			{
				if(curRect.topLeft.x > oldRect.topLeft.x)
				{
					// the left side of the new rect has moved to the right from the left side of the previous rect
					gDisplayModule->GetDisplayDriver()->FillRect(SDisplayRect(oldRect.topLeft, SDisplayPoint(curRect.topLeft.x, oldRect.bottomRight.y)), SDisplayColor(0, 0, 0));
				}
				if(curRect.bottomRight.x < oldRect.bottomRight.x)
				{
					// the right side of the new rect has moved to the left from the right side of the previous rect
					gDisplayModule->GetDisplayDriver()->FillRect(SDisplayRect(SDisplayPoint(curRect.bottomRight.x, oldRect.topLeft.y), oldRect.bottomRight), SDisplayColor(0, 0, 0));
				}
				if(curRect.topLeft.y > oldRect.topLeft.y)
				{
					// the top side of the new rect has moved below the top side of the old rect
					gDisplayModule->GetDisplayDriver()->FillRect(SDisplayRect(oldRect.topLeft, SDisplayPoint(oldRect.bottomRight.x, curRect.bottomRight.y)), SDisplayColor(0, 0, 0));
				}
				if(curRect.bottomRight.y < oldRect.bottomRight.y)
				{
					// the bottom side of the new rect has moved above the top side of the previous rect
					gDisplayModule->GetDisplayDriver()->FillRect(SDisplayRect(SDisplayPoint(oldRect.topLeft.x, curRect.bottomRight.y), oldRect.bottomRight), SDisplayColor(0, 0, 0));
				}

				// Erase the borders
				#if 0
				if(borderLeft > 0)
				{
					gDisplayModule->GetDisplayDriver()->FillRect(SDisplayRect(SDisplayPoint(curRect.topLeft.x, curRect.topLeft.y), SDisplayPoint(curRect.topLeft.x + borderLeft, curRect.bottomRight.y)), SDisplayColor(0, 0, 0));
				}
				if(borderTop > 0)
				{
					gDisplayModule->GetDisplayDriver()->FillRect(SDisplayRect(SDisplayPoint(curRect.topLeft.x, curRect.topLeft.y), SDisplayPoint(curRect.bottomRight.x, curRect.topLeft.y + borderRight)), SDisplayColor(0, 0, 0));
				}
				if(borderRight > 0)
				{
					gDisplayModule->GetDisplayDriver()->FillRect(SDisplayRect(SDisplayPoint(curRect.bottomRight.x - borderRight, curRect.topLeft.y), SDisplayPoint(curRect.bottomRight.x, curRect.bottomRight.y)), SDisplayColor(0, 0, 0));
				}
				if(borderBottom > 0)
				{
					gDisplayModule->GetDisplayDriver()->FillRect(SDisplayRect(SDisplayPoint(curRect.topLeft.x, curRect.bottomRight.y - borderBottom), SDisplayPoint(curRect.bottomRight.x, curRect.bottomRight.y)), SDisplayColor(0, 0, 0));
				}
				#endif
			}
			else
			{
				// erase the whole rect
				gDisplayModule->GetDisplayDriver()->FillRect(oldRect, SDisplayColor(0, 0, 0));
			}
		}
	}
	else
	{
		CDisplayRegion*	curRegion = firstChild;

		while(curRegion != NULL)
		{
			curRegion->EraseOldRegions();
			curRegion = curRegion->nextChild;
		}
	}
}

void
CDisplayRegion::Draw(
	void)
{
	CDisplayRegion*	curRegion = firstChild;

	// update all of the children
	while(curRegion != NULL)
	{
		curRegion->Draw();
		curRegion = curRegion->nextChild;
	}

	#if 1
	if(parent != NULL)
	{
		gDisplayModule->GetDisplayDriver()->DrawRect(curRect, SDisplayColor(255, 255, 255));
	}
	#endif
}

void
CDisplayRegion::SumRegionList(
	int16_t&		outWidth,
	int16_t&		outHeight,
	SPlacement		inPlacement,
	CDisplayRegion*	inList)
{
	outWidth = 0;
	outHeight = 0;

	CDisplayRegion*	curRegion = firstChild;

	while(curRegion != NULL)
	{
		if(curRegion->placement.Match(inPlacement))
		{
			outWidth += curRegion->width;
			outHeight += curRegion->height;
		}

		curRegion = curRegion->nextChild;
	}
}

CDisplayRegion_Text::CDisplayRegion_Text(
	SPlacement			inPlacement,
	CDisplayRegion*		inParent,
	SDisplayColor		inFGColor,
	SDisplayColor		inBGColor,
	SFontData const&	inFont,
	char const*			inFormat,
	...)
	:
	CDisplayRegion(inPlacement, inParent),
	text(NULL),
	dirty(true),
	fgColor(inFGColor),
	bgColor(inBGColor),
	font(&inFont)
{
	char buffer[256];

	va_list	varArgs;
	va_start(varArgs, inFormat);
	vsnprintf(buffer, sizeof(buffer) - 1, inFormat, varArgs);
	buffer[sizeof(buffer) - 1] = 0;	// Ensure valid string
	va_end(varArgs);

	int	newStrLen = strlen(buffer);
	text = gDisplayModule->ReallocString(text, newStrLen + 1);
	MReturnOnError(text == NULL);
	strcpy(text, buffer);

	fixedSize = true;
}
	
CDisplayRegion_Text::~CDisplayRegion_Text(
	)
{
	if(text != NULL)
	{
		gDisplayModule->FreeString(text);
	}
}

void
CDisplayRegion_Text::printf(
	char const*	inFormat,
	...)
{
	char buffer[256];

	va_list	varArgs;
	va_start(varArgs, inFormat);
	vsnprintf(buffer, sizeof(buffer) - 1, inFormat, varArgs);
	buffer[sizeof(buffer) - 1] = 0;	// Ensure valid string
	va_end(varArgs);

	int	newStrLen = strlen(buffer);
	text = gDisplayModule->ReallocString(text, newStrLen + 1);

	MReturnOnError(text == NULL);

	strcpy(text, buffer);

	dirty = true;
}
	
void
CDisplayRegion_Text::SetTextAlignment(
	uint16_t	inAlignmentFlags)
{

}

void
CDisplayRegion_Text::SetTextFont(
	SFontData const&	inFont)
{
	font = &inFont;

	dirty = true;
}

void
CDisplayRegion_Text::SetTextColor(
	SDisplayColor const&	inFGColor,
	SDisplayColor const&	inBGColor)
{
	dirty = true;
	fgColor = inFGColor;
	bgColor = inBGColor;
}

void
CDisplayRegion_Text::Draw(
	void)
{
	gDisplayModule->DrawText(text, SDisplayRect(curRect.topLeft.x + borderLeft, curRect.topLeft.y + borderTop, width - borderLeft - borderRight, height - borderTop - borderBottom), font, fgColor, bgColor);
	dirty = false;

	CDisplayRegion*	curRegion = firstChild;

	CDisplayRegion::Draw();
}

void
CDisplayRegion_Text::UpdateDimensions(
	void)
{
	if(text != NULL && font != NULL)
	{
		SDisplayPoint	textDim = gDisplayModule->GetTextDimensions(text, font);
		width = textDim.x + borderLeft + borderRight;
		height = textDim.y + borderTop + borderBottom;
	}

	CDisplayRegion::UpdateDimensions();
}

CModule_Display::CModule_Display(
	)
	:
	CModule("disp", 0, 0, NULL, 20000)
{
	displayDriver = NULL;
	touchDriver = NULL;
	gDisplayModule = this;
	touchDown = false;
	memset(stringTable, 0xFF, sizeof(stringTable));
}

void
CModule_Display::Update(
	uint32_t inDeltaTimeUS)
{
	if(touchDriver != NULL && topRegion != NULL)
	{
		bool			curTouchDown;
		curTouchDown = touchDriver->GetTouch(touchLoc);

		if(curTouchDown != touchDown)
		{
			if(curTouchDown)
			{
				topRegion->ProcessTouch(eTouchEvent_Down, touchLoc);
				touchDown = true;
			}
			else if(touchDown)
			{
				topRegion->ProcessTouch(eTouchEvent_Up, touchLoc);
				touchDown = false;
			}
		}
	}
}
	
int16_t
CModule_Display::GetWidth(
	void)
{
	if(displayDriver != NULL)
	{
		return displayDriver->GetWidth();
	}
	return 0;
}
	
int16_t
CModule_Display::GetHeight(
	void)
{
	if(displayDriver != NULL)
	{
		return displayDriver->GetHeight();
	}
	return 0;
}

void
CModule_Display::SetDisplayDriver(
	IDisplayDriver*	inDisplayDriver)
{
	displayDriver = inDisplayDriver;
	topRegion = new CDisplayRegion(SDisplayRect(0, 0, displayDriver->GetWidth(), displayDriver->GetHeight()), NULL);
}

void
CModule_Display::SetTouchscreenDriver(
	ITouchDriver*	inTouchDriver)
{
	touchDriver = inTouchDriver;
}

IDisplayDriver*
CModule_Display::GetDisplayDriver(
	void)
{
	return displayDriver;
}

CDisplayRegion*
CModule_Display::GetTopDisplayRegion(
	void)
{
	MReturnOnError(topRegion == NULL, NULL);
	return topRegion;
}

SDisplayPoint
CModule_Display::GetTextDimensions(
	char const*			inStr,
	SFontData const*	inFont)
{
	SDisplayPoint		result(0, 0);
	uint32_t		bitoffset;
	const uint8_t*	data;
	
	for(;;)
	{
		unsigned char c = *inStr++;

		if(c == 0)
		{
			break;
		}

		if (c >= inFont->index1_first && c <= inFont->index1_last) 
		{
			bitoffset = c - inFont->index1_first;
			bitoffset *= inFont->bits_index;
		} 
		else if (c >= inFont->index2_first && c <= inFont->index2_last)
		{
			bitoffset = c - inFont->index2_first + inFont->index1_last - inFont->index1_first + 1;
			bitoffset *= inFont->bits_index;
		} 
		else
		{
			return result;
		}

		uint32_t	index = fetchbits_unsigned(inFont->index, bitoffset, inFont->bits_index);
		//Serial.printf("  index =  %d\n", index);
		data = inFont->data + index;

		uint32_t encoding = fetchbits_unsigned(data, 0, 3);
		if (encoding != 0)
		{
			return result;
		}

		int16_t width = (int16_t)fetchbits_unsigned(data, 3, inFont->bits_width);
		bitoffset = inFont->bits_width + 3;

		int16_t	height = (int16_t)fetchbits_unsigned(data, bitoffset, inFont->bits_height);
		bitoffset += inFont->bits_height;

		int16_t xoffset = (int16_t)fetchbits_signed(data, bitoffset, inFont->bits_xoffset);
		bitoffset += inFont->bits_xoffset;

		int16_t yoffset = (int16_t)fetchbits_signed(data, bitoffset, inFont->bits_yoffset);
		bitoffset += inFont->bits_yoffset;

		int16_t delta = (int16_t)fetchbits_unsigned(data, bitoffset, inFont->bits_delta);

		result.x += delta;
		int16_t	curHeight = height + yoffset;
		if(curHeight > result.y)
		{
			result.y = curHeight;
		}

		if(*inStr == 0 && width + xoffset > delta)
		{
			result.x += width + xoffset - delta;
		}
	}

	return result;
}

int16_t
CModule_Display::DrawChar(
	char					inChar,
	SDisplayRect const&		inRect,
	SFontData const*		inFont,
	SDisplayColor const&	inForeground,
	SDisplayColor const&	inBackground)
{
	uint32_t		bitoffset;
	const uint8_t*	data;
	unsigned char	c = (unsigned char)inChar;

	//SystemMsg("c=%d", inChar);

	if (c >= inFont->index1_first && c <= inFont->index1_last) 
	{
		bitoffset = c - inFont->index1_first;
		bitoffset *= inFont->bits_index;
	} 
	else if (c >= inFont->index2_first && c <= inFont->index2_last)
	{
		bitoffset = c - inFont->index2_first + inFont->index1_last - inFont->index1_first + 1;
		bitoffset *= inFont->bits_index;
	} 
	else
	{
		return 0;
	}

	uint32_t	index = fetchbits_unsigned(inFont->index, bitoffset, inFont->bits_index);
	//Serial.printf("  index =  %d\n", index);
	data = inFont->data + index;

	uint32_t encoding = fetchbits_unsigned(data, 0, 3);
	if (encoding != 0)
	{
		return 0;
	}

	int16_t width = (int16_t)fetchbits_unsigned(data, 3, inFont->bits_width);
	bitoffset = inFont->bits_width + 3;

	int16_t height = (int16_t)fetchbits_unsigned(data, bitoffset, inFont->bits_height);
	bitoffset += inFont->bits_height;

	int16_t xoffset = (int16_t)fetchbits_signed(data, bitoffset, inFont->bits_xoffset);
	bitoffset += inFont->bits_xoffset;

	int16_t yoffset = (int16_t)fetchbits_signed(data, bitoffset, inFont->bits_yoffset);
	bitoffset += inFont->bits_yoffset;

	int16_t delta = (int16_t)fetchbits_unsigned(data, bitoffset, inFont->bits_delta);
	bitoffset += inFont->bits_delta;

	if(inRect.topLeft.x == inRect.bottomRight.x)
	{
		displayDriver->DrawContinuousStart(SDisplayRect(inRect.topLeft.x, inRect.topLeft.y, MMax(delta, width + abs(xoffset)), inRect.GetHeight()), inForeground, inBackground);
	}
	else
	{
		displayDriver->DrawContinuousStart(inRect, inForeground, inBackground);
	}

	int16_t	topLines = (int16_t)inFont->cap_height - height - yoffset;
	int16_t	bottomLines = (int16_t)inRect.GetHeight() - topLines - height;

	//SystemMsg("width=%d height=%d xoff=%d yoff=%d delta=%d line_space=%d cap_height=%d topLines=%d bottomLines=%d", width, height, xoffset, yoffset, delta, inFont->line_space, inFont->cap_height, topLines, bottomLines);

	for(int16_t i = 0; i < topLines; ++i)
	{
		displayDriver->DrawContinuousSolid(delta, false);
	}

	int16_t	trailingX = delta - width - xoffset;

	uint16_t	y = 0;
	while(y < height)
	{
		uint32_t b = fetchbit(data, bitoffset++);
		if (b == 0)
		{
			if(xoffset > 0)
			{
				displayDriver->DrawContinuousSolid(xoffset, false);
			}
			displayDriver->DrawContinuousBits(width, bitoffset & 0x7, data + (bitoffset >> 3));
			if(trailingX > 0)
			{
				displayDriver->DrawContinuousSolid(trailingX, false);
			}

			bitoffset += width;

			++y;
		} 
		else 
		{
			uint16_t n = (uint16_t)fetchbits_unsigned(data, bitoffset, 3) + 2;
			bitoffset += 3;

			for(uint16_t i = 0; i < n; ++i)
			{
				if(xoffset > 0)
				{
					displayDriver->DrawContinuousSolid(xoffset, false);
				}
				displayDriver->DrawContinuousBits(width, bitoffset & 0x7, data + (bitoffset >> 3));
				if(trailingX > 0)
				{
					displayDriver->DrawContinuousSolid(trailingX, false);
				}
			}
			y += n;
			bitoffset += width;
		}
	}
	for(int16_t i = 0; i < bottomLines; ++i)
	{
		displayDriver->DrawContinuousSolid(delta, false);
	}

	displayDriver->DrawContinuousEnd();

	return delta;
}

void
CModule_Display::DrawText(
	char const*				inStr,
	SDisplayRect const&		inRect,
	SFontData const*		inFont,
	SDisplayColor const&	inForeground,
	SDisplayColor const&	inBackground)
{
	SDisplayRect	curCharRect(inRect);

	curCharRect.bottomRight.x = curCharRect.topLeft.x;
	for(;;)
	{
		char c = *inStr++;

		if(c == 0)
		{
			break;
		}

		int16_t	charWidth = DrawChar(c, curCharRect, inFont, inForeground, inBackground);
		curCharRect.topLeft.x += charWidth;
		curCharRect.bottomRight.x += charWidth;
	}
}

void
CModule_Display::UpdateDisplay(
	void)
{
	topRegion->StartUpdate();
	topRegion->UpdateDimensions();
	topRegion->UpdateOrigins();
	displayDriver->BeginDrawing();
	topRegion->EraseOldRegions();
	topRegion->Draw();
	displayDriver->EndDrawing();
}

char*
CModule_Display::ReallocString(
	char*	inStr,
	int		inNewSize)
{
	int	strLen;
	
	if(inStr != NULL)
	{
		strLen = strlen(inStr) + 1;
	}
	else
	{
		strLen = 0;
		inStr = (char*)gDisplayModule->stringTable;
	}

	if(inNewSize <= strLen)
	{
		uint8_t*	cp = (uint8_t*)inStr + inNewSize;

		for(int i = 0; i < strLen - inNewSize; ++i)
		{
			*cp++ = 0xFF;
		}

		return inStr;
	}

	// See if there is room after the current string
	int	delta = inNewSize - strLen;
	uint8_t*	cp = (uint8_t*)inStr + strLen;
	uint8_t*	ep = gDisplayModule->stringTable + sizeof(gDisplayModule->stringTable);

	while(delta > 0 && cp < ep)
	{
		uint8_t c = *cp++;
		if(c != 0xFF)
		{
			break;
		}

		--delta;
	}

	if(delta == 0)
	{
		// there is room after the current string
		return inStr;
	}

	// Return the old string back to the string pool
	cp = (uint8_t*)inStr;
	uint8_t*	sep = (uint8_t*)inStr + strLen;

	while(cp < sep)
	{
		*cp++ = 0xFF;
	}

	cp = gDisplayModule->stringTable;

	// Now we need to find storage
	while(cp < ep)
	{
		uint8_t c = *cp++;

		if(c != 0xFF)
		{
			continue;
		}

		uint8_t*	sp = cp - 1;
		int			count = inNewSize;

		while(count > 0 && cp < ep)
		{
			uint8_t c = *cp++;
			if(c != 0xFF)
			{
				break;
			}

			--count;
		}

		if(count == 0)
		{
			// We have found room
			return (char*)sp;
		}
	}

	// Eventually compact memory here and then see if there is room
	#if 0
	cp = gDisplayModule->stringTable;

	char*	fp = cp;

	for(;;)
	{
		int	strLen = str(cp) + 1;

		cp += strLen;

		if(cp >= ep)
		{
			break;
		}

		if(*cp != 0xFF)
		{
			continue;
		}

		char*	sp = cp;

		while(sp < ep && *cp == 0xFF)
		{
			++cp;
		}

		if(sp >= ep)
		{
			cp = sp;
			break;
		}

		CDisplayRegion_Text*	curRegion = FindRegion(cp);

		if(curRegion == NULL)
		{
			return NULL;
		}

		memmove(sp, cp, strlen(cp) + 1);

	}
	#endif

	return NULL;
}

void
CModule_Display::FreeString(
	char*	inStr)
{
	unsigned char*	cp = (unsigned char*)inStr;
		
	for(;;)
	{
		unsigned char c = *cp;

		*cp++ = 0xFF;

		if(c == 0)
		{
			break;
		}
	}
}

#if 0
void
TestDisplay(
	void)
{
	IDisplayDriver*	displayDriver = CreateILI9341Driver(eDisplayOrientation_LandscapeUpsideDown, eDisplay_CS, eDisplay_DC, eSPI_MOSI, eSPI_SCLK, eSPI_MISO);

	gDisplayModule->SetDisplayDriver(displayDriver);

	displayDriver->BeginDrawing();
	displayDriver->FillScreen(SDisplayColor(0, 0, 0));
	#if 0
	//gDisplayModule->DrawText("$", SDisplayPoint(10, 10), &Arial_14, SDisplayColor(255, 255, 255), SDisplayColor(0, 0, 0));
	gDisplayModule->DrawText("abcdefghijklmnopqrstuvwxyz", SDisplayPoint(10, 10), &Arial_14, SDisplayColor(255, 255, 255), SDisplayColor(0, 0, 0));
	gDisplayModule->DrawText("ABCDEFGHIJKLMNOPQRSTUVWXYZ", SDisplayPoint(10, 30), &Arial_14, SDisplayColor(255, 255, 255), SDisplayColor(0, 0, 0));
	gDisplayModule->DrawText("0123456789", SDisplayPoint(10, 50), &Arial_14, SDisplayColor(255, 255, 255), SDisplayColor(0, 0, 0));
	gDisplayModule->DrawText("`~!@#$%^&*", SDisplayPoint(10, 70), &Arial_14, SDisplayColor(255, 255, 255), SDisplayColor(0, 0, 0)); // %^&*
	gDisplayModule->DrawText("*()_-+=[]{}\\|/?.>,<", SDisplayPoint(10, 90), &Arial_14, SDisplayColor(255, 255, 255), SDisplayColor(0, 0, 0));
	return;
	#endif
	displayDriver->EndDrawing();

	// Build up the display graph
	CDisplayRegion*	topRegion = gDisplayModule->GetTopDisplayRegion();

	CDisplayRegion_Text*	curTimeRegionTL = new CDisplayRegion_Text(SPlacement::Inside(eInside_Horiz_Left, eInside_Vert_Top), topRegion, SDisplayColor(255, 255, 255), SDisplayColor(0, 0, 0), Arial_14, "TopLeft");
	CDisplayRegion_Text*	curTimeRegionBL = new CDisplayRegion_Text(SPlacement::Inside(eInside_Horiz_Left, eInside_Vert_Bottom), topRegion, SDisplayColor(255, 255, 255), SDisplayColor(0, 0, 0), Arial_14, "BottomLeft");
	CDisplayRegion_Text*	curTimeRegionTR = new CDisplayRegion_Text(SPlacement::Inside(eInside_Horiz_Right, eInside_Vert_Top), topRegion, SDisplayColor(255, 255, 255), SDisplayColor(0, 0, 0), Arial_14, "TopRight");
	CDisplayRegion_Text*	curTimeRegionTB = new CDisplayRegion_Text(SPlacement::Inside(eInside_Horiz_Right, eInside_Vert_Bottom), topRegion, SDisplayColor(255, 255, 255), SDisplayColor(0, 0, 0), Arial_14, "BottomRight");
	CDisplayRegion*			centerRegion = new CDisplayRegion(SPlacement::Inside(eInside_Horiz_Center, eInside_Vert_Center), topRegion);
	CDisplayRegion_Text*	curTimeRegionCCTop = new CDisplayRegion_Text(SPlacement::Inside(eInside_Horiz_Center, eInside_Vert_Top), centerRegion, SDisplayColor(255, 255, 255), SDisplayColor(0, 0, 0), Arial_14, "Top");
	CDisplayRegion_Text*	curTimeRegionCCMiddle = new CDisplayRegion_Text(SPlacement::Inside(eInside_Horiz_Center, eInside_Vert_Center), centerRegion, SDisplayColor(255, 255, 255), SDisplayColor(0, 0, 0), Arial_14, "Middle");
	CDisplayRegion_Text*	curTimeRegionCCBottom = new CDisplayRegion_Text(SPlacement::Inside(eInside_Horiz_Center, eInside_Vert_Bottom), centerRegion, SDisplayColor(255, 255, 255), SDisplayColor(0, 0, 0), Arial_14, "Bottom");
	CDisplayRegion_Text*	curTimeRegionOutsideLeftTop = new CDisplayRegion_Text(SPlacement::Outside(eOutside_SideLeft, eOutside_AlignTop), centerRegion, SDisplayColor(255, 255, 255), SDisplayColor(0, 0, 0), Arial_8, "LeftTop");
	CDisplayRegion_Text*	curTimeRegionOutsideLeftCenter = new CDisplayRegion_Text(SPlacement::Outside(eOutside_SideLeft, eOutside_AlignCenter), centerRegion, SDisplayColor(255, 255, 255), SDisplayColor(0, 0, 0), Arial_8, "LeftCenter");
	CDisplayRegion_Text*	curTimeRegionOutsideLeftBottom = new CDisplayRegion_Text(SPlacement::Outside(eOutside_SideLeft, eOutside_AlignBottom), centerRegion, SDisplayColor(255, 255, 255), SDisplayColor(0, 0, 0), Arial_8, "LeftBottom");
	CDisplayRegion_Text*	curTimeRegionOutsideRightTop = new CDisplayRegion_Text(SPlacement::Outside(eOutside_SideRight, eOutside_AlignTop), centerRegion, SDisplayColor(255, 255, 255), SDisplayColor(0, 0, 0), Arial_8, "RightTop");
	CDisplayRegion_Text*	curTimeRegionOutsideRightCenter = new CDisplayRegion_Text(SPlacement::Outside(eOutside_SideRight, eOutside_AlignCenter), centerRegion, SDisplayColor(255, 255, 255), SDisplayColor(0, 0, 0), Arial_8, "RightCenter");
	CDisplayRegion_Text*	curTimeRegionOutsideRightBottom = new CDisplayRegion_Text(SPlacement::Outside(eOutside_SideRight, eOutside_AlignBottom), centerRegion, SDisplayColor(255, 255, 255), SDisplayColor(0, 0, 0), Arial_8, "RightBottom");
	CDisplayRegion_Text*	curTimeRegionOutsideTopLeft = new CDisplayRegion_Text(SPlacement::Outside(eOutside_SideTop, eOutside_AlignLeft), centerRegion, SDisplayColor(255, 255, 255), SDisplayColor(0, 0, 0), Arial_8, "TL");
	CDisplayRegion_Text*	curTimeRegionOutsideTopCenter = new CDisplayRegion_Text(SPlacement::Outside(eOutside_SideTop, eOutside_AlignCenter), centerRegion, SDisplayColor(255, 255, 255), SDisplayColor(0, 0, 0), Arial_8, "TC");
	CDisplayRegion_Text*	curTimeRegionOutsideTopRight = new CDisplayRegion_Text(SPlacement::Outside(eOutside_SideTop, eOutside_AlignRight), centerRegion, SDisplayColor(255, 255, 255), SDisplayColor(0, 0, 0), Arial_8, "TR");
	CDisplayRegion_Text*	curTimeRegionOutsideBottomLeft = new CDisplayRegion_Text(SPlacement::Outside(eOutside_SideBottom, eOutside_AlignLeft), centerRegion, SDisplayColor(255, 255, 255), SDisplayColor(0, 0, 0), Arial_8, "BL");
	CDisplayRegion_Text*	curTimeRegionOutsideBottomCenter = new CDisplayRegion_Text(SPlacement::Outside(eOutside_SideBottom, eOutside_AlignCenter), centerRegion, SDisplayColor(255, 255, 255), SDisplayColor(0, 0, 0), Arial_8, "BC");
	CDisplayRegion_Text*	curTimeRegionOutsideBottomRight = new CDisplayRegion_Text(SPlacement::Outside(eOutside_SideBottom, eOutside_AlignRight), centerRegion, SDisplayColor(255, 255, 255), SDisplayColor(0, 0, 0), Arial_8, "BR");

	gDisplayModule->UpdateDisplay();

	curTimeRegionTL->SetTextFont(Arial_10);
	curTimeRegionBL->SetTextFont(Arial_10);
	curTimeRegionTR->SetTextFont(Arial_10);
	curTimeRegionTB->SetTextFont(Arial_10);
	curTimeRegionCCTop->SetTextFont(Arial_24);
	curTimeRegionCCMiddle->SetTextFont(Arial_24);
	curTimeRegionCCBottom->SetTextFont(Arial_24);

	gDisplayModule->UpdateDisplay();

}
#endif