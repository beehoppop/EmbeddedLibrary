#if defined(ARDUINO) && ARDUINO >= 100
#ifndef _ELDISPLAY_ILI9341_
#define _ELDISPLAY_ILI9341_

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
		if(touchscreen.touched())
		{
			TS_Point	touch = touchscreen.getPoint();
			outTouchLoc.x = (int16_t)map(touch.x, TS_MINX, TS_MAXX, gDisplayModule->GetWidth(), 0);
			outTouchLoc.y = (int16_t)map(touch.y, TS_MINY, TS_MAXY, gDisplayModule->GetHeight(), 0);

			return true;
		}

		return false;
	}

	XPT2046_Touchscreen	touchscreen;
};

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
	DrawRect(
		SDisplayRect const&		inRect,
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
#endif
