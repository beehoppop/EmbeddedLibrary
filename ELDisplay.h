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

class IDisplayDriver
{
public:

	virtual void
	DrawText(
		char const*	inStr,
		uint16_t	inX,
		uint16_t	inY,
		SFontData*	inFont,
		uint16_t	inForground,
		uint16_t	inBackground) = 0;

};

class CDisplayRegion
{
public:
	
	CDisplayRegion(
		uint16_t	inFlags);

	virtual uint16_t
	GetWidth(
		void) = 0;

	virtual uint16_t
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
		CDisplayRegion*	inParent);

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
	uint16_t		width;
	uint16_t		height;
	uint16_t		x;
	uint16_t		y;
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

	virtual uint16_t
	GetWidth(
		void);

	virtual uint16_t
	GetHeight(
		void);

	virtual void
	Draw(
		void);

	char const*	text;
};

class CDisplayModule : public CModule
{
public:

	CDisplayModule(
		);

	CDisplayRegion*
	GetTopDisplayRegion(
		void);

	void
	Draw(
		void);

	uint16_t
	GetTextWidth(
		char const*	inStr,
		SFontData*	inFont);

	uint16_t
	GetTextHeight(
		char const*	inStr,
		SFontData*	inFont);


private:

};

extern CDisplayModule*	gModule;

#endif /* _EL_DISPLAY_H_ */
