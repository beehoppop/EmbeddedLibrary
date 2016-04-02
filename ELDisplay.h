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
	ePlace_Inside	= 0,				// Place the child box inside of the parent box
	ePlace_Outside	= 1,				// Place the child box outside of the parent box

	eInside_Horiz_Left		= 0,		// Place the child box inside in the left side of the parent box
	eInside_Horiz_Center	= 1,		// Place the child box inside in the horiz center of the parent box
	eInside_Horiz_Right		= 2,		// Place the child box inside in the right side of the parent box

	eInside_Vert_Top		= 0,		// Place the child box inside in the top of the parent box
	eInside_Vert_Center		= 1,		// PLace the child box inside in the vertical center of the parent box
	eInside_Vert_Bottom		= 2,		// Place the child box inside in the bottom of the parent box

	eOutside_SideTop			= 0,	// Place the child box outside on top of the parent box
	eOutside_SideBottom			= 1,	// Place the child box outside on the bottom of the parent box
	eOutside_SideLeft			= 2,	// Place the child box outside on the left of the parent box
	eOutside_SideRight			= 3,	// Place the child box outside on the right of the parent box

	eOutside_AlignLeft			= 0,	// Place the child box outside on the left top or bottom side of the parent box
	eOutside_AlignCenter		= 1,	// Place the child box outside in the center top, bottom, left, or side side of the parent box
	eOutside_AlignRight			= 2,	// Place the child box outside on the right top or bottom side of the parent box
	eOutside_AlignTop			= 0,	// Place the child box outside on the top left or right side of the parent box
	eOutside_AlignBottom		= 2,	// Place the child box outside on the bottom left or right side of the parent box
	
	ePrimary_Any = 4,
	eSecondary_Any = 3,

	eStringTableSize = 1024,
};

enum EDisplayOrientation
{
	eDisplayOrientation_LandscapeUpside,
	eDisplayOrientation_LandscapeUpsideDown,
	eDisplayOrientation_PortraitUpside,
	eDisplayOrientation_PortraitUpsideDown,
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

struct SPlacement	// This is used to organize the placement data of how to position a box relative to its parent
{

	static SPlacement
	Inside(
		uint8_t	inHoriz,
		uint8_t	inVert);

	static SPlacement
	Outside(
		uint8_t	inSide,
		uint8_t	inAlignment);
	
	inline bool
	GetPlace(
		void)
	{
		return place;
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

	inline bool
	Match(
		SPlacement	inPlacement)
	{
		if(place != inPlacement.place)
		{
			return false;
		}

		if(inPlacement.primary != ePrimary_Any && primary != inPlacement.primary)
		{
			return false;
		}

		if(inPlacement.secondary != eSecondary_Any && secondary != inPlacement.secondary)
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
		place(inPlace),
		primary(inPrimary),
		secondary(inSecondary)
	{
	}

	uint8_t		place : 1,
				primary : 3,
				secondary : 2;
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
		SPlacement		inPlacement,
		CDisplayRegion*	inParent);
	
	CDisplayRegion(
		SDisplayRect const&	inRect,
		CDisplayRegion*		inParent);

	virtual int16_t
	GetWidth(
		void);

	virtual int16_t
	GetHeight(
		void);

	void
	AddToGraph(
		CDisplayRegion*	inParent);

	void
	RemoveFromGraph(
		void);

	void
	SetPlacement(
		SPlacement	inPlacement);

	void
	SetBorder(
		int16_t	inLeft,
		int16_t	inRight,
		int16_t	inTop,
		int16_t	inBottom);

protected:

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
	int16_t			width;
	int16_t			height;
	int16_t			borderLeft;
	int16_t			borderRight;
	int16_t			borderTop;
	int16_t			borderBottom;

	friend CModule_Display;
};

class CDisplayRegion_Text : public CDisplayRegion
{
public:
	
	CDisplayRegion_Text(
		SPlacement			inPlacement,
		CDisplayRegion*		inParent,
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
		uint16_t	inAlignmentFlags);

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
};

class CModule_Display : public CModule
{
public:

	CModule_Display(
		);

	void
	SetDisplayDriver(
		IDisplayDriver*	inDisplayDriver);

	IDisplayDriver*
	GetDisplayDriver(
		void);

	CDisplayRegion*
	GetTopDisplayRegion(
		void);

	uint16_t
	GetTextWidth(
		char const*			inStr,
		SFontData const*	inFont);

	uint16_t
	GetTextHeight(
		char const*			inStr,
		SFontData const*	inFont);

	int16_t		// returns the width of the char
	DrawChar(
		char					inChar,
		SDisplayPoint const&	inPoint,
		SFontData const*		inFont,
		SDisplayColor const&	inForeground,
		SDisplayColor const&	inBackground);

	void
	DrawText(
		char const*				inStr,
		SDisplayPoint const&	inPoint,
		SFontData const*		inFont,
		SDisplayColor const&	inForeground,
		SDisplayColor const&	inBackground);

	void
	UpdateDisplay(
		void);

private:

	char*
	ReallocString(
		char*	inStr,
		int		inNewSize);

	void
	FreeString(
		char*	inStr);

	friend CDisplayRegion_Text;

	IDisplayDriver*	displayDriver;
	CDisplayRegion*	topRegion;
	uint8_t			stringTable[eStringTableSize];
};

IDisplayDriver*
CreateILI9341Driver(
	EDisplayOrientation	inDisplayOrientation,
	uint8_t	inCS,
	uint8_t	inDC,
	uint8_t	inMOSI,
	uint8_t	inClk,
	uint8_t	inMISO);

extern CModule_Display*	gDisplayModule;

#endif /* _EL_DISPLAY_H_ */
