#ifdef WIN32
#ifndef _ELDISPLAY_ILI9341WIN_
#define _ELDISPLAY_ILI9341WIN_
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
*/

/*
	ABOUT

*/

#include <Windows.h>
#include <Windowsx.h>

#include "ELDisplay.h"

class ITouchDriver_XPT2046;
static ITouchDriver_XPT2046*	gTouchDriverXPT;

class ITouchDriver_XPT2046 : public ITouchDriver
{
public:

	ITouchDriver_XPT2046(
		uint8_t	inCS)
	{
		gTouchDriverXPT = this;
		mouseDown = false;
	}

	virtual bool
	GetTouch(
		SDisplayPoint&	outTouchLoc)
	{
		outTouchLoc = mouseLoc;
		return mouseDown;
	}

	bool			mouseDown;
	SDisplayPoint	mouseLoc;

};

class CDisplayDriver_ILI9341 : public CModule, public IDisplayDriver
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

		hdc = GetDC(hwnd);
		uint16_t	color = inColor.GetRGB565();
		for(int x = 0; x < displayWidth; ++x)
		{
			for(int y = 0; y < displayHeight; ++y)
			{
				displayMemory[y][x] = color;
				uint8_t		r = color >> 11;
				uint8_t		g = (color >> 5) & 0x3F;
				uint8_t		b = color & 0x1F;
				SetPixel(hdc, x, y, RGB(r << 3, g << 2, b << 3));
			}
		}
		ReleaseDC(hwnd, hdc);
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

		hdc = GetDC(hwnd);
		uint16_t	color = inColor.GetRGB565();
		for(int x = inRect.topLeft.x; x < inRect.bottomRight.x; ++x)
		{
			for(int y = inRect.topLeft.y; y < inRect.bottomRight.y; ++y)
			{
				displayMemory[y][x] = color;
				uint8_t		r = color >> 11;
				uint8_t		g = (color >> 5) & 0x3F;
				uint8_t		b = color & 0x1F;
				SetPixel(hdc, x, y, RGB(r << 3, g << 2, b << 3));
			}
		}
		ReleaseDC(hwnd, hdc);
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

		uint16_t	color = inColor.GetRGB565();

		hdc = GetDC(hwnd);
		displayMemory[inPoint.y][inPoint.x] = color;
		uint8_t		r = color >> 11;
		uint8_t		g = (color >> 5) & 0x3F;
		uint8_t		b = color & 0x1F;
		SetPixel(hdc, inPoint.x, inPoint.y, RGB(r << 3, g << 2, b << 3));
		ReleaseDC(hwnd, hdc);
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

		hdc = GetDC(hwnd);
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
					uint8_t		r = fgColor >> 11;
					uint8_t		g = (fgColor >> 5) & 0x3F;
					uint8_t		b = fgColor & 0x1F;
					SetPixel(hdc, continuousX, continuousY, RGB(r << 3, g << 2, b << 3));
				}
				else
				{
					displayMemory[continuousY][continuousX] = bgColor;
					uint8_t		r = bgColor >> 11;
					uint8_t		g = (bgColor >> 5) & 0x3F;
					uint8_t		b = bgColor & 0x1F;
					SetPixel(hdc, continuousX, continuousY, RGB(r << 3, g << 2, b << 3));
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
					uint8_t		r = fgColor >> 11;
					uint8_t		g = (fgColor >> 5) & 0x3F;
					uint8_t		b = fgColor & 0x1F;
					SetPixel(hdc, continuousX, continuousY, RGB(r << 3, g << 2, b << 3));
				}
				else
				{
					displayMemory[continuousY][continuousX] = bgColor;
					uint8_t		r = bgColor >> 11;
					uint8_t		g = (bgColor >> 5) & 0x3F;
					uint8_t		b = bgColor & 0x1F;
					SetPixel(hdc, continuousX, continuousY, RGB(r << 3, g << 2, b << 3));
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
		ReleaseDC(hwnd, hdc);
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

		hdc = GetDC(hwnd);
		uint16_t	color = inColor.GetRGB565();
		for(int i = 0; i < inPixelsRight; ++i)
		{
			int16_t	x = inStart.x + i;
			if(x >= 0 && x < displayWidth)
			{
				displayMemory[inStart.y][x] = color;
				uint8_t		r = color >> 11;
				uint8_t		g = (color >> 5) & 0x3F;
				uint8_t		b = color & 0x1F;
				SetPixel(hdc, x, inStart.y, RGB(r << 3, g << 2, b << 3));
			}
		}
		ReleaseDC(hwnd, hdc);
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

		hdc = GetDC(hwnd);
		uint16_t	color = inColor.GetRGB565();
		for(int i = 0; i < inPixelsDown; ++i)
		{
			int16_t	y = inStart.y + i;
			if(y >= 0 && y < displayHeight)
			{
				displayMemory[y][inStart.x] = color;
				uint8_t		r = color >> 11;
				uint8_t		g = (color >> 5) & 0x3F;
				uint8_t		b = color & 0x1F;
				SetPixel(hdc, inStart.x, y, RGB(r << 3, g << 2, b << 3));
			}
		}
		ReleaseDC(hwnd, hdc);
	}

private:

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
		CModule(0, 0, NULL, 10 * 1000)
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

		case WM_LBUTTONDOWN:
			if(gTouchDriverXPT != NULL)
			{
				gTouchDriverXPT->mouseLoc.x = GET_X_LPARAM(lparam); 
				gTouchDriverXPT->mouseLoc.y = GET_Y_LPARAM(lparam); 
				gTouchDriverXPT->mouseDown = true;
			}
			break;

		case WM_LBUTTONUP:
			if(gTouchDriverXPT != NULL)
			{
				gTouchDriverXPT->mouseLoc.x = GET_X_LPARAM(lparam); 
				gTouchDriverXPT->mouseLoc.y = GET_Y_LPARAM(lparam); 
				gTouchDriverXPT->mouseDown = false;
			}
			break;

		case WM_MOUSEMOVE:
			if(gTouchDriverXPT != NULL)
			{
				gTouchDriverXPT->mouseLoc.x = GET_X_LPARAM(lparam); 
				gTouchDriverXPT->mouseLoc.y = GET_Y_LPARAM(lparam);
			}
			break;
	}

	return DefWindowProc(hwnd, msg, wparam, lparam);
}
#endif
#endif
