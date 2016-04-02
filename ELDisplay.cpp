

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

#define SPICLOCK 30000000
#define MADCTL_MY  0x80
#define MADCTL_MX  0x40
#define MADCTL_MV  0x20
#define MADCTL_ML  0x10
#define MADCTL_RGB 0x00
#define MADCTL_BGR 0x08
#define MADCTL_MH  0x04

#define ILI9341_TFTWIDTH  240
#define ILI9341_TFTHEIGHT 320

#define ILI9341_NOP     0x00
#define ILI9341_SWRESET 0x01
#define ILI9341_RDDID   0x04
#define ILI9341_RDDST   0x09

#define ILI9341_SLPIN   0x10
#define ILI9341_SLPOUT  0x11
#define ILI9341_PTLON   0x12
#define ILI9341_NORON   0x13

#define ILI9341_RDMODE  0x0A
#define ILI9341_RDMADCTL  0x0B
#define ILI9341_RDPIXFMT  0x0C
#define ILI9341_RDIMGFMT  0x0D
#define ILI9341_RDSELFDIAG  0x0F

#define ILI9341_INVOFF  0x20
#define ILI9341_INVON   0x21
#define ILI9341_GAMMASET 0x26
#define ILI9341_DISPOFF 0x28
#define ILI9341_DISPON  0x29

#define ILI9341_CASET   0x2A
#define ILI9341_PASET   0x2B
#define ILI9341_RAMWR   0x2C
#define ILI9341_RAMRD   0x2E

#define ILI9341_PTLAR    0x30
#define ILI9341_MADCTL   0x36
#define ILI9341_VSCRSADD 0x37
#define ILI9341_PIXFMT   0x3A

#define ILI9341_FRMCTR1 0xB1
#define ILI9341_FRMCTR2 0xB2
#define ILI9341_FRMCTR3 0xB3
#define ILI9341_INVCTR  0xB4
#define ILI9341_DFUNCTR 0xB6

#define ILI9341_PWCTR1  0xC0
#define ILI9341_PWCTR2  0xC1
#define ILI9341_PWCTR3  0xC2
#define ILI9341_PWCTR4  0xC3
#define ILI9341_PWCTR5  0xC4
#define ILI9341_VMCTR1  0xC5
#define ILI9341_VMCTR2  0xC7

#define ILI9341_RDID1   0xDA
#define ILI9341_RDID2   0xDB
#define ILI9341_RDID3   0xDC
#define ILI9341_RDID4   0xDD

#define ILI9341_GMCTRP1 0xE0
#define ILI9341_GMCTRN1 0xE1

static const uint8_t init_commands[] =
{
	4, 0xEF, 0x03, 0x80, 0x02,
	4, 0xCF, 0x00, 0XC1, 0X30,
	5, 0xED, 0x64, 0x03, 0X12, 0X81,
	4, 0xE8, 0x85, 0x00, 0x78,
	6, 0xCB, 0x39, 0x2C, 0x00, 0x34, 0x02,
	2, 0xF7, 0x20,
	3, 0xEA, 0x00, 0x00,
	2, ILI9341_PWCTR1, 0x23, // Power control
	2, ILI9341_PWCTR2, 0x10, // Power control
	3, ILI9341_VMCTR1, 0x3e, 0x28, // VCM control
	2, ILI9341_VMCTR2, 0x86, // VCM control2
	2, ILI9341_MADCTL, 0x48, // Memory Access Control
	2, ILI9341_PIXFMT, 0x55,
	3, ILI9341_FRMCTR1, 0x00, 0x18,
	4, ILI9341_DFUNCTR, 0x08, 0x82, 0x27, // Display Function Control
	2, 0xF2, 0x00, // Gamma Function Disable
	2, ILI9341_GAMMASET, 0x01, // Gamma curve selected
	16, ILI9341_GMCTRP1, 0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08,
		0x4E, 0xF1, 0x37, 0x07, 0x10, 0x03, 0x0E, 0x09, 0x00, // Set Gamma
	16, ILI9341_GMCTRN1, 0x00, 0x0E, 0x14, 0x03, 0x11, 0x07,
		0x31, 0xC1, 0x48, 0x08, 0x0F, 0x0C, 0x31, 0x36, 0x0F, // Set Gamma
	0
};

CModule_Display*	gDisplayModule;

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

#ifdef WIN32
#include <Windows.h>

class CDisplayDriver_ILI9341 : public CModule, public IDisplayDriver
{
public:

	CDisplayDriver_ILI9341(
		EDisplayOrientation	inDisplayOrientation,
		uint8_t	inCS,
		uint8_t	inDC,
		uint8_t	inMOSI,
		uint8_t	inClk,
		uint8_t	inMISO)
		:
		displayOrientation(inDisplayOrientation),
		cs(inCS), dc(inDC), mosi(inMOSI), clk(inClk), miso(inMISO),
		CModule("ili9", 0, 0, NULL, 10 * 1000)
	{
		MReturnOnError(!((mosi == 11 || mosi == 7) && (miso == 12 || miso == 8) && (clk == 13 || clk == 14)));

		me = this;

		displayWidth = 320;
		displayHeight = 240;

		displayPort.topLeft.x = 0;
		displayPort.topLeft.y = 0;
		displayPort.bottomRight.x = displayWidth;
		displayPort.bottomRight.y = displayHeight;

		drawingActive = false;

		WNDCLASSEX ex;
 
		ex.cbSize = sizeof(WNDCLASSEX);
		ex.style = CS_OWNDC;
		ex.lpfnWndProc = WinProc;
		ex.cbClsExtra = 0;
		ex.cbWndExtra = 0;
		ex.hInstance = GetModuleHandle(0);
		ex.hIcon = LoadIcon(NULL, IDI_APPLICATION);
		ex.hCursor = LoadCursor(NULL, IDC_ARROW);
		ex.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
		ex.lpszMenuName = NULL;
		ex.lpszClassName = L"wndclass";
		ex.hIconSm = NULL;
 
		RegisterClassEx(&ex);
	
		hwnd = 
			CreateWindowEx(
				NULL,
 				L"wndclass",
				L"Window",
				WS_OVERLAPPED | WS_VISIBLE,
				100, 100,
				displayWidth + 50, displayHeight + 50,
				NULL,
				NULL,
				GetModuleHandle(0),
				this);

		ShowWindow(hwnd, SW_SHOW);
		UpdateWindow(hwnd);
		
	}
	
	virtual void
	Update(
		uint32_t inDeltaTimeUS)
	{
		MSG msg;
		if(PeekMessage(&msg, NULL, NULL, NULL, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	
	virtual int16_t
	GetWidth(
		void)
	{
		return displayWidth;
	}
	
	virtual int16_t
	GetHeight(
		void)
	{
		return displayHeight;
	}

	virtual void
	BeginDrawing(
		void)
	{
		drawingActive = true;
	}

	virtual void
	EndDrawing(
		void)
	{
		drawingActive = false;
	}
	
	virtual void
	FillScreen(
		SDisplayColor const&	inColor)
	{
		MReturnOnError(!drawingActive);

		uint16_t	color = inColor.GetRGB565();
		for(int x = 0; x < displayWidth; ++x)
		{
			for(int y = 0; y < displayHeight; ++y)
			{
				displayMemory[y][x] = color;
			}
		}
	}
	
	virtual void
	FillRect(
		SDisplayRect const&		inRect,
		SDisplayColor const&	inColor)
	{
		MReturnOnError(!drawingActive);

		SDisplayRect	clippedRect;

		clippedRect.Intersect(displayPort, inRect);

		if(clippedRect.IsEmpty())
		{
			return;
		}

		uint16_t	color = inColor.GetRGB565();
		for(int x = inRect.topLeft.x; x < inRect.bottomRight.x; ++x)
		{
			for(int y = inRect.topLeft.y; y < inRect.bottomRight.y; ++y)
			{
				displayMemory[y][x] = color;
			}
		}
	}

	virtual void
	DrawPixel(
		SDisplayPoint const&	inPoint,
		SDisplayColor const&	inColor)
	{
		MReturnOnError(!drawingActive);

		uint16_t	color = inColor.GetRGB565();

		displayMemory[inPoint.y][inPoint.x] = color;
	}

	virtual void
	DrawContinuousStart(
		SDisplayRect const&		inRect,
		SDisplayColor const&	inFGColor,
		SDisplayColor const&	inBGColor)
	{
		MReturnOnError(!drawingActive);

		SDisplayRect	clippedRect;

		clippedRect.Intersect(displayPort, inRect);

		continuousTotalClip = clippedRect.IsEmpty();
		if(continuousTotalClip)
		{
			return;
		}

		continuousRect = inRect;
		fgColor = inFGColor.GetRGB565();
		bgColor = inBGColor.GetRGB565();
		continuousX = inRect.topLeft.x;
		continuousY = inRect.topLeft.y;
	}

	virtual void
	DrawContinuousBits(
		int16_t					inPixelCount,
		uint16_t				inSrcBitStartIndex,
		uint8_t const*			inSrcBitData)
	{
		MReturnOnError(!drawingActive);

		if(continuousTotalClip)
		{
			return;
		}

		while(inPixelCount-- > 0)
		{
			if(continuousX >= 0 && continuousX < displayWidth && continuousY >= 0 && continuousY < displayHeight)
			{
				if(inSrcBitData[inSrcBitStartIndex >> 3] & (1 << (7 - (inSrcBitStartIndex & 0x7))))
				{
					displayMemory[continuousY][continuousX] = fgColor;
				}
				else
				{
					displayMemory[continuousY][continuousX] = bgColor;
				}
			}

			++continuousX;
			if(continuousX >= continuousRect.bottomRight.x)
			{
				continuousX = continuousRect.topLeft.x;
				++continuousY;
			}
			++inSrcBitStartIndex;
		}
	}

	virtual void
	DrawContinuousSolid(
		int16_t					inPixelCount,
		bool					inUseForeground)
	{
		MReturnOnError(!drawingActive);

		if(continuousTotalClip)
		{
			return;
		}

		while(inPixelCount-- > 0)
		{
			if(continuousX >= 0 && continuousX < displayWidth && continuousY >= 0 && continuousY < displayHeight)
			{
				if(inUseForeground)
				{
					displayMemory[continuousY][continuousX] = fgColor;
				}
				else
				{
					displayMemory[continuousY][continuousX] = bgColor;
				}
			}

			++continuousX;
			if(continuousX >= continuousRect.bottomRight.x)
			{
				continuousX = continuousRect.topLeft.x;
				++continuousY;
			}
		}
	}

	virtual void
	DrawContinuousEnd(
		void)
	{
		MReturnOnError(!drawingActive);
	}

	static LRESULT CALLBACK 
	WinProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

	static CDisplayDriver_ILI9341*	me;

	EDisplayOrientation	displayOrientation;
	int16_t	displayWidth;
	int16_t	displayHeight;
	uint8_t	cs, dc, mosi, clk, miso;
	uint8_t pcs_data, pcs_command;

	bool	drawingActive;

	SDisplayRect	displayPort;

	bool			continuousTotalClip;
	int16_t			continuousX;
	int16_t			continuousY;
	SDisplayRect	continuousRect;
	uint16_t		fgColor;
	uint16_t		bgColor;

	uint16_t	displayMemory[240][320];

	HDC		hdc;
	HWND	hwnd;
};

CDisplayDriver_ILI9341*	CDisplayDriver_ILI9341::me;

LRESULT CALLBACK 
CDisplayDriver_ILI9341::WinProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	switch(msg)
	{
		case WM_PAINT:
		{
			HDC hdc = GetDC(hwnd);

			for(int x = 0; x < me->displayWidth; ++x)
			{
				for(int y = 0; y < me->displayHeight; ++y)
				{
					uint16_t	color = me->displayMemory[y][x];
					uint8_t		r = color >> 11;
					uint8_t		g = (color >> 5) & 0x3F;
					uint8_t		b = color & 0x1F;
					SetPixel(hdc, x, y, RGB(r << 3, g << 2, b << 3));
				}
			}

			ReleaseDC(hwnd, hdc);
			return 0;
		}
	}

	return DefWindowProc(hwnd, msg, wparam, lparam);
}

#else

class CDisplayDriver_ILI9341 : public IDisplayDriver
{
public:

	inline void
	WaitFIFOReady(
		void)
	{
		uint32_t sr;
		volatile uint32_t MUNUSED tmp;
		do {
			sr = (KINETISK_SPI0.SR >> 12) & 0xF;
		} while (sr > 3);
	}

	inline void
	WaitFIFOEmpty(void)
	{
		uint32_t sr;
		volatile uint32_t MUNUSED tmp;
		do {
			sr = (KINETISK_SPI0.SR >> 12) & 0xF;
		} while (sr > 0);
	}

	inline void 
	WriteCommand(
		uint8_t inCommand)
	{
		WaitFIFOReady();
		KINETISK_SPI0.PUSHR = inCommand | (pcs_command << 16) | SPI_PUSHR_CTAS(0) | SPI_PUSHR_CONT;
	}

	inline void 
	WriteData8(uint8_t inData)
	{
		WaitFIFOReady();
		KINETISK_SPI0.PUSHR = inData | (pcs_data << 16) | SPI_PUSHR_CTAS(0) | SPI_PUSHR_CONT;
	}

	inline void 
	WriteData16(
		uint16_t inData)
	{
		WaitFIFOReady();
		KINETISK_SPI0.PUSHR = inData | (pcs_data << 16) | SPI_PUSHR_CTAS(1) | SPI_PUSHR_CONT;
	}

	inline void 
	setAddr(
		uint16_t x0, 
		uint16_t y0, 
		uint16_t x1, 
		uint16_t y1)
	{
		WriteCommand(ILI9341_CASET); // Column addr set
		WriteData16(x0);   // XSTART
		WriteData16(x1);   // XEND
		WriteCommand(ILI9341_PASET); // Row addr set
		WriteData16(y0);   // YSTART
		WriteData16(y1);   // YEND
	}

	CDisplayDriver_ILI9341(
		EDisplayOrientation	inDisplayOrientation,
		uint8_t	inCS,
		uint8_t	inDC,
		uint8_t	inMOSI,
		uint8_t	inClk,
		uint8_t	inMISO)
		:
		displayOrientation(inDisplayOrientation),
		cs(inCS), dc(inDC), mosi(inMOSI), clk(inClk), miso(inMISO)
	{
		MReturnOnError(!((mosi == 11 || mosi == 7) && (miso == 12 || miso == 8) && (clk == 13 || clk == 14)));

		SPI.begin();
		MReturnOnError(!SPI.pinIsChipSelect(cs, dc));

		pcs_data = SPI.setCS(cs);
		pcs_command = pcs_data | SPI.setCS(dc);

		SPI.beginTransaction(SPISettings(SPICLOCK, MSBFIRST, SPI_MODE0));

		const uint8_t *addr = init_commands;
		while (1) 
		{
			uint8_t count = *addr++;
			if (count-- == 0)
			{
				break;
			}
			WriteCommand(*addr++);
			while (count-- > 0)
			{
				WriteData8(*addr++);
			}
		}
		WriteCommand(ILI9341_SLPOUT);    // Exit Sleep
		WaitFIFOEmpty();

		delay(120); 		
		WriteCommand(ILI9341_DISPON);    // Display on
		WaitFIFOEmpty();

		WriteCommand(ILI9341_MADCTL);
		switch(displayOrientation)
		{
			case eDisplayOrientation_LandscapeUpside:
				displayWidth = ILI9341_TFTHEIGHT;
				displayHeight = ILI9341_TFTWIDTH;
				WriteData8(MADCTL_MV | MADCTL_BGR);
				break;

			case eDisplayOrientation_LandscapeUpsideDown:
				displayWidth = ILI9341_TFTHEIGHT;
				displayHeight = ILI9341_TFTWIDTH;
				WriteData8(MADCTL_MX | MADCTL_MY | MADCTL_MV | MADCTL_BGR);
				break;

			case eDisplayOrientation_PortraitUpside:
				displayWidth = ILI9341_TFTWIDTH;
				displayHeight = ILI9341_TFTHEIGHT;
				WriteData8(MADCTL_MX | MADCTL_BGR);
				break;

			case eDisplayOrientation_PortraitUpsideDown:
				WriteData8(MADCTL_MY | MADCTL_BGR);
				displayWidth = ILI9341_TFTWIDTH;
				displayHeight = ILI9341_TFTHEIGHT;
				break;

			default:
				WriteData8(MADCTL_MV | MADCTL_BGR);
				SystemMsg("Unknown orientation");
		}
		WaitFIFOEmpty();

		SPI.endTransaction();
		SPI.end();

		displayPort.topLeft.x = 0;
		displayPort.topLeft.y = 0;
		displayPort.bottomRight.x = displayWidth;
		displayPort.bottomRight.y = displayHeight;

		drawingActive = false;
	}
	
	virtual int16_t
	GetWidth(
		void)
	{
		return displayWidth;
	}
	
	virtual int16_t
	GetHeight(
		void)
	{
		return displayHeight;
	}

	virtual void
	BeginDrawing(
		void)
	{
		drawingActive = true;
		SPI.begin();
	}

	virtual void
	EndDrawing(
		void)
	{
		drawingActive = false;
		SPI.end();
	}
	
	virtual void
	FillScreen(
		SDisplayColor const&	inColor)
	{
		MReturnOnError(!drawingActive);

		SPI.beginTransaction(SPISettings(SPICLOCK, MSBFIRST, SPI_MODE0));
		setAddr(0, 0, displayWidth - 1, displayHeight - 1);
		WriteCommand(ILI9341_RAMWR);

		uint16_t	color = inColor.GetRGB565();
		for(int32_t y = displayWidth * displayHeight; y > 0; y--)
		{
			WriteData16(color);
		}
		WaitFIFOEmpty();
		SPI.endTransaction();
	}
	
	virtual void
	FillRect(
		SDisplayRect const&		inRect,
		SDisplayColor const&	inColor)
	{
		MReturnOnError(!drawingActive);

		SDisplayRect	clippedRect;

		clippedRect.Intersect(displayPort, inRect);

		if(clippedRect.IsEmpty())
		{
			return;
		}

		SPI.beginTransaction(SPISettings(SPICLOCK, MSBFIRST, SPI_MODE0));
		setAddr(clippedRect.topLeft.x, clippedRect.topLeft.y, clippedRect.bottomRight.x - 1, clippedRect.bottomRight.y - 1);
		WriteCommand(ILI9341_RAMWR);

		SDisplayPoint dimensions = clippedRect.GetDimensions();
		uint16_t	color = inColor.GetRGB565();
		for(int32_t y = dimensions.y * dimensions.x; y > 0; y--)
		{
			WriteData16(color);
		}
		WaitFIFOEmpty();
		SPI.endTransaction();
	}

	virtual void
	DrawPixel(
		SDisplayPoint const&	inPoint,
		SDisplayColor const&	inColor)
	{
		MReturnOnError(!drawingActive);

	}

	virtual void
	DrawContinuousStart(
		SDisplayRect const&		inRect,
		SDisplayColor const&	inFGColor,
		SDisplayColor const&	inBGColor)
	{
		MReturnOnError(!drawingActive);

		SDisplayRect	clippedRect;

		clippedRect.Intersect(displayPort, inRect);

		continuousTotalClip = clippedRect.IsEmpty();
		if(continuousTotalClip)
		{
			return;
		}

		continuousRect = inRect;
		fgColor = inFGColor.GetRGB565();
		bgColor = inBGColor.GetRGB565();
		continuousX = inRect.topLeft.x;
		continuousY = inRect.topLeft.y;

		SPI.beginTransaction(SPISettings(SPICLOCK, MSBFIRST, SPI_MODE0));
		setAddr(clippedRect.topLeft.x, clippedRect.topLeft.y, clippedRect.bottomRight.x - 1, clippedRect.bottomRight.y - 1);
		WriteCommand(ILI9341_RAMWR);
	}

	virtual void
	DrawContinuousBits(
		int16_t					inPixelCount,
		uint16_t				inSrcBitStartIndex,
		uint8_t const*			inSrcBitData)
	{
		MReturnOnError(!drawingActive);

		if(continuousTotalClip)
		{
			return;
		}

		while(inPixelCount-- > 0)
		{
			if(continuousX >= 0 && continuousX < displayWidth && continuousY >= 0 && continuousY < displayHeight)
			{
				if(inSrcBitData[inSrcBitStartIndex >> 3] & (1 << (7 - (inSrcBitStartIndex & 0x7))))
				{
					WriteData16(fgColor);
				}
				else
				{
					WriteData16(bgColor);
				}
			}

			++continuousX;
			if(continuousX >= continuousRect.bottomRight.x)
			{
				continuousX = continuousRect.topLeft.x;
				++continuousY;
			}
			++inSrcBitStartIndex;
		}
	}

	virtual void
	DrawContinuousSolid(
		int16_t					inPixelCount,
		bool					inUseForeground)
	{
		MReturnOnError(!drawingActive);

		if(continuousTotalClip)
		{
			return;
		}

		while(inPixelCount-- > 0)
		{
			if(continuousX >= 0 && continuousX < displayWidth && continuousY >= 0 && continuousY < displayHeight)
			{
				if(inUseForeground)
				{
					WriteData16(fgColor);
				}
				else
				{
					WriteData16(bgColor);
				}
			}

			++continuousX;
			if(continuousX >= continuousRect.bottomRight.x)
			{
				continuousX = continuousRect.topLeft.x;
				++continuousY;
			}
		}
	}

	virtual void
	DrawContinuousEnd(
		void)
	{
		MReturnOnError(!drawingActive);

		if(continuousTotalClip)
		{
			return;
		}

		WaitFIFOEmpty();
		SPI.endTransaction();
	}

	EDisplayOrientation	displayOrientation;
	int16_t	displayWidth;
	int16_t	displayHeight;
	uint8_t	cs, dc, mosi, clk, miso;
	uint8_t pcs_data, pcs_command;

	bool	drawingActive;

	SDisplayRect	displayPort;

	bool			continuousTotalClip;
	int16_t			continuousX;
	int16_t			continuousY;
	SDisplayRect	continuousRect;
	uint16_t		fgColor;
	uint16_t		bgColor;
};
#endif

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
	borderBottom(0)
{
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
	borderBottom(0)
{
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

		SumRegionList(curWidth, curHeight, SPlacement::Inside(eInside_Horiz_Center, eSecondary_Any), firstChild);
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
							curRect.topLeft.x = parentRect.topLeft.x + ((parentRect.GetWidth() - width) >> 1);
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
							curRect.topLeft.y = parentRect.topLeft.y + ((parentRect.GetHeight() - height) >> 1);
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
					curRect.topLeft.x = parentRect.topLeft.x + ((parentRect.GetWidth() - width) >> 1);
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
					curRect.topLeft.y = parentRect.topLeft.y + ((parentRect.GetHeight() - height) >> 1);
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
	gDisplayModule->DrawText(text, curRect.topLeft + SDisplayPoint(borderLeft, borderTop), font, fgColor, bgColor);
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
		width = gDisplayModule->GetTextWidth(text, font) + borderLeft + borderRight;
		height = gDisplayModule->GetTextHeight(text, font) + borderTop + borderBottom;
	}

	CDisplayRegion::UpdateDimensions();
}

CModule_Display::CModule_Display(
	)
	:
	CModule("disp")
{
	displayDriver = NULL;
	gDisplayModule = this;
	memset(stringTable, 0xFF, sizeof(stringTable));
}

void
CModule_Display::SetDisplayDriver(
	IDisplayDriver*	inDisplayDriver)
{
	displayDriver = inDisplayDriver;
	topRegion = new CDisplayRegion(SDisplayRect(0, 0, displayDriver->GetWidth(), displayDriver->GetHeight()), NULL);
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

uint16_t
CModule_Display::GetTextWidth(
	char const*			inStr,
	SFontData const*	inFont)
{
	uint16_t		result = 0;
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

		bitoffset = inFont->bits_width + 3;
		bitoffset += inFont->bits_height;
		bitoffset += inFont->bits_xoffset;
		bitoffset += inFont->bits_yoffset;

		uint32_t delta = fetchbits_unsigned(data, bitoffset, inFont->bits_delta);

		result += (uint16_t)delta;
	}

	return result;
}

uint16_t
CModule_Display::GetTextHeight(
	char const*			inStr,
	SFontData const*	inFont)
{
	return inFont->line_space;
}

int16_t
CModule_Display::DrawChar(
	char					inChar,
	SDisplayPoint const&	inPoint,
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

	displayDriver->DrawContinuousStart(SDisplayRect(inPoint.x, inPoint.y, MMax(delta, width + abs(xoffset)), inFont->line_space), inForeground, inBackground);

	int16_t	topLines = (int16_t)inFont->cap_height - height - yoffset;
	int16_t	bottomLines = (int16_t)inFont->line_space - topLines - height;

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
	SDisplayPoint const&	inPoint,
	SFontData const*		inFont,
	SDisplayColor const&	inForeground,
	SDisplayColor const&	inBackground)
{
	SDisplayPoint	curPoint = inPoint;

	for(;;)
	{
		char c = *inStr++;

		if(c == 0)
		{
			break;
		}

		int16_t	charWidth = DrawChar(c, curPoint, inFont, inForeground, inBackground);
		curPoint.x += charWidth;
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

