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

enum
{
	eHoriz_Left		= 1 << 0,
	eHoriz_Center	= 1 << 1,
	eHoriz_Right	= 1 << 2,
	eVert_Top		= 1 << 3,
	eVert_Center	= 1 << 4,
	eVert_Bottom	= 1 << 5,
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

	void
	Dump(
		char const* inMsg) const
	{
		SystemMsg("%s=(%d,%d)", inMsg, x, y);
	}

	int16_t	x, y;
};

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

	void
	Dump(
		char const*	inMsg) const
	{
		SystemMsg(inMsg);
		topLeft.Dump("  topLeft");
		bottomRight.Dump("  bottomRight");
	}

	void
	Intersect(
		SDisplayRect const&	inRectA,
		SDisplayRect const&	inRectB)
	{
		topLeft.x = MPin(inRectA.topLeft.x, inRectB.topLeft.x, inRectA.bottomRight.x);
		bottomRight.x = MPin(inRectA.topLeft.x, inRectB.bottomRight.x, inRectA.bottomRight.x);
		topLeft.y = MPin(inRectA.topLeft.y, inRectB.topLeft.y, inRectA.bottomRight.y);
		bottomRight.y = MPin(inRectA.topLeft.y, inRectB.bottomRight.y, inRectA.bottomRight.y);
	}

	bool
	IsEmpty(
		void) const
	{
		return topLeft.x >= bottomRight.x || topLeft.y >= bottomRight.y;
	}

	SDisplayPoint
	GetDimensions(
		void) const
	{
		return SDisplayPoint(bottomRight.x - topLeft.x, bottomRight.y - topLeft.y);
	}

	SDisplayPoint	topLeft;
	SDisplayPoint	bottomRight;
};

struct SDisplayColor
{
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
		uint16_t	inFlags);

	virtual int16_t
	GetWidth(
		void) = 0;

	virtual int16_t
	GetHeight(
		void) = 0;

	virtual void
	Draw(
		void) = 0;

	void
	AddToGraph(
		CDisplayRegion*	inParent);

	void
	RemoveFromGraph(
		void);

	void
	SetFlags(
		uint16_t	inFlags);

private:

	void
	UpdatePlacement(
		void);

	CDisplayRegion*	parent;
	CDisplayRegion*	firstChild;
	CDisplayRegion*	nextChild;
	uint16_t		flags;
	int16_t			width;
	int16_t			height;
	int16_t			x;
	int16_t			y;
};

class CDisplayRegion_Text : public CDisplayRegion
{
public:
	
	void
	printf(
		char const&	inFormat,
		...);
	
	void
	SetAlignment(
		uint16_t	inAlignmentFlags);

	void
	SetFont(
		SFontData*	inFont);

	void
	SetForegroundColor(
		void);

	void
	SetBackgroundColor(
		void);

private:

	virtual int16_t
	GetWidth(
		void);

	virtual int16_t
	GetHeight(
		void);

	virtual void
	Draw(
		void);

	char const*	text;
};

class CModule_Display : public CModule
{
public:

	CModule_Display(
		);

	void
	SetDisplayDriver(
		IDisplayDriver*	inDisplayDriver);

	CDisplayRegion*
	GetTopDisplayRegion(
		void);

	uint16_t
	GetTextWidth(
		char const*	inStr,
		SFontData*	inFont);

	uint16_t
	GetTextHeight(
		char const*	inStr,
		SFontData*	inFont);

	int16_t		// returns the width of the char
	DrawChar(
		char					inChar,
		SDisplayPoint const&	inPoint,
		SFontData*				inFont,
		SDisplayColor const&	inForeground,
		SDisplayColor const&	inBackground);

	void
	DrawText(
		char const*				inStr,
		SDisplayPoint const&	inPoint,
		SFontData*				inFont,
		SDisplayColor const&	inForeground,
		SDisplayColor const&	inBackground);

private:

	IDisplayDriver*	displayDriver;
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
