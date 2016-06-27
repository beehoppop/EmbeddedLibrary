

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


	Parts based on AdaFruit ILI9341 library:

	This is our library for the Adafruit ILI9341 Breakout and Shield
	----> http://www.adafruit.com/products/1651

	Check out the links above for our tutorials and wiring diagrams
	These displays use SPI to communicate, 4 or 5 pins are required to
	interface (RST is optional)
	Adafruit invests time and resources providing this open source code,
	please support Adafruit and open-source hardware by purchasing
	products from Adafruit!

	Written by Limor Fried/Ladyada for Adafruit Industries.
	MIT license, all text above must be included in any redistribution

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

MModuleImplementation_Start(
	CDisplayDriver_ILI9341,
	EDisplayOrientation	inDisplayOrientation,
	uint8_t	inCS,
	uint8_t	inDC,
	uint8_t	inMOSI,
	uint8_t	inClk,
	uint8_t	inMISO)
MModuleImplementation(CDisplayDriver_ILI9341, inDisplayOrientation, inCS, inDC, inMOSI, inClk, inMISO)

SPlacement
SPlacement::Outside(
	uint8_t	inSide,
	uint8_t	inAlignment)
{
	return SPlacement(ePlacementType_Outside, inSide, inAlignment);
}

SPlacement
SPlacement::Inside(
	uint8_t	inHoriz,
	uint8_t	inVert)
{
	return SPlacement(ePlacementType_Inside, inHoriz, inVert);
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
	CDisplayRegion*	inParent,
	SPlacement		inPlacement)
	:
	parent(NULL),
	firstChild(NULL),
	nextChild(NULL),
	placement(inPlacement),
	curRect(0, 0, 0, 0),
	oldRect(0, 0, 0, 0),
	borderLeft(0),
	borderRight(0),
	borderTop(0),
	borderBottom(0)
{
	touchObject = NULL;

	if(inParent != NULL)
	{
		AddToParent(inParent);
	}
}
	

int16_t
CDisplayRegion::GetWidth(
	void)
{
	return curRect.GetWidth();;
}

int16_t
CDisplayRegion::GetHeight(
	void)
{
	return curRect.GetHeight();
}

void
CDisplayRegion::AddToParent(
	CDisplayRegion*	inParent)
{
	MReturnOnError(parent != NULL);

	parent = inParent;
	nextChild = inParent->firstChild;
	inParent->firstChild = this;
}

void
CDisplayRegion::RemoveFromParent(
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
		MAssert(parent->firstChild != NULL);
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
	int8_t	inLeft,
	int8_t	inRight,
	int8_t	inTop,
	int8_t	inBottom)
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

	curRect.Clear();
	if(parent == NULL)
	{
		curRect.bottomRight.x = gDisplayModule->GetWidth();
		curRect.bottomRight.y = gDisplayModule->GetHeight();
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

		int16_t	curWidth;
		int16_t	curHeight;
		if(placement.GetPlacementType() == ePlacementType_Outside || placement.GetInsideHoriz() != eAlign_Horiz_Expand)
		{
			int16_t	maxWidth  = 0;

			SumRegionList(curWidth, curHeight, SPlacement::Inside(ePlacement_Any, eAlign_Vert_Top), firstChild);
			if(curWidth > maxWidth)
			{
				maxWidth = curWidth;
			}

			SumRegionList(curWidth, curHeight, SPlacement::Inside(ePlacement_Any, eAlign_Vert_Center), firstChild);
			if(curWidth > maxWidth)
			{
				maxWidth = curWidth;
			}

			SumRegionList(curWidth, curHeight, SPlacement::Inside(ePlacement_Any, eAlign_Vert_Bottom), firstChild);
			if(curWidth > maxWidth)
			{
				maxWidth = curWidth;
			}

			curRect.bottomRight.x = maxWidth + borderLeft + borderRight;
		}

		if(placement.GetPlacementType() == ePlacementType_Outside || placement.GetInsideVert() != eAlign_Vert_Expand)
		{
			int16_t	maxHeight = 0;
			SumRegionList(curWidth, curHeight, SPlacement::Inside(eAlign_Horiz_Left, ePlacement_Any), firstChild);
			if(curHeight > maxHeight)
			{
				maxHeight = curHeight;
			}

			SumRegionList(curWidth, curHeight, SPlacement::Inside(eAlign_Horiz_Center, ePlacement_Any), firstChild);
			if(curHeight > maxHeight)
			{
				maxHeight = curHeight;
			}

			SumRegionList(curWidth, curHeight, SPlacement::Inside(eAlign_Horiz_Right, ePlacement_Any), firstChild);
			if(curHeight > maxHeight)
			{
				maxHeight = curHeight;
			}

			curRect.bottomRight.y = maxHeight + borderTop + borderBottom;
		}
	}
}
	
void
CDisplayRegion::UpdateOrigins(
	void)
{
	CDisplayRegion*	curRegion = firstChild;

	while(curRegion != NULL)
	{
		curRegion->PlaceInRect(borderLeft, borderTop, borderRight, borderBottom, curRect);
		curRegion->UpdateOrigins();
		curRegion = curRegion->nextChild;
	}
}
	
void
CDisplayRegion::PlaceInRect(
	int8_t				inBorderLeft,
	int8_t				inBorderTop,
	int8_t				inBorderRight,
	int8_t				inBorderBottom,
	SDisplayRect const&	inRect)
{
	int16_t	offset;

	if(placement.GetPlacementType() == ePlacementType_Inside)
	{
		switch(placement.GetInsideHoriz())
		{
			case eAlign_Horiz_Left:
				offset = inRect.topLeft.x + inBorderLeft;
				curRect.topLeft.x += offset;
				curRect.bottomRight.x += offset;
				break;

			case eAlign_Horiz_Center:
				offset = inRect.topLeft.x + inBorderLeft + ((inRect.GetWidth() - inBorderLeft - inBorderRight - curRect.GetWidth()) >> 1);
				curRect.topLeft.x += offset;
				curRect.bottomRight.x += offset;
				break;

			case eAlign_Horiz_Right:
				offset = inRect.bottomRight.x - inBorderRight - curRect.GetWidth();
				curRect.topLeft.x += offset;
				curRect.bottomRight.x += offset;
				break;

			case eAlign_Horiz_Expand:
				curRect.topLeft.x = inRect.topLeft.x + inBorderLeft;
				curRect.bottomRight.x = inRect.bottomRight.x - inBorderRight;
				break;
		}

		switch(placement.GetInsideVert())
		{
			case eAlign_Vert_Top:
				offset = inRect.topLeft.y + inBorderTop;
				curRect.topLeft.y += offset;
				curRect.bottomRight.y += offset;
				break;

			case eAlign_Vert_Center:
				offset = inRect.topLeft.y + inBorderTop + ((inRect.GetHeight() - inBorderTop - inBorderBottom - curRect.GetHeight()) >> 1);
				curRect.topLeft.y += offset;
				curRect.bottomRight.y += offset;
				break;

			case eAlign_Vert_Bottom:
				offset = inRect.bottomRight.y - inBorderBottom - curRect.GetHeight();
				curRect.topLeft.y += offset;
				curRect.bottomRight.y += offset;
				break;

			case eAlign_Vert_Expand:
				curRect.topLeft.y = inRect.topLeft.y + inBorderTop;
				curRect.bottomRight.y = inRect.bottomRight.y - inBorderBottom;
				break;
		}
	}
	else
	{
		switch(placement.GetOutsideSide())
		{
			case eAlign_Side_Top:
			case eAlign_Side_Bottom:
					
				if(placement.GetOutsideSide() == eAlign_Side_Top)
				{
					offset = inRect.topLeft.y - curRect.GetHeight();
				}
				else
				{
					offset = inRect.bottomRight.y;
				}

				curRect.topLeft.y += offset; 
				curRect.bottomRight.y += offset; 

				switch(placement.GetOutsideAlign())
				{
					case eAlign_Horiz_Left:
						offset = inRect.topLeft.x;
						curRect.topLeft.x += offset;
						curRect.bottomRight.x += offset;
						break;

					case eAlign_Horiz_Center:
						offset = inRect.topLeft.x + ((inRect.GetWidth() - curRect.GetWidth()) >> 1);
						curRect.topLeft.x += offset;
						curRect.bottomRight.x += offset;
						break;

					case eAlign_Horiz_Right:
						offset = inRect.bottomRight.x - curRect.GetWidth();
						curRect.topLeft.x += offset;
						curRect.bottomRight.x += offset;
						break;

					case eAlign_Horiz_Expand:
						curRect.topLeft.x = inRect.topLeft.x;
						curRect.bottomRight.x = inRect.bottomRight.x;
						break;
				}

				break;

			case eAlign_Side_Left:
			case eAlign_Side_Right:
				if(placement.GetOutsideSide() == eAlign_Side_Left)
				{
					offset = inRect.topLeft.x - curRect.GetWidth();
				}
				else
				{
					offset = inRect.bottomRight.x;
				}

				curRect.topLeft.x += offset; 
				curRect.bottomRight.x += offset; 

				switch(placement.GetOutsideAlign())
				{
					case eAlign_Vert_Top:
						offset = inRect.topLeft.y;
						curRect.topLeft.y += offset;
						curRect.bottomRight.y += offset;
						break;

					case eAlign_Vert_Center:
						offset = inRect.topLeft.y + ((inRect.GetHeight() - curRect.GetHeight()) >> 1);
						curRect.topLeft.y += offset;
						curRect.bottomRight.y += offset;
						break;

					case eAlign_Vert_Bottom:
						offset = inRect.bottomRight.y - curRect.GetHeight();
						curRect.topLeft.y += offset;
						curRect.bottomRight.y += offset;
						break;

					case eAlign_Vert_Expand:
						curRect.topLeft.y = inRect.topLeft.y;
						curRect.bottomRight.y = inRect.bottomRight.y;
						break;
				}
				break;
		}
	}
}

void
CDisplayRegion::EraseOldRegions(
	void)
{
	gDisplayModule->FillRectDiff(curRect, oldRect, SDisplayColor(0, 0, 0));

	if(borderRight > 0)
	{
		//gDisplayModule->FillRect(
	}

	CDisplayRegion*	curRegion = firstChild;
	while(curRegion != NULL)
	{
		curRegion->EraseOldRegions();
		curRegion = curRegion->nextChild;
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
		// Draw this region
		curRegion->Draw();

		// Advance to next child
		curRegion = curRegion->nextChild;
	}

	#if 0
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
			outWidth += curRegion->curRect.GetWidth();
			outHeight += curRegion->GetHeight();
		}

		curRegion = curRegion->nextChild;
	}
}

CDisplayRegion_Grid::CDisplayRegion_Grid(
	CDisplayRegion*	inParent,
	SPlacement		inPlacement,
	bool			inEqualCellDimensions,
	uint8_t			inNumRows,
	uint8_t			inNumCols)
	:
	CDisplayRegion(inParent, inPlacement),
	equalCellPlacement(inEqualCellDimensions),
	numRows(inNumRows),
	numCols(inNumCols)
{
	cellMatrix = (CDisplayRegion**)malloc(numRows * numCols * sizeof(CDisplayRegion*));
	MAssert(cellMatrix != NULL);
	memset(cellMatrix, 0, numRows * numCols * sizeof(CDisplayRegion*));

	rowOffsets = (int16_t*)malloc((numRows + 1) * sizeof(int16_t));
	MAssert(rowOffsets != NULL);

	colOffsets = (int16_t*)malloc((numCols + 1) * sizeof(int16_t));
	MAssert(colOffsets != NULL);

}

void
CDisplayRegion_Grid::SetCellRegion(
	uint8_t			inRow,
	uint8_t			inCol,
	CDisplayRegion*	inDisplayRegion)
{
	MAssert(inRow < numRows && inCol < numCols);
	cellMatrix[inRow * numCols + inCol] = inDisplayRegion;
}

void
CDisplayRegion_Grid::UpdateDimensions(
	void)
{
	CDisplayRegion*	curRegion = firstChild;

	while(curRegion != NULL)
	{
		curRegion->UpdateDimensions();

		curRegion = curRegion->nextChild;
	}
	
	memset(rowOffsets, 0, (numRows + 1) * sizeof(int16_t));
	memset(colOffsets, 0, (numCols + 1) * sizeof(int16_t));
	for(uint8_t	y = 0; y < numRows; ++y)
	{
		for(uint8_t x = 0; x < numCols; ++x)
		{
			CDisplayRegion*	curRegion = cellMatrix[y * numCols + x];

			if(curRegion != NULL)
			{
				int16_t	curRegionWidth = curRegion->curRect.GetWidth();
				int16_t	curRegionHeight = curRegion->curRect.GetHeight();

				if(curRegionWidth > colOffsets[x])
				{
					colOffsets[x] = curRegionWidth;
				}

				if(curRegionHeight > rowOffsets[y])
				{
					rowOffsets[y] = curRegionHeight;
				}
			}
		}
	}

	int16_t	gridHeightTotal = 0;
	int16_t	gridWidthTotal = 0;
	for(int i = 0; i < numCols; ++i)
	{
		int16_t	curWidth = colOffsets[i];
		colOffsets[i] = gridWidthTotal;
		gridWidthTotal += curWidth;
	}
	colOffsets[numCols] = gridWidthTotal;

	for(int i = 0; i < numRows; ++i)
	{
		int16_t	curHeight = rowOffsets[i];
		rowOffsets[i] = gridHeightTotal;
		gridHeightTotal += curHeight;
	}
	rowOffsets[numRows] = gridHeightTotal;
	
	curRect.bottomRight.x = gridWidthTotal + borderLeft + borderRight;
	curRect.bottomRight.y = gridHeightTotal + borderTop + borderBottom;
}

void
CDisplayRegion_Grid::UpdateOrigins(
	void)
{
	CDisplayRegion*	curRegion;

	if(equalCellPlacement)
	{
		int16_t	gridWidth = curRect.GetWidth() - borderLeft - borderRight;
		int16_t	gridHeight = curRect.GetHeight() - borderTop - borderBottom;

		for(uint8_t x = 0; x <= numCols; ++x)
		{
			colOffsets[x] = x * gridWidth / numCols;
		}

		for(uint8_t	y = 0; y <= numRows; ++y)
		{
			rowOffsets[y] = y * gridHeight / numRows;
		}
	}

	int16_t	gridLeft = curRect.topLeft.x + borderLeft;
	int16_t	gridTop = curRect.topLeft.y + borderTop;
	for(uint8_t	y = 0; y < numRows; ++y)
	{
		for(uint8_t x = 0; x < numCols; ++x)
		{
			curRegion = cellMatrix[y * numCols + x];

			if(curRegion != NULL)
			{
				SDisplayRect	rect(SDisplayPoint(gridLeft + colOffsets[x], gridTop + rowOffsets[y]), SDisplayPoint(gridLeft + colOffsets[x + 1], gridTop + rowOffsets[y + 1]));
				curRegion->PlaceInRect(0, 0, 0, 0, rect);
			}
		}
	}

	curRegion = firstChild;

	while(curRegion != NULL)
	{
		curRegion->UpdateOrigins();

		curRegion = curRegion->nextChild;
	}
}

CDisplayRegion_Text::CDisplayRegion_Text(
	CDisplayRegion*		inParent,
	SPlacement			inPlacement,
	SDisplayColor		inFGColor,
	SDisplayColor		inBGColor,
	SFontData const&	inFont,
	char const*			inFormat,
	...)
	:
	CDisplayRegion(inParent, inPlacement),
	text(NULL),
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

	horizAlign = eAlign_Horiz_Left;
	vertAlign = eAlign_Vert_Top;
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
}

void
CDisplayRegion_Text::SetTextFont(
	SFontData const&	inFont)
{
	font = &inFont;
}

void
CDisplayRegion_Text::SetTextColor(
	SDisplayColor const&	inFGColor,
	SDisplayColor const&	inBGColor)
{
	fgColor = inFGColor;
	bgColor = inBGColor;
}
	
void
CDisplayRegion_Text::SetTextAlignment(
	uint8_t	inHorizAlignment,
	uint8_t	inVertAlignment)
{
	horizAlign = inHorizAlignment;
	vertAlign = inVertAlignment;
}

void
CDisplayRegion_Text::Draw(
	void)
{
	gDisplayModule->DrawText(text, SDisplayRect(SDisplayPoint(curRect.topLeft.x + borderLeft, curRect.topLeft.y + borderTop), SDisplayPoint(curRect.bottomRight.x - borderRight, curRect.bottomRight.y - borderBottom)), font, fgColor, bgColor);

	CDisplayRegion::Draw();
}

void
CDisplayRegion_Text::UpdateDimensions(
	void)
{
	if(text != NULL && font != NULL)
	{
		curRect.bottomRight = gDisplayModule->GetTextDimensions(text, font);
		curRect.bottomRight.x += borderLeft + borderRight;
		curRect.bottomRight.y += borderTop + borderBottom;
	}

	CDisplayRegion*	curRegion = firstChild;

	// update all of the children
	while(curRegion != NULL)
	{
		curRegion->UpdateDimensions();
		curRegion = curRegion->nextChild;
	}
}

MModuleSingleton_ImplementationGlobal(CModule_Display, gDisplayModule)

CModule_Display::CModule_Display(
	)
	:
	CModule(0, 0, NULL, 20000)
{
	displayDriver = NULL;
	touchDriver = NULL;
	touchDown = false;
	topRegion = NULL;
	memset(stringTable, 0xFF, sizeof(stringTable));

	DoneIncluding();
}

void
CModule_Display::Update(
	uint32_t inDeltaTimeUS)
{
	if(touchDriver != NULL)
	{
		bool			curTouchDown;
		curTouchDown = touchDriver->GetTouch(touchLoc);

		if(curTouchDown != touchDown)
		{
			if(curTouchDown)
			{
				if(topRegion != NULL)
				{
					topRegion->ProcessTouch(eTouchEvent_Down, touchLoc);
				}
				touchDown = true;
			}
			else if(touchDown)
			{
				if(topRegion != NULL)
				{
					topRegion->ProcessTouch(eTouchEvent_Up, touchLoc);
				}

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
	topRegion = new CDisplayRegion(NULL, SPlacement::Inside(eAlign_Horiz_Expand, eAlign_Vert_Expand));
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
		int16_t	curHeight = height + abs(yoffset);
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
	if(displayDriver == NULL)
	{
		return;
	}

	topRegion->StartUpdate();
	topRegion->UpdateDimensions();
	topRegion->UpdateOrigins();
	displayDriver->BeginDrawing();
	topRegion->EraseOldRegions();
	topRegion->Draw();
	displayDriver->EndDrawing();
}

void
CModule_Display::FillRectDiff(
	SDisplayRect const&		inRectNew,
	SDisplayRect const&		inRectOld,
	SDisplayColor const&	inColor)
{
	if(inRectNew == inRectOld)
	{
		return; // Nothing to do
	}

	if(!(inRectNew && inRectOld))
	{
		// there is no intersection so just fill rect b
		displayDriver->FillRect(inRectOld, inColor);
		return;
	}

	if(inRectNew.topLeft.x > inRectOld.topLeft.x)
	{
		// the left side of the new rect has moved to the right from the left side of the previous rect
		gDisplayModule->GetDisplayDriver()->FillRect(SDisplayRect(inRectOld.topLeft, SDisplayPoint(inRectNew.topLeft.x, inRectOld.bottomRight.y)), inColor);
	}

	if(inRectNew.bottomRight.x < inRectOld.bottomRight.x)
	{
		// the right side of the new rect has moved to the left from the right side of the previous rect
		gDisplayModule->GetDisplayDriver()->FillRect(SDisplayRect(SDisplayPoint(inRectNew.bottomRight.x, inRectOld.topLeft.y), inRectOld.bottomRight), inColor);
	}

	if(inRectNew.topLeft.y > inRectOld.topLeft.y)
	{
		// the top side of the new rect has moved below the top side of the old rect
		gDisplayModule->GetDisplayDriver()->FillRect(SDisplayRect(inRectOld.topLeft, SDisplayPoint(inRectOld.bottomRight.x, inRectNew.topLeft.y)), inColor);
	}

	if(inRectNew.bottomRight.y < inRectOld.bottomRight.y)
	{
		// the bottom side of the new rect has moved above the top side of the previous rect
		gDisplayModule->GetDisplayDriver()->FillRect(SDisplayRect(SDisplayPoint(inRectOld.topLeft.x, inRectNew.bottomRight.y), inRectOld.bottomRight), inColor);
	}
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

	CDisplayRegion_Text*	curTimeRegionTL = new CDisplayRegion_Text(SPlacement::Inside(eAlign_Horiz_Left, eAlign_Vert_Top), topRegion, SDisplayColor(255, 255, 255), SDisplayColor(0, 0, 0), Arial_14, "TopLeft");
	CDisplayRegion_Text*	curTimeRegionBL = new CDisplayRegion_Text(SPlacement::Inside(eAlign_Horiz_Left, eAlign_Vert_Bottom), topRegion, SDisplayColor(255, 255, 255), SDisplayColor(0, 0, 0), Arial_14, "BottomLeft");
	CDisplayRegion_Text*	curTimeRegionTR = new CDisplayRegion_Text(SPlacement::Inside(eAlign_Horiz_Right, eAlign_Vert_Top), topRegion, SDisplayColor(255, 255, 255), SDisplayColor(0, 0, 0), Arial_14, "TopRight");
	CDisplayRegion_Text*	curTimeRegionTB = new CDisplayRegion_Text(SPlacement::Inside(eAlign_Horiz_Right, eAlign_Vert_Bottom), topRegion, SDisplayColor(255, 255, 255), SDisplayColor(0, 0, 0), Arial_14, "BottomRight");
	CDisplayRegion*			centerRegion = new CDisplayRegion(SPlacement::Inside(eAlign_Horiz_Center, eAlign_Vert_Center), topRegion);
	CDisplayRegion_Text*	curTimeRegionCCTop = new CDisplayRegion_Text(SPlacement::Inside(eAlign_Horiz_Center, eAlign_Vert_Top), centerRegion, SDisplayColor(255, 255, 255), SDisplayColor(0, 0, 0), Arial_14, "Top");
	CDisplayRegion_Text*	curTimeRegionCCMiddle = new CDisplayRegion_Text(SPlacement::Inside(eAlign_Horiz_Center, eAlign_Vert_Center), centerRegion, SDisplayColor(255, 255, 255), SDisplayColor(0, 0, 0), Arial_14, "Middle");
	CDisplayRegion_Text*	curTimeRegionCCBottom = new CDisplayRegion_Text(SPlacement::Inside(eAlign_Horiz_Center, eAlign_Vert_Bottom), centerRegion, SDisplayColor(255, 255, 255), SDisplayColor(0, 0, 0), Arial_14, "Bottom");
	CDisplayRegion_Text*	curTimeRegionOutsideLeftTop = new CDisplayRegion_Text(SPlacement::Outside(eAlign_Side_Left, eAlign_Vert_Top), centerRegion, SDisplayColor(255, 255, 255), SDisplayColor(0, 0, 0), Arial_8, "LeftTop");
	CDisplayRegion_Text*	curTimeRegionOutsideLeftCenter = new CDisplayRegion_Text(SPlacement::Outside(eAlign_Side_Left, eAlign_Vert_Center), centerRegion, SDisplayColor(255, 255, 255), SDisplayColor(0, 0, 0), Arial_8, "LeftCenter");
	CDisplayRegion_Text*	curTimeRegionOutsideLeftBottom = new CDisplayRegion_Text(SPlacement::Outside(eAlign_Side_Left, eAlign_Vert_Bottom), centerRegion, SDisplayColor(255, 255, 255), SDisplayColor(0, 0, 0), Arial_8, "LeftBottom");
	CDisplayRegion_Text*	curTimeRegionOutsideRightTop = new CDisplayRegion_Text(SPlacement::Outside(eAlign_Side_Right, eAlign_Vert_Top), centerRegion, SDisplayColor(255, 255, 255), SDisplayColor(0, 0, 0), Arial_8, "RightTop");
	CDisplayRegion_Text*	curTimeRegionOutsideRightCenter = new CDisplayRegion_Text(SPlacement::Outside(eAlign_Side_Right, eAlign_Vert_Center), centerRegion, SDisplayColor(255, 255, 255), SDisplayColor(0, 0, 0), Arial_8, "RightCenter");
	CDisplayRegion_Text*	curTimeRegionOutsideRightBottom = new CDisplayRegion_Text(SPlacement::Outside(eAlign_Side_Right, eAlign_Vert_Bottom), centerRegion, SDisplayColor(255, 255, 255), SDisplayColor(0, 0, 0), Arial_8, "RightBottom");
	CDisplayRegion_Text*	curTimeRegionOutsideTopLeft = new CDisplayRegion_Text(SPlacement::Outside(eAlign_Side_Top, eAlign_Horiz_Left), centerRegion, SDisplayColor(255, 255, 255), SDisplayColor(0, 0, 0), Arial_8, "TL");
	CDisplayRegion_Text*	curTimeRegionOutsideTopCenter = new CDisplayRegion_Text(SPlacement::Outside(eAlign_Side_Top, eAlign_Horiz_Center), centerRegion, SDisplayColor(255, 255, 255), SDisplayColor(0, 0, 0), Arial_8, "TC");
	CDisplayRegion_Text*	curTimeRegionOutsideTopRight = new CDisplayRegion_Text(SPlacement::Outside(eAlign_Side_Top, eAlign_Horiz_Right), centerRegion, SDisplayColor(255, 255, 255), SDisplayColor(0, 0, 0), Arial_8, "TR");
	CDisplayRegion_Text*	curTimeRegionOutsideBottomLeft = new CDisplayRegion_Text(SPlacement::Outside(eAlign_Side_Bottom, eAlign_Horiz_Left), centerRegion, SDisplayColor(255, 255, 255), SDisplayColor(0, 0, 0), Arial_8, "BL");
	CDisplayRegion_Text*	curTimeRegionOutsideBottomCenter = new CDisplayRegion_Text(SPlacement::Outside(eAlign_Side_Bottom, eAlign_Horiz_Center), centerRegion, SDisplayColor(255, 255, 255), SDisplayColor(0, 0, 0), Arial_8, "BC");
	CDisplayRegion_Text*	curTimeRegionOutsideBottomRight = new CDisplayRegion_Text(SPlacement::Outside(eAlign_Side_Bottom, eAlign_Horiz_Right), centerRegion, SDisplayColor(255, 255, 255), SDisplayColor(0, 0, 0), Arial_8, "BR");

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