#ifndef _EL_DISPLAY_H_
#define _EL_DISPLAY_H_
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

/*
	ABOUT

*/

#include <ELModule.h>
#include <ELUtilities.h>
#include <ELAssert.h>

#define MMakeColor(r, g, b) ((((r) & 0x1f) << 11) | (((g) & 0x3f) << 5) | ((b) & 0x1F))

class CModule_Display;

enum
{
	ePlacementType_Inside	= 0,		// Place the child box inside of the parent box
	ePlacementType_Outside	= 1,		// Place the child box outside of the parent box
	ePlacementType_Grid		= 2,		// Place the child box inside of the parent box as a grid

	eInside_Horiz_Left		= 0,		// Place the child box inside on the left side of the parent box
	eInside_Horiz_Center	= 1,		// Place the child box inside in the horiz center of the parent box
	eInside_Horiz_Right		= 2,		// Place the child box inside on the right side of the parent box

	eInside_Vert_Top		= 0,		// Place the child box inside on the top of the parent box
	eInside_Vert_Center		= 1,		// PLace the child box inside in the vertical center of the parent box
	eInside_Vert_Bottom		= 2,		// Place the child box inside on the bottom of the parent box

	eOutside_Side_Top			= 0,	// Place the child box outside on top of the parent box
	eOutside_Side_Bottom		= 1,	// Place the child box outside on the bottom of the parent box
	eOutside_Side_Left			= 2,	// Place the child box outside on the left of the parent box
	eOutside_Side_Right			= 3,	// Place the child box outside on the right of the parent box

	eOutside_Align_Left			= 0,	// Place the child box outside on the left top or bottom side of the parent box
	eOutside_Align_Center		= 1,	// Place the child box outside in the center top, bottom, left, or side side of the parent box
	eOutside_Align_Right		= 2,	// Place the child box outside on the right top or bottom side of the parent box
	eOutside_Align_Top			= 0,	// Place the child box outside on the top left or right side of the parent box
	eOutside_Align_Bottom		= 2,	// Place the child box outside on the bottom left or right side of the parent box
	
	ePlacement_Any = 0x7F,

	eGrid_MaxRows = 16,
	eGrid_MaxCols = 16,

	eStringTableSize = 1024,
};

enum EDisplayOrientation
{
	eDisplayOrientation_LandscapeUpside,
	eDisplayOrientation_LandscapeUpsideDown,
	eDisplayOrientation_PortraitUpside,
	eDisplayOrientation_PortraitUpsideDown,
};

enum ETouchEvent
{
	eTouchEvent_Down,
	eTouchEvent_Up
};

struct SFontData
{
	const unsigned char *index;
	const unsigned char *unicode;
	const unsigned char *data;
	unsigned char version;
	unsigned char reserved;
	unsigned char index1_first;
	unsigned char index1_last;
	unsigned char index2_first;
	unsigned char index2_last;
	unsigned char bits_index;
	unsigned char bits_width;
	unsigned char bits_height;
	unsigned char bits_xoffset;
	unsigned char bits_yoffset;
	unsigned char bits_delta;
	unsigned char line_space;
	unsigned char cap_height;
};

// This is used to organize the placement data of how to position a box relative to its parent
// The child defines how it is placed within the parent not the other way around. No enforcement is done on placement.
// Use one of the static functions to construct the required variant, Inside, Outside, Grid
struct SPlacement	
{
	// Create an inside my parent placement
	static SPlacement
	Inside(
		uint8_t	inHoriz,	// eInside_Horiz_*
		uint8_t	inVert);	// eInside_Vert_*
	
	// Create an outside my parent placement
	static SPlacement
	Outside(
		uint8_t	inSide,			// eOutside_Side_*
		uint8_t	inAlignment);	// eOutside_Align_*
	
	// Create a grid placement within my parent
	static SPlacement
	Grid(
		uint8_t	inRowIndex,		// The row index
		uint8_t	inColIndex);	// The col index
	
	inline uint8_t
	GetPlacementType(
		void)
	{
		return placementType;
	}

	inline uint8_t
	GetInsideHoriz(
		void)
	{
		return primary;
	}

	inline uint8_t
	GetInsideVert(
		void)
	{
		return secondary;
	}

	inline uint8_t
	GetOutsideSide(
		void)
	{
		return primary;
	}

	inline uint8_t
	GetOutsideAlign(
		void)
	{
		return secondary;
	}

	inline uint8_t
	GetGridRow(
		void)
	{
		return primary;
	}

	inline uint8_t
	GetGridCol(
		void)
	{
		return secondary;
	}

	inline bool
	Match(
		SPlacement	inPlacement)
	{
		if(placementType != inPlacement.placementType)
		{
			return false;
		}

		if(inPlacement.primary != ePlacement_Any && primary != inPlacement.primary)
		{
			return false;
		}

		if(inPlacement.secondary != ePlacement_Any && secondary != inPlacement.secondary)
		{
			return false;
		}

		return true;
	}

private:
	
	SPlacement(
		uint8_t	inPlace,
		uint8_t	inPrimary,
		uint8_t	inSecondary)
		:
		placementType(inPlace),
		primary(inPrimary),
		secondary(inSecondary)
	{
	}

	uint16_t	placementType : 2,
				primary : 7,
				secondary : 7;
};

struct SDisplayPoint
{
	SDisplayPoint(){}

	SDisplayPoint(
		int16_t	inX,
		int16_t	inY)
		:
		x(inX), y(inY)
	{
	}

	SDisplayPoint(
		SDisplayPoint const&	inPoint)
		:
		x(inPoint.x), y(inPoint.y)
	{
	}

	inline void
	Reset(
		void)
	{
		x = y = 0;
	}

	SDisplayPoint
	operator +=(
		SDisplayPoint const&	inRHS)
	{
		x += inRHS.x;
		y += inRHS.y;
		return *this;
	}

	inline bool
	operator ==(
		SDisplayPoint const&	inRHS) const
	{
		return x == inRHS.x && y == inRHS.y;
	}

	inline bool
	operator !=(
		SDisplayPoint const&	inRHS) const
	{
		return x != inRHS.x || y != inRHS.y;
	}

	inline void
	Dump(
		char const* inMsg) const
	{
		SystemMsg("%s=(%d,%d)", inMsg, x, y);
	}

	int16_t	x, y;
};

inline SDisplayPoint
operator +(
	SDisplayPoint const& inLHS,
	SDisplayPoint const& inRHS)
{
	return SDisplayPoint(inLHS.x + inRHS.x, inLHS.y + inRHS.y);
}

inline SDisplayPoint
operator -(
	SDisplayPoint const& inLHS,
	SDisplayPoint const& inRHS)
{
	return SDisplayPoint(inLHS.x - inRHS.x, inLHS.y - inRHS.y);
}

struct SDisplayRect
{
	SDisplayRect() {}

	SDisplayRect(
		SDisplayPoint const&	inTopLeft,
		SDisplayPoint const&	inBottomRight)
		:
		topLeft(inTopLeft),
		bottomRight(inBottomRight)
	{
	}

	SDisplayRect(
		int16_t	inLeft,
		int16_t	inTop,
		int16_t	inWidth,
		int16_t	inHeight)
		:
		topLeft(inLeft, inTop),
		bottomRight(inLeft + inWidth, inTop + inHeight)
	{
	}

	inline void
	Dump(
		char const*	inMsg) const
	{
		SystemMsg(inMsg);
		topLeft.Dump("  topLeft");
		bottomRight.Dump("  bottomRight");
	}

	inline void
	Intersect(
		SDisplayRect const&	inRectA,
		SDisplayRect const&	inRectB)
	{
		topLeft.x = MPin(inRectA.topLeft.x, inRectB.topLeft.x, inRectA.bottomRight.x);
		bottomRight.x = MPin(inRectA.topLeft.x, inRectB.bottomRight.x, inRectA.bottomRight.x);
		topLeft.y = MPin(inRectA.topLeft.y, inRectB.topLeft.y, inRectA.bottomRight.y);
		bottomRight.y = MPin(inRectA.topLeft.y, inRectB.bottomRight.y, inRectA.bottomRight.y);
	}

	inline bool
	IsEmpty(
		void) const
	{
		return topLeft.x >= bottomRight.x || topLeft.y >= bottomRight.y;
	}

	inline SDisplayPoint
	GetDimensions(
		void) const
	{
		return SDisplayPoint(bottomRight.x - topLeft.x, bottomRight.y - topLeft.y);
	}

	inline int16_t
	GetWidth(
		void) const
	{
		return bottomRight.x - topLeft.x;
	}

	inline int16_t
	GetHeight(
		void) const
	{
		return bottomRight.y - topLeft.y;
	}

	inline void
	Reset(
		void)
	{
		topLeft.Reset();
		bottomRight.Reset();
	}

	inline void
	SetWidth(
		int16_t	inWidth)
	{
		bottomRight.x = topLeft.x + inWidth;
	}

	inline void
	SetHeight(
		int16_t	inHeight)
	{
		bottomRight.y = topLeft.y + inHeight;
	}

	inline void
	SetWidthHeight(
		int16_t	inWidth,
		int16_t	inHeight)
	{
		bottomRight.x = topLeft.x + inWidth;
		bottomRight.y = topLeft.y + inHeight;
	}

	inline bool
	operator ==(
		SDisplayRect const&	inRHS) const
	{
		return topLeft == inRHS.topLeft && bottomRight == inRHS.bottomRight;
	}

	inline bool
	operator !=(
		SDisplayRect const&	inRHS) const
	{
		return topLeft != inRHS.topLeft || bottomRight != inRHS.bottomRight;
	}

	inline bool
	operator &&(
		SDisplayRect const&	inRHS) const
	{
		return !(bottomRight.x < inRHS.topLeft.x || bottomRight.y < inRHS.topLeft.y || topLeft.x >= inRHS.bottomRight.x || topLeft.y >= inRHS.bottomRight.y);
	}

	inline bool
	operator &&(
		SDisplayPoint const&	inRHS) const
	{
		return topLeft.x <= inRHS.x && inRHS.x < bottomRight.x && topLeft.y <= inRHS.y && inRHS.y < bottomRight.y;
	}

	SDisplayPoint	topLeft;
	SDisplayPoint	bottomRight;
};

struct SDisplayColor
{
	SDisplayColor(
		)
	{
	}

	SDisplayColor(
		uint8_t	inR,
		uint8_t	inG,
		uint8_t	inB,
		uint8_t	inA = 0xFF)
		:
		r(inR), g(inG), b(inB), a(inA)
	{
	}

	uint16_t
	GetRGB565(
		void) const
	{
		return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
	}

	uint8_t	r, g, b, a;
};

// A dummy class for the touch handler method object
class ITouchHandler
{
public:
};

// The typedef for the command handler method
typedef void
(ITouchHandler::*TTouchHandlerMethod)(
	ETouchEvent				inEvent,
	SDisplayPoint const&	inPoint,
	void*					inRefCon);

class ITouchDriver
{
public:

	virtual bool
	GetTouch(
		SDisplayPoint&	outTouchLoc) = 0;

};

class IDisplayDriver
{
public:
	
	virtual int16_t
	GetWidth(
		void) = 0;
	
	virtual int16_t
	GetHeight(
		void) = 0;

	virtual void
	BeginDrawing(
		void) = 0;

	virtual void
	EndDrawing(
		void) = 0;
	
	virtual void
	FillScreen(
		SDisplayColor const&	inColor) = 0;

	virtual void
	FillRect(
		SDisplayRect const&		inRect,
		SDisplayColor const&	inColor) = 0;

	virtual void
	DrawRect(
		SDisplayRect const&		inRect,
		SDisplayColor const&	inColor) = 0;

	virtual void
	DrawLine(
		SDisplayPoint const&	inPointA,
		SDisplayPoint const&	inPointB,
		SDisplayColor const&	inColor) = 0;

	virtual void
	DrawPixel(
		SDisplayPoint const&	inPoint,
		SDisplayColor const&	inColor) = 0;

	virtual void
	DrawContinuousStart(
		SDisplayRect const&		inRect,
		SDisplayColor const&	inFGColor,
		SDisplayColor const&	inBGColor) = 0;

	virtual void
	DrawContinuousBits(
		int16_t					inPixelCount,
		uint16_t				inSrcBitStartIndex,
		uint8_t const*			inSrcBitData) = 0;

	virtual void
	DrawContinuousSolid(
		int16_t					inPixelCount,
		bool					inUseForeground) = 0;

	virtual void
	DrawContinuousEnd(
		void) = 0;
};

class CDisplayRegion
{
public:
	
	CDisplayRegion(
		CDisplayRegion*	inParent,
		SPlacement		inPlacement);
	
	CDisplayRegion(
		CDisplayRegion*		inParent,
		SDisplayRect const&	inRect);

	int16_t
	GetWidth(
		void);

	int16_t
	GetHeight(
		void);

	void
	AddToParent(
		CDisplayRegion*	inParent);

	void
	RemoveFromParent(
		void);

	void
	SetPlacement(
		SPlacement	inPlacement);

	void
	SetBorder(
		int8_t	inLeft,
		int8_t	inRight,
		int8_t	inTop,
		int8_t	inBottom);

	void
	SetTouchHandler(
		ITouchHandler*		inHandler,
		TTouchHandlerMethod	inMethod,
		void*				inRefCon);

protected:
	
	void
	ProcessTouch(
		ETouchEvent				inTouchEvent,
		SDisplayPoint const&	inTouchLoc);

	void
	StartUpdate(
		void);

	virtual void
	UpdateDimensions(
		void);
	
	void
	UpdateOrigins(
		void);

	void
	EraseOldRegions(
		void);

	virtual void
	Draw(
		void);

	void
	SumRegionList(
		int16_t&		outWidth,
		int16_t&		outHeight,
		SPlacement		inPlacement,
		CDisplayRegion*	inList);

	CDisplayRegion*	parent;
	CDisplayRegion*	firstChild;
	CDisplayRegion*	nextChild;
	SPlacement		placement;
	SDisplayRect	curRect;
	SDisplayRect	oldRect;

	ITouchHandler*		touchObject;
	TTouchHandlerMethod	touchMethod;
	void*				touchRefCon;

	int16_t			width;
	int16_t			height;
	int8_t			borderLeft;
	int8_t			borderRight;
	int8_t			borderTop;
	int8_t			borderBottom;
	bool			fixedSize;

	friend CModule_Display;
};

class CDisplayRegion_Text : public CDisplayRegion
{
public:
	
	CDisplayRegion_Text(
		CDisplayRegion*		inParent,
		SPlacement			inPlacement,
		SDisplayColor		inFGColor,
		SDisplayColor		inBGColor,
		SFontData const&	inFont,
		char const*			inFormat,
		...);
	
	~CDisplayRegion_Text(
		);

	void
	printf(
		char const*	inFormat,
		...);
	
	void
	SetTextAlignment(
		uint8_t	inHorizAlignment,
		uint8_t	inVertAlignment);

	void
	SetTextFont(
		SFontData const&	inFont);

	void
	SetTextColor(
		SDisplayColor const&	inFGColor,
		SDisplayColor const&	inBGColor);

private:

	virtual void
	Draw(
		void);

	virtual void
	UpdateDimensions(
		void);

	char*				text;
	SDisplayColor		fgColor;
	SDisplayColor		bgColor;
	SFontData const*	font;
	bool				dirty;
	uint8_t				horizAlign;
	uint8_t				vertAlign;
};

class CModule_Display : public CModule
{
public:

	CModule_Display(
		);
	
	int16_t
	GetWidth(
		void);
	
	int16_t
	GetHeight(
		void);

	void
	SetDisplayDriver(
		IDisplayDriver*	inDisplayDriver);

	void
	SetTouchscreenDriver(
		ITouchDriver*	inTouchDriver);

	IDisplayDriver*
	GetDisplayDriver(
		void);

	CDisplayRegion*
	GetTopDisplayRegion(
		void);

	SDisplayPoint
	GetTextDimensions(
		char const*			inStr,
		SFontData const*	inFont);

	int16_t		// returns the width of the char
	DrawChar(
		char					inChar,
		SDisplayRect const&		inRect,
		SFontData const*		inFont,
		SDisplayColor const&	inForeground,
		SDisplayColor const&	inBackground);

	void
	DrawText(
		char const*				inStr,
		SDisplayRect const&		inRect,
		SFontData const*		inFont,
		SDisplayColor const&	inForeground,
		SDisplayColor const&	inBackground);

	void
	UpdateDisplay(
		void);

	// Fill the parts of inRectB not intersecting inRectA
	void
	FillRectDiff(
		SDisplayRect const&		inRectNew,
		SDisplayRect const&		inRectOld,
		SDisplayColor const&	inColor);

private:

	virtual void
	Update(
		uint32_t inDeltaTimeUS);

	char*
	ReallocString(
		char*	inStr,
		int		inNewSize);

	void
	FreeString(
		char*	inStr);

	friend CDisplayRegion_Text;

	IDisplayDriver*	displayDriver;
	ITouchDriver*	touchDriver;
	CDisplayRegion*	topRegion;
	uint8_t			stringTable[eStringTableSize];
	bool			touchDown;
	SDisplayPoint	touchLoc;
};

IDisplayDriver*
CreateILI9341Driver(
	EDisplayOrientation	inDisplayOrientation,
	uint8_t	inCS,
	uint8_t	inDC,
	uint8_t	inMOSI,
	uint8_t	inClk,
	uint8_t	inMISO);

ITouchDriver*
CreateXPT2046Driver(
	uint8_t	inChipselect);

extern CModule_Display*	gDisplayModule;

extern SDisplayColor	gColorWhite;
extern SDisplayColor	gColorBlack;
extern SDisplayColor	gColorRed;
extern SDisplayColor	gColorGreen;
extern SDisplayColor	gColorBlue;

#endif /* _EL_DISPLAY_H_ */
