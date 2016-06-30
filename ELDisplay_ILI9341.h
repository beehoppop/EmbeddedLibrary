#if defined(ARDUINO) && ARDUINO >= 100
#ifndef _ELDISPLAY_ILI9341_
#define _ELDISPLAY_ILI9341_

// https://github.com/PaulStoffregen/ILI9341_t3
// http://forum.pjrc.com/threads/26305-Highly-optimized-ILI9341-(320x240-TFT-color-display)-library

/***************************************************
  This is our library for the Adafruit  ILI9341 Breakout and Shield
  ----> http://www.adafruit.com/products/1651

  Check out the links above for our tutorials and wiring diagrams
  These displays use SPI to communicate, 4 or 5 pins are required to
  interface (RST is optional)
  Adafruit invests time and resources providing this open source code,
  please support Adafruit and open-source hardware by purchasing
  products from Adafruit!

  Written by Limor Fried/Ladyada for Adafruit Industries.
  MIT license, all text above must be included in any redistribution
 ****************************************************/

/*
	I had plans to try and optimize this code some more
		- clearing the rcv fifo instead of incrementally draining it
		- Checking for room on the fifo before adding to it instead of after
		- Simplifying the cont/last functions
	I had this all working just fine with the ILI9341 itself but it looks like
	somehow these things left the SPI hardware in a bad state so using other
	devices started to fail... I really wish that SPI.begin() would put
	the hardware into a know working state but it doesn't - maybe one day I will 
	try to write a better SPI api and implementation

*/

#include <XPT2046_Touchscreen.h>

#include "ELDisplay.h"

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

#define TS_MINX 153
#define TS_MINY 215
#define TS_MAXX 3960
#define TS_MAXY 3836

class ITouchDriver_XPT2046 : public ITouchDriver
{
public:

	ITouchDriver_XPT2046(
		uint8_t	inCS)
	:
	touchscreen(inCS)
	{
		touchscreen.begin();
	}

	virtual bool
	GetTouch(
		SDisplayPoint&	outTouchLoc)
	{
		bool	result = touchscreen.touched();

		if(result)
		{
			TS_Point	touch = touchscreen.getPoint();
			outTouchLoc.x = (int16_t)map(touch.x, TS_MINX, TS_MAXX, gDisplayModule->GetWidth(), 0);
			outTouchLoc.y = (int16_t)map(touch.y, TS_MINY, TS_MAXY, gDisplayModule->GetHeight(), 0);

			Serial.printf("%d %d\n", outTouchLoc.x, outTouchLoc.y);
		}

		return result;
	}

	XPT2046_Touchscreen	touchscreen;
};

class CDisplayDriver_ILI9341 : public IDisplayDriver, public CModule
{
public:

	MModule_Declaration(
		CDisplayDriver_ILI9341,
		EDisplayOrientation	inDisplayOrientation,
		uint8_t	inCS,
		uint8_t	inDC,
		uint8_t	inMOSI,
		uint8_t	inClk,
		uint8_t	inMISO)

	#if 0
	// sigh...
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
	WaitFIFOEmpty(
		void)
	{
		uint32_t sr;
		volatile uint32_t MUNUSED tmp;
		do {
			sr = (KINETISK_SPI0.SR >> 12) & 0xF;
		} while (sr > 0);
	}

	inline void
	WaitEOQ(
		void)
	{
		uint32_t sr;
		volatile uint32_t MUNUSED tmp;
		do {
			sr = KINETISK_SPI0.SR;
		} while (!(sr & SPI_SR_EOQF));
		KINETISK_SPI0.SR = SPI_SR_EOQF;
	}

	inline void 
	WriteCommand(
		uint8_t inCommand,
		bool	inEOQ = false)
	{
		WaitFIFOReady();
		if(inEOQ)
		{
			KINETISK_SPI0.PUSHR = inCommand | (pcs_command << 16) | SPI_PUSHR_CTAS(0) | SPI_PUSHR_EOQ;
		}
		else
		{
			KINETISK_SPI0.PUSHR = inCommand | (pcs_command << 16) | SPI_PUSHR_CTAS(0) | SPI_PUSHR_CONT;
		}
	}

	inline void 
	WriteData8(
		uint8_t	inData,
		bool	inEOQ = false)
	{
		WaitFIFOReady();
		if(inEOQ)
		{
			KINETISK_SPI0.PUSHR = inData | (pcs_data << 16) | SPI_PUSHR_CTAS(0) | SPI_PUSHR_EOQ;
		}
		else
		{
			KINETISK_SPI0.PUSHR = inData | (pcs_data << 16) | SPI_PUSHR_CTAS(0) | SPI_PUSHR_CONT;
		}
	}

	inline void 
	WriteData16(
		uint16_t	inData,
		bool		inEOQ = false)
	{
		WaitFIFOReady();
		if(inEOQ)
		{
			KINETISK_SPI0.PUSHR = inData | (pcs_data << 16) | SPI_PUSHR_CTAS(1) | SPI_PUSHR_EOQ;
		}
		else
		{
			KINETISK_SPI0.PUSHR = inData | (pcs_data << 16) | SPI_PUSHR_CTAS(1) | SPI_PUSHR_CONT;
		}
	}

	inline void
	BeginTransaction(
		void)
	{
		SPI.beginTransaction(SPISettings(SPICLOCK, MSBFIRST, SPI_MODE0));
		savedSPIMCR = SPI0_MCR;
	}

	inline void
	EndTransaction(
		void)
	{
		SPI0_MCR = savedSPIMCR | SPI_MCR_CLR_RXF;
		SPI.endTransaction();
	}
	#endif

	inline void 
	WaitFifoNotFull(
		void) 
	{
		uint32_t sr;
		uint32_t tmp __attribute__((unused));
		do {
			sr = KINETISK_SPI0.SR;
			if (sr & 0xF0) tmp = KINETISK_SPI0.POPR;  // drain RX FIFO
		} while ((sr & (15 << 12)) > (3 << 12));
	}

	inline void 
	WaitFifoEmpty(
		void) 
	{
		uint32_t sr;
		uint32_t tmp __attribute__((unused));
		do {
			sr = KINETISK_SPI0.SR;
			if (sr & 0xF0) tmp = KINETISK_SPI0.POPR;  // drain RX FIFO
		} while ((sr & 0xF0F0) > 0);             // wait both RX & TX empty
	}

	inline void 
	WaitTransmitComplete(
		void)
	{
		uint32_t tmp __attribute__((unused));
		while (!(KINETISK_SPI0.SR & SPI_SR_TCF)) ; // wait until final output done
		tmp = KINETISK_SPI0.POPR;                  // drain the final RX FIFO word
	}

	inline void 
	WaitTransmitComplete(
		uint32_t inMCR)
	{
		uint32_t tmp __attribute__((unused));
		while (1) 
		{
			uint32_t sr = KINETISK_SPI0.SR;
			if (sr & SPI_SR_EOQF) break;  // wait for last transmit
			if (sr &  0xF0) tmp = KINETISK_SPI0.POPR;
		}
		KINETISK_SPI0.SR = SPI_SR_EOQF;
		SPI0_MCR = inMCR;
		while (KINETISK_SPI0.SR & 0xF0) 
		{
			tmp = KINETISK_SPI0.POPR;
		}
	}

	inline void 
	WriteCommand(
		uint8_t inCommand,
		bool	inLast)
	{
		if(inLast)
		{
		uint32_t mcr = SPI0_MCR;
		KINETISK_SPI0.PUSHR = inCommand | (pcs_command << 16) | SPI_PUSHR_CTAS(0) | SPI_PUSHR_EOQ;
		WaitTransmitComplete(mcr);
		}
		else
		{
			KINETISK_SPI0.PUSHR = inCommand | (pcs_command << 16) | SPI_PUSHR_CTAS(0) | SPI_PUSHR_CONT;
			WaitFifoNotFull();
		}
	}

	inline void 
	WriteData8(
		uint8_t inData8,
		bool	inLast)
	{
		if(inLast)
		{
			uint32_t mcr = SPI0_MCR;
			KINETISK_SPI0.PUSHR = inData8 | (pcs_data << 16) | SPI_PUSHR_CTAS(0) | SPI_PUSHR_EOQ;
			WaitTransmitComplete(mcr);
		}
		else
		{
			KINETISK_SPI0.PUSHR = inData8 | (pcs_data << 16) | SPI_PUSHR_CTAS(0) | SPI_PUSHR_CONT;
			WaitFifoNotFull();
		}
	}

	inline void 
	WriteData16(
		uint16_t	inData16,
		bool		inLast)
	{
		if(inLast)
		{
			uint32_t mcr = SPI0_MCR;
			KINETISK_SPI0.PUSHR = inData16 | (pcs_data << 16) | SPI_PUSHR_CTAS(1) | SPI_PUSHR_EOQ;
			WaitTransmitComplete(mcr);
		}
		else
		{
			KINETISK_SPI0.PUSHR = inData16 | (pcs_data << 16) | SPI_PUSHR_CTAS(1) | SPI_PUSHR_CONT;
			WaitFifoNotFull();
		}
	}

	inline void 
	SetAddr(
		uint16_t x0, 
		uint16_t y0, 
		uint16_t x1, 
		uint16_t y1)
	{
		WriteCommand(ILI9341_CASET, false); // Column addr set
		WriteData16(x0, false);   // XSTART
		WriteData16(x1, false);   // XEND
		WriteCommand(ILI9341_PASET, false); // Row addr set
		WriteData16(y0, false);   // YSTART
		WriteData16(y1, false);   // YEND
		pixelCount = (x1 - x0 + 1) * (y1 - y0 + 1);
	}

	CDisplayDriver_ILI9341(
		EDisplayOrientation	inDisplayOrientation,
		uint8_t	inCS,
		uint8_t	inDC,
		uint8_t	inMOSI,
		uint8_t	inClk,
		uint8_t	inMISO)
		:
		CModule(),
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
			WriteCommand(*addr++, false);
			while (count-- > 0)
			{
				WriteCommand(*addr++, false);
			}
			break;
		}

		WriteCommand(ILI9341_SLPOUT, true);    // Exit Sleep

		delay(120); 		
		WriteCommand(ILI9341_DISPON, false);    // Display on

		WriteCommand(ILI9341_MADCTL, false);
		switch(displayOrientation)
		{
			case eDisplayOrientation_LandscapeUpside:
				displayWidth = ILI9341_TFTHEIGHT;
				displayHeight = ILI9341_TFTWIDTH;
				WriteData8(MADCTL_MV | MADCTL_BGR, true);
				break;

			case eDisplayOrientation_LandscapeUpsideDown:
				displayWidth = ILI9341_TFTHEIGHT;
				displayHeight = ILI9341_TFTWIDTH;
				WriteData8(MADCTL_MX | MADCTL_MY | MADCTL_MV | MADCTL_BGR, true);
				break;

			case eDisplayOrientation_PortraitUpside:
				displayWidth = ILI9341_TFTWIDTH;
				displayHeight = ILI9341_TFTHEIGHT;
				WriteData8(MADCTL_MX | MADCTL_BGR, true);
				break;

			case eDisplayOrientation_PortraitUpsideDown:
				WriteData8(MADCTL_MY | MADCTL_BGR, true);
				displayWidth = ILI9341_TFTWIDTH;
				displayHeight = ILI9341_TFTHEIGHT;
				break;

			default:
				WriteData8(MADCTL_MV | MADCTL_BGR, true);
				SystemMsg("Unknown orientation");
		}

		SPI.endTransaction();

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

		SPI.beginTransaction(SPISettings(SPICLOCK, MSBFIRST, SPI_MODE0));
		SetAddr(0, 0, displayWidth - 1, displayHeight - 1);
		WriteCommand(ILI9341_RAMWR, false);

		uint16_t	color = inColor.GetRGB565();
		for(int32_t y = displayWidth * displayHeight; y > 1; y--)
		{
			WriteData16(color, false);
		}
		WriteData16(color, true);

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

		SetAddr(clippedRect.topLeft.x, clippedRect.topLeft.y, clippedRect.bottomRight.x - 1, clippedRect.bottomRight.y - 1);
		WriteCommand(ILI9341_RAMWR, false);

		SDisplayPoint dimensions = clippedRect.GetDimensions();
		uint16_t	color = inColor.GetRGB565();
		for(int32_t y = dimensions.y * dimensions.x; y > 1; y--)
		{
			WriteData16(color, false);
		}
		WriteData16(color, true);

		SPI.endTransaction();
	}

	virtual void
	DrawRect(
		SDisplayRect const&		inRect,
		SDisplayColor const&	inColor)
	{
		DrawHLine(inRect.topLeft, inRect.GetWidth(), inColor);
		DrawHLine(SDisplayPoint(inRect.topLeft.x, inRect.bottomRight.y - 1), inRect.GetWidth(), inColor);
		DrawVLine(inRect.topLeft, inRect.GetHeight(), inColor);
		DrawVLine(SDisplayPoint(inRect.bottomRight.x - 1, inRect.topLeft.y), inRect.GetHeight(), inColor);
	}

	virtual void
	DrawLine(
		SDisplayPoint const&	inPointA,
		SDisplayPoint const&	inPointB,
		SDisplayColor const&	inColor)
	{
		
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
		SetAddr(clippedRect.topLeft.x, clippedRect.topLeft.y, clippedRect.bottomRight.x - 1, clippedRect.bottomRight.y - 1);
		WriteCommand(ILI9341_RAMWR, false);
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
					WriteData16(fgColor, pixelCount == 1);
				}
				else
				{
					WriteData16(bgColor, pixelCount == 1);
				}
				--pixelCount;
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
					WriteData16(fgColor, pixelCount == 1);
				}
				else
				{
					WriteData16(bgColor, pixelCount == 1);
				}
				--pixelCount;
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

		SPI.endTransaction();
	}

	void
	DrawHLine(
		SDisplayPoint const&	inStart,
		int16_t					inPixelsRight,
		SDisplayColor const&	inColor)
	{
		if(inStart.y < 0 || inStart.y >= displayHeight)
		{
			return;
		}

		int16_t	startX = inStart.x;
		int16_t	endX = startX + inPixelsRight;
		if(startX < 0)
		{
			startX = 0;
		}

		if(endX >= displayWidth)
		{
			endX = displayWidth;
		}

		if(startX >= endX)
		{
			return;
		}

		SPI.beginTransaction(SPISettings(SPICLOCK, MSBFIRST, SPI_MODE0));
		SetAddr(startX, inStart.y, endX - 1, inStart.y);
		WriteCommand(ILI9341_RAMWR, false);

		uint16_t	color = inColor.GetRGB565();
		for(int i = 0; i < endX - startX - 1; ++i)
		{
			WriteData16(color, false);
		}

		WriteData16(color, true);
		SPI.endTransaction();
	}

	void
	DrawVLine(
		SDisplayPoint const&	inStart,
		int16_t					inPixelsDown,
		SDisplayColor const&	inColor)
	{
		if(inStart.x < 0 || inStart.x >= displayWidth)
		{
			return;
		}

		int16_t	startY = inStart.y;
		int16_t	endY = startY + inPixelsDown;
		if(startY < 0)
		{
			startY = 0;
		}

		if(endY >= displayHeight)
		{
			endY = displayHeight;
		}

		if(startY >= endY)
		{
			return;
		}

		SPI.beginTransaction(SPISettings(SPICLOCK, MSBFIRST, SPI_MODE0));
		SetAddr(inStart.x, startY, inStart.x, endY - 1);
		WriteCommand(ILI9341_RAMWR, false);

		uint16_t	color = inColor.GetRGB565();
		for(int i = 0; i < endY - startY - 1; ++i)
		{
			WriteData16(color, false);
		}

		WriteData16(color, true);
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

	int32_t			pixelCount;
};

#endif
#endif
