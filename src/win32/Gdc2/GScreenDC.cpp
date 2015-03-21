/*hdr
**	FILE:		Gdc2.h
**	AUTHOR:		Matthew Allen
**	DATE:		20/2/97
**	DESCRIPTION:	GDC v2.xx header
**
**	Copyright (C) 1997, Matthew Allen
**		fret@memecode.com
*/

#include <stdio.h>
#include <math.h>

#include "Gdc2.h"
#include "GdiLeak.h"
#include "GPalette.h"

class GScreenPrivate
{
public:
	#if defined WIN32

	bool		Release;
	bool		End;
	HWND		hWnd;
	PAINTSTRUCT	Ps;
	HPEN		hPen;
	HBRUSH		hBrush;
	HBITMAP		hOldBitmap;
	GRect		Client;

	int			Mode;
	COLOUR		Col;
	int			Sx, Sy;
	GGlobalColour *Gc;
	
	class NullObjects
	{
		friend class GScreenDC;
		HPEN Pen;
		HBRUSH Brush;

	public:
		NullObjects()
		{
			LOGBRUSH LogBrush;
			LogBrush.lbStyle = BS_NULL;
			LogBrush.lbColor = 0;
			LogBrush.lbHatch = 0;
			Brush = CreateBrushIndirect(&LogBrush);
			Pen = CreatePen(PS_NULL, 1, 0);
		}

		~NullObjects()
		{
			DeleteObject(Pen);
			DeleteObject(Brush);
		}
	};

	static GPalette *LastRealized;
	static NullObjects Null;

	GScreenPrivate()
	{
		Gc = 0;
		hWnd = 0;
		hPen = 0;
		hBrush = 0;
		hOldBitmap = 0;
		Mode = GDC_SET;
		Col = 0;
		Sx = Sy = 0;
		Release = End = false;
		Client.ZOff(-1, -1);
	}

	#else

		OsView		View;
		int			BitDepth;

		bool		_ClientClip;
		GRect		_Client;
		void		_SetClient(GRect *c);

		#ifdef LINUX
		friend class GFont;
		friend class GView;
		
		COLOUR		Col;
		QPainter	p;
		#endif

	#endif
};

GScreenPrivate::NullObjects GScreenPrivate::Null;
GPalette *GScreenPrivate::LastRealized = 0;


/////////////////////////////////////////////////////////////////////////////////////////////////////
GScreenDC::GScreenDC()
{
	d = new GScreenPrivate;
	ColourSpace = CsNone;
}

GScreenDC::GScreenDC(GViewI *view)
{
	d = new GScreenPrivate;
	ColourSpace = GdcD->GetColourSpace();

	d->hWnd = view->Handle();
	d->End = true;
	Create(BeginPaint(d->hWnd, &d->Ps));

	RECT rc;
	GetClientRect(d->hWnd, &rc);
	SetSize(rc.right-rc.left, rc.bottom-rc.top);
}

GScreenDC::GScreenDC(HWND hWindow)
{
	d = new GScreenPrivate;
	ColourSpace = GdcD->GetColourSpace();

	d->hWnd = hWindow;
	d->End = true;
	Create(BeginPaint(d->hWnd, &d->Ps));

	RECT rc;
	GetClientRect(d->hWnd, &rc);
	SetSize(rc.right-rc.left, rc.bottom-rc.top);
}

GScreenDC::GScreenDC(HDC hdc, HWND hwnd, bool Release)
{
	d = new GScreenPrivate;
	ColourSpace = GdcD->GetColourSpace();

	LgiAssert(hdc);
	d->hWnd = hwnd;
	Create(hdc);
	d->Release = Release;

	RECT rc;
	if (GetWindowRect(d->hWnd, &rc))
	{
		SetSize(rc.right-rc.left, rc.bottom-rc.top);
	}
}

GScreenDC::GScreenDC(HBITMAP hbmp, int Sx, int Sy)
{
	d = new GScreenPrivate;
	ColourSpace = GdcD->GetColourSpace();

	Create(CreateCompatibleDC(0));
	hBmp = hbmp;
	d->hOldBitmap = (HBITMAP) SelectObject(hDC, hBmp);
	SetSize(Sx, Sy);
}

GScreenDC::~GScreenDC()
{
	if (hDC)
	{
		if (d->hPen)
		{
			d->hPen = (HPEN) SelectObject(hDC, d->hPen);
			if (d->hPen)
			{
				DeleteObject(d->hPen);
			}
		}
		if (d->hBrush)
		{
			d->hBrush = (HBRUSH) SelectObject(hDC, d->hBrush);
			if (d->hBrush)
			{
				DeleteObject(d->hBrush);
			}
		}

		if (d->hOldBitmap)
		{
			SelectObject(hDC, d->hOldBitmap);
			DeleteDC(hDC);
			hDC = 0;
		}
	}

	if (d->End)
	{
		EndPaint(d->hWnd, &d->Ps);
	}
	else if (d->Release)
	{
		ReleaseDC(d->hWnd, hDC);
	}

	DeleteObj(d);
}

bool GScreenDC::GetClient(GRect *c)
{
	if (!c) return false;
	*c = d->Client;
	return true;
}

void GScreenDC::SetClient(GRect *c)
{
	if (c)
	{
		SetWindowOrgEx(hDC, 0, 0, NULL);
		SelectClipRgn(hDC, 0);
		
		HRGN hRgn = CreateRectRgn(c->x1, c->y1, c->x2+1, c->y2+1);
		if (hRgn)
		{
			SelectClipRgn(hDC, hRgn);
			DeleteObject(hRgn);
		}

		SetWindowOrgEx(hDC, -c->x1, -c->y1, NULL);
		d->Client = *c;
	}
	else
	{
		d->Client.ZOff(-1, -1);
		SetWindowOrgEx(hDC, 0, 0, NULL);
		SelectClipRgn(hDC, 0);
	}
}

void GScreenDC::SetSize(int x, int y)
{
	d->Sx = x;
	d->Sy = y;
	Clip.ZOff(d->Sx-1, d->Sy-1);
}

bool GScreenDC::Create(HDC hdc)
{
	bool Status = FALSE;

	hDC = hdc;
	if (hdc)
	{
		LOGBRUSH LogBrush;
		LogBrush.lbStyle = BS_SOLID;
		LogBrush.lbColor = 0;
		LogBrush.lbHatch = 0;
		d->hBrush = (HBRUSH) SelectObject(hDC, CreateBrushIndirect(&LogBrush));
		d->hPen = (HPEN) SelectObject(hDC, CreatePen(PS_SOLID, 1, 0));

		Status = TRUE;
	}

	if (GetBits() == 8 && d->hWnd)
	{
		d->Gc = GdcD->GetGlobalColour();
		if (d->Gc)
		{
			GPalette *Pal = d->Gc->GetPalette();
			if (Pal)
			{
				HPALETTE hpal = SelectPalette(hdc, Pal->Handle(), false);
				RealizePalette(hdc);
			}
		}
	}

	return Status;
}

void GScreenDC::GetOrigin(int &x, int &y)
{
	POINT pt;
	if (GetWindowOrgEx(hDC, &pt))
	{
		x = pt.x;
		y = pt.y;
	}
	else
	{
		x = y = 0;
	}
}

void GScreenDC::SetOrigin(int x, int y)
{
	GSurface::SetOrigin(x, y);
	if (hDC)
	{
		SetWindowOrgEx(hDC, x, y, NULL);
	}
}

GPalette *GScreenDC::Palette()
{
	return GSurface::Palette();
}

void GScreenDC::Palette(GPalette *pPal, bool bOwnIt)
{
	GSurface::Palette(pPal, bOwnIt);
}

GRect GScreenDC::ClipRgn(GRect *Rgn)
{
	GRect Prev = Clip;

	if (Rgn)
	{
		POINT Origin;
		GetWindowOrgEx(hDC, &Origin);

		Clip.x1 = max(Rgn->x1 - Origin.x, 0);
		Clip.y1 = max(Rgn->y1 - Origin.y, 0);
		Clip.x2 = min(Rgn->x2 - Origin.x, d->Sx-1);
		Clip.y2 = min(Rgn->y2 - Origin.y, d->Sy-1);

		HRGN hRgn = CreateRectRgn(Clip.x1, Clip.y1, Clip.x2+1, Clip.y2+1);
		if (hRgn)
		{
			SelectClipRgn(hDC, hRgn);
			DeleteObject(hRgn);
		}
	}
	else
	{
		Clip.x1 = 0;
		Clip.y1 = 0;
		Clip.x2 = X()-1;
		Clip.y2 = Y()-1;

		SelectClipRgn(hDC, NULL);
	}

	return Prev;
}

GRect GScreenDC::ClipRgn()
{
	return Clip;
}

COLOUR GScreenDC::Colour(COLOUR c, int Bits)
{
	COLOUR Prev = d->Col;

	if (hDC)
	{
		d->hPen = (HPEN) SelectObject(hDC, d->hPen);
		d->hBrush = (HBRUSH) SelectObject(hDC, d->hBrush);
		DeleteObject(d->hPen);
		DeleteObject(d->hBrush);

		if (Bits)
		{
			d->Col = CBit(24, c, Bits, pPalette);
		}
		else
		{
			d->Col = CBit(24, c, GetBits(), pPalette);
		}

		if (d->Gc)
		{
			// Give the global colour manager a chance to map the
			// colour to a palette index (8 bit only)
			d->Col = d->Gc->GetColour(d->Col);
		}

		uint32 WinCol = RGB( R24(d->Col), G24(d->Col), B24(d->Col) );
		LOGBRUSH LogBrush;
		LogBrush.lbStyle = BS_SOLID;
		LogBrush.lbColor = WinCol;
		LogBrush.lbHatch = 0;
		d->hBrush = (HBRUSH) SelectObject(hDC, CreateBrushIndirect(&LogBrush));
		
		if (LineBits == 0xffffffff)
		{
			d->hPen = (HPEN) SelectObject(hDC, CreatePen(PS_SOLID, 1, WinCol));
		}
		else
		{
			int Type = PS_SOLID;
			switch (LineBits)
			{
				case LineNone:
					Type = PS_NULL;
					break;
				case LineSolid:
					Type = PS_SOLID;
					break;
				case LineAlternate:
					Type = PS_ALTERNATE;
					break;
				case LineDash:
					Type = PS_DASH;
					break;
				case LineDot:
					Type = PS_DOT;
					break;
				case LineDashDot:
					Type = PS_DASHDOT;
					break;
				case LineDashDotDot:
					Type = PS_DASHDOTDOT;
					break;
			}
			LOGBRUSH br = { BS_SOLID, WinCol, HS_VERTICAL };
			d->hPen = (HPEN) SelectObject(hDC, ExtCreatePen(PS_COSMETIC | Type, 1, &br, 0, NULL));
		}
	}

	return Prev;
}

GColour GScreenDC::Colour(GColour c)
{
	GColour cPrev(d->Col, GetBits());
	Colour(c.c32(), 32);
	return cPrev;
}

int GScreenDC::Op(int Op)
{
	int Prev = d->Mode;
	int Rop;

	switch (Op)
	{
		case GDC_ALPHA:
		case GDC_SET:
		{
			Rop = R2_COPYPEN;
			break;
		}
		case GDC_AND:
		{
			Rop = R2_MASKPEN;
			break;
		}
		case GDC_OR:
		{
			Rop = R2_MERGEPEN;
			break;
		}
		case GDC_XOR:
		{
			Rop = R2_XORPEN;
			break;
		}
		default:
		{
			return Prev;
		}
	}

	SetROP2(hDC, Rop);
	d->Mode = Op;

	return Prev;
}

COLOUR GScreenDC::Colour()
{
	return d->Col;
}

int GScreenDC::Op()
{
	return d->Mode;
}

int GScreenDC::X()
{
	if (d->Client.Valid())
		return d->Client.X();

	return d->Sx;
}

int GScreenDC::Y()
{
	if (d->Client.Valid())
		return d->Client.Y();

	return d->Sy;
}

int GScreenDC::GetBits()
{
	return GetDeviceCaps(hDC, BITSPIXEL) * GetDeviceCaps(hDC, PLANES);
}

bool GScreenDC::SupportsAlphaCompositing()
{
	// Windows does support blending screen content with bitmaps that have alpha
	return true;
}

void GScreenDC::Set(int x, int y)
{
	uint32 WinCol = RGB( R24(d->Col), G24(d->Col), B24(d->Col) );
	SetPixel(hDC, x, y, WinCol);
}

COLOUR GScreenDC::Get(int x, int y)
{
	return GetPixel(hDC, x, y);
}

uint GScreenDC::LineStyle(uint32 Bits, uint32 Reset)
{
	return LineBits = Bits;
}

uint GScreenDC::LineStyle()
{
	return LineBits;
}

void GScreenDC::HLine(int x1, int x2, int y)
{
	if (x1 > x2)
	{
		int t = x1;
		x1 = x2;
		x2 = t;
	}
	MoveToEx(hDC, x1, y, NULL);
	LineTo(hDC, x2 + 1, y);
}

void GScreenDC::VLine(int x, int y1, int y2)
{
	if (y1 > y2)
	{
		int t = y1;
		y1 = y2;
		y2 = t;
	}
	MoveToEx(hDC, x, y1, NULL);
	LineTo(hDC, x, y2 + 1);
}

void GScreenDC::Line(int x1, int y1, int x2, int y2)
{
	MoveToEx(hDC, x1, y1, NULL);
	LineTo(hDC, x2, y2);
	uint32 WinCol = RGB( R24(d->Col), G24(d->Col), B24(d->Col) );
	SetPixel(hDC, x2, y2, WinCol);
}

void GScreenDC::Circle(double cx, double cy, double radius)
{
	HBRUSH hTemp = (HBRUSH) SelectObject(hDC, d->Null.Brush);
	::Ellipse(	hDC,
				(int)floor(cx - radius),
				(int)floor(cy - radius),
				(int)ceil(cx + radius),
				(int)ceil(cy + radius));
	SelectObject(hDC, hTemp);
}

void GScreenDC::FilledCircle(double cx, double cy, double radius)
{
	::Ellipse(	hDC,
				(int)floor(cx - radius),
				(int)floor(cy - radius),
				(int)ceil(cx + radius),
				(int)ceil(cy + radius));
}

void GScreenDC::Arc(double cx, double cy, double radius, double start, double end)
{
	int StartX = (int)(cx + (cos(start) * radius));
	int StartY = (int)(cy + (int)(sin(start) * radius));
	int EndX = (int)(cx + (int)(cos(end) * radius));
	int EndY = (int)(cy + (int)(sin(end) * radius));

	::Arc(	hDC,
			(int)floor(cx - radius),
			(int)floor(cy - radius),
			(int)ceil(cx + radius),
			(int)ceil(cy + radius),
			StartX, StartY,
			EndX, EndY);
}

void GScreenDC::FilledArc(double cx, double cy, double radius, double start, double end)
{
	int StartX = (int)(cx + (cos(start) * radius));
	int StartY = (int)(cy + (int)(sin(start) * radius));
	int EndX = (int)(cx + (int)(cos(end) * radius));
	int EndY = (int)(cy + (int)(sin(end) * radius));

	::Pie(	hDC,
			(int)floor(cx - radius),
			(int)floor(cy - radius),
			(int)ceil(cx + radius),
			(int)ceil(cy + radius),
			StartX, StartY,
			EndX, EndY);
}

void GScreenDC::Ellipse(double cx, double cy, double x, double y)
{
	HBRUSH hTemp = (HBRUSH) SelectObject(hDC, d->Null.Brush);
	::Ellipse(	hDC,
				(int)floor(cx - x),
				(int)floor(cy - y),
				(int)ceil(cx + x),
				(int)ceil(cy + y)
				);
	SelectObject(hDC, hTemp);
}

void GScreenDC::FilledEllipse(double cx, double cy, double x, double y)
{
	::Ellipse(	hDC,
				(int)floor(cx - x),
				(int)floor(cy - y),
				(int)ceil(cx + x),
				(int)ceil(cy + y)
				);
}

void GScreenDC::Box(int x1, int y1, int x2, int y2)
{
	HBRUSH hTemp = (HBRUSH) SelectObject(hDC, d->Null.Brush);
	::Rectangle(hDC, x1, y1, x2+1, y2+1);
	SelectObject(hDC, hTemp);
}

void GScreenDC::Box(GRect *a)
{
	HBRUSH hTemp = (HBRUSH) SelectObject(hDC, d->Null.Brush);
	if (a)
	{
		::Rectangle(hDC, a->x1, a->y1, a->x2+1, a->y2+1);
	}
	else
	{
		::Rectangle(hDC, 0, 0, X(), Y());
	}
	SelectObject(hDC, hTemp);
}

void GScreenDC::Rectangle(int x1, int y1, int x2, int y2)
{
	::Rectangle(hDC, x1, y1, x2+1, y2+1);
}

void GScreenDC::Rectangle(GRect *a)
{
	GRect b;
	if (a)
	{
		b = *a;
	}
	else
	{
		b.ZOff(X()-1, Y()-1);
	}

	::Rectangle(hDC, b.x1, b.y1, b.x2+1, b.y2+1);
}

void GScreenDC::Blt(int x, int y, GSurface *Src, GRect *a)
{
	if (Src)
	{
		GRect b;
		if (a)
		{
			b = *a;
		}
		else
		{
			b.ZOff(Src->X()-1, Src->Y()-1);
		}

		int RowOp;

		switch (d->Mode)
		{
			case GDC_SET:
			{
				RowOp = SRCCOPY;
				break;
			}
			case GDC_AND:
			{
				RowOp = SRCAND;
				break;
			}
			case GDC_OR:
			{
				RowOp = SRCPAINT;
				break;
			}
			case GDC_XOR:
			{
				RowOp = SRCINVERT;
				break;
			}
			case GDC_ALPHA:
			{
				if (GdcD->AlphaBlend &&
					Src->GetBits() == 32)
				{
					HDC hDestDC = StartDC();
					HDC hSrcDC = Src->StartDC();

					BLENDFUNCTION Blend;
					Blend.BlendOp = AC_SRC_OVER;
					Blend.BlendFlags = 0;
					Blend.SourceConstantAlpha = 255;
					Blend.AlphaFormat = AC_SRC_ALPHA;

					if (!GdcD->AlphaBlend(	hDestDC,
											x, y,
											b.X(), b.Y(),
											hSrcDC,
											b.x1, b.y1,
											b.X(), b.Y(),
											Blend))
					{
						printf("%s:%i - AlphaBlend failed.\n", _FL);
					}

					Src->EndDC();
					EndDC();

					return;
				}
				else
				{
					/*
					GSurface *Temp = new GMemDC(b.X(), b.Y(), 32);
					if (Temp)
					{
						POINT p = {x, y};
						ClientToScreen(hWnd, &p);

						HDC hTempDC = Temp->StartDC();

						HDC hSrcDC = GetWindowDC(GetDesktopWindow());
						BitBlt(hTempDC, 0, 0, b.X(), b.Y(), hSrcDC, p.x, p.y, SRCCOPY);
						ReleaseDC(hWnd, hSrcDC);

						Temp->Op(GDC_ALPHA);
						Temp->Blt(0, 0, Src);

						HDC hDestDC = StartDC();
						BitBlt(hDestDC, x, y, b.X(), b.Y(), hTempDC, 0, 0, SRCCOPY);
						EndDC();

						Temp->EndDC();

						DeleteObj(Temp);
						return;
					}
					*/

					// Lame '95
					RowOp = SRCCOPY;
				}
				break;
			}
			default:
			{
				return;
			}
		}

		HDC hDestDC = StartDC();
		HDC hSrcDC = Src->StartDC();

		GPalette *Pal = Src->DrawOnAlpha() ? NULL : Src->Palette();
		HPALETTE sPal = 0, dPal = 0;
		if (Pal)
		{
			Src->Update(-1);
			dPal = SelectPalette(hDestDC, Pal->Handle(), false);
		}

		POINT Old;
		GetWindowOrgEx(hDestDC, &Old);
		int RealX = x - Old.x;
		int RealY = y - Old.y;

		BitBlt(hDestDC, x, y, b.X(), b.Y(), hSrcDC, b.x1, b.y1, RowOp);

		if (Pal)
		{
			dPal = SelectPalette(hDestDC, dPal, false);
		}
		
		Src->EndDC();
		EndDC();
	}
}

void GScreenDC::StretchBlt(GRect *dst, GSurface *Src, GRect *s)
{
	if (Src)
	{
		GRect DestR;
		if (dst)
		{
			DestR = *dst;
		}
		else
		{
			DestR.ZOff(X()-1, Y()-1);
		}

		GRect SrcR;
		if (s)
		{
			SrcR = *s;
		}
		else
		{
			SrcR.ZOff(Src->X()-1, Src->Y()-1);
		}

		int RowOp;

		switch (d->Mode)
		{
			case GDC_SET:
			{
				RowOp = SRCCOPY;
				break;
			}
			case GDC_AND:
			{
				RowOp = SRCAND;
				break;
			}
			case GDC_OR:
			{
				RowOp = SRCPAINT;
				break;
			}
			case GDC_XOR:
			{
				RowOp = SRCINVERT;
				break;
			}
			default:
			{
				return;
			}
		}

		HDC hDestDC = StartDC();
		HDC hSrcDC = Src->StartDC();
		::StretchBlt(hDestDC, DestR.x1, DestR.y1, DestR.X(), DestR.Y(), hSrcDC, SrcR.x1, SrcR.y1, SrcR.X(), SrcR.Y(), RowOp);
		Src->EndDC();
		EndDC();
	}
}

void GScreenDC::Polygon(int Points, GdcPt2 *Data)
{
	if (Points > 0 && Data)
	{
		PPOINT pt = new POINT[Points];
		if (pt)
		{
			int *d = (int*)Data;

			for (int i=0; i<Points; i++)
			{
				pt[i].x = *d++;
				pt[i].y = *d++;
			}

			::Polygon(hDC, pt, Points);
			DeleteArray(pt);
		}
	}
}

void GScreenDC::Bezier(int Threshold, GdcPt2 *Pt)
{
}

void GScreenDC::FloodFill(int x, int y, int Mode, COLOUR Border, GRect *r)
{
	::FloodFill(hDC, x, y, d->Col);
}

