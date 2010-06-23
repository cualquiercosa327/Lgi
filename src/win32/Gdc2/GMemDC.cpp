/*hdr
**	FILE:			GMemDC.h
**	AUTHOR:			Matthew Allen
**	DATE:			27/11/2001
**	DESCRIPTION:	GDC v2.xx header
**
**	Copyright (C) 2001, Matthew Allen
**		fret@memecode.com
*/

#include <stdio.h>
#include <math.h>

#include "Gdc2.h"
#include "GdiLeak.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////
class GMemDCPrivate
{
public:
	void		*pBits;
	PBITMAPINFO	Info;
	HPALETTE	OldPal;
	bool		UpsideDown;
	GRect		Client;

	GMemDCPrivate()
	{
		pBits = 0;
		Info = 0;
		OldPal = 0;
		Client.ZOff(-1, -1);
		UpsideDown = false;
	}
	
	~GMemDCPrivate()
	{
		DeleteObj(Info);
	}
};

GMemDC::GMemDC(int x, int y, int bits)
{
	d = new GMemDCPrivate;
	hBmp = 0;
	hDC = 0;
	pMem = 0;

	if (x AND y AND bits)
	{
		Create(x, y, bits);
	}
}

GMemDC::GMemDC(GSurface *pDC)
{
	d = new GMemDCPrivate;
	hBmp = 0;
	hDC = 0;
	pMem = 0;

	if (pDC AND
		Create(pDC->X(), pDC->Y(), pDC->GetBits()) )
	{
		if (pDC->Palette())
		{
			Palette(new GPalette(pDC->Palette()));
		}

		Blt(0, 0, pDC);

		if (pDC->AlphaDC() AND
			IsAlpha(true))
		{
			pAlphaDC->Blt(0, 0, pDC->AlphaDC());
		}
	}
}

GMemDC::~GMemDC()
{
	if (hBmp)
	{
		DeleteObject(hBmp);
		hBmp = 0;
	}
	DeleteObj(pMem);
	DeleteObj(d);
}

void GMemDC::SetClient(GRect *c)
{
	if (c)
	{
		if (hDC)
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
		}

		Clip = d->Client = *c;
		OriginX = -c->x1;
		OriginY = -c->y1;
	}
	else
	{
		d->Client.ZOff(-1, -1);

		if (hDC)
		{
			SetWindowOrgEx(hDC, 0, 0, NULL);
			SelectClipRgn(hDC, 0);
		}
		OriginX = 0;
		OriginY = 0;
		Clip.ZOff(pMem->x-1, pMem->y-1);
	}
}

void GMemDC::UpsideDown(bool upsidedown)
{
	d->UpsideDown = upsidedown;
}

bool GMemDC::Lock()
{
	return true;
}

bool GMemDC::Unlock()
{
	return true;
}

PBITMAPINFO	GMemDC::GetInfo()
{
	return d->Info;
}

void GMemDC::Update(int Flags)
{
	if (d->Info AND pPalette AND (Flags & GDC_PAL_CHANGE))
	{
		GdcRGB *p = (*pPalette)[0];
		if (p)
		{
			for (int i=0; i<pPalette->GetSize(); i++, p++)
			{
				d->Info->bmiColors[i].rgbRed = p->R;
				d->Info->bmiColors[i].rgbGreen = p->G;
				d->Info->bmiColors[i].rgbBlue = p->B;
			}

			if (hDC)
			{
				SetDIBColorTable(hDC, 0, 256, d->Info->bmiColors);
			}
		}
	}

}

HDC GMemDC::StartDC()
{
	hDC = CreateCompatibleDC(NULL);
	if (hDC)
	{
		hBmp = (HBITMAP) SelectObject(hDC, hBmp);
		SetWindowOrgEx(hDC, OriginX, OriginY, NULL);
	}

	return hDC;
}

void GMemDC::EndDC()
{
	if (hDC)
	{
		hBmp = (HBITMAP) SelectObject(hDC, hBmp);
		DeleteDC(hDC);
		hDC = 0;
	}
}

GRect GMemDC::ClipRgn(GRect *Rgn)
{
	GRect Prev = Clip;

	if (Rgn)
	{
		POINT Origin = {0, 0};
		if (hDC)
		    GetWindowOrgEx(hDC, &Origin);
		else
		{
			Origin.x = OriginX;
			Origin.y = OriginY;
		}

		Clip.x1 = max(Rgn->x1 - Origin.x, 0);
		Clip.y1 = max(Rgn->y1 - Origin.y, 0);
		Clip.x2 = min(Rgn->x2 - Origin.x, pMem->x-1);
		Clip.y2 = min(Rgn->y2 - Origin.y, pMem->y-1);

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

bool GMemDC::Create(int x, int y, int Bits, int LineLen, bool KeepData)
{
	bool Status = FALSE;
	HBITMAP hOldBmp = hBmp;
	BITMAPINFO *OldInfo = d->Info;
	GBmpMem *pOldMem = pMem;

	DrawOnAlpha(FALSE);
	DeleteObj(pAlphaDC);

	hBmp = 0;
	d->Info = 0;
	pMem = 0;

	if (Bits == 15) Bits = 16;

	if ((x > 0 AND y > 0) AND (Bits == 8 OR Bits == 16 OR Bits == 24 OR Bits == 32))
	{
		int Colours = 1 << min(Bits, 8);
		int SizeOf = sizeof(BITMAPINFO)+(sizeof(RGBQUAD)*Colours);
		d->Info = (PBITMAPINFO) new char[SizeOf];
		if (d->Info)
		{
			LineLen = max((((x * Bits) + 31) / 32) * 4, LineLen);

			d->Info->bmiHeader.biSize = sizeof(d->Info->bmiHeader);
			d->Info->bmiHeader.biWidth = x;
			d->Info->bmiHeader.biHeight = d->UpsideDown ? y : -y;
			d->Info->bmiHeader.biPlanes = 1;
			d->Info->bmiHeader.biBitCount = Bits;
			d->Info->bmiHeader.biCompression = (Bits == 16 OR Bits == 32) ? BI_BITFIELDS : BI_RGB;
			d->Info->bmiHeader.biSizeImage = LineLen * y;
			d->Info->bmiHeader.biXPelsPerMeter = 3000;
			d->Info->bmiHeader.biYPelsPerMeter = 3000;
			d->Info->bmiHeader.biClrUsed = 0;
			d->Info->bmiHeader.biClrImportant = 0;

			if (Bits == 16)
			{
				int *BitFeilds = (int*) d->Info->bmiColors;
				BitFeilds[0] = 0xF800;
				BitFeilds[1] = 0x07E0;
				BitFeilds[2] = 0x001F;
			}
			else if (Bits == 32)
			{
				int *BitFeilds = (int*) d->Info->bmiColors;
				BitFeilds[0] = 0x00FF0000;
				BitFeilds[1] = 0x0000FF00;
				BitFeilds[2] = 0x000000FF;
			}

			HDC hDC = GetDC(NULL);
			if (hDC)
			{
				if (Bits == 8 AND GdcD->GetBits())
				{
					PALETTEENTRY Pal[256];
					int Cols = 1 << Bits;
					GetSystemPaletteEntries(hDC, 0, Cols, Pal);
					for (int i=0; i<Cols; i++)
					{
						d->Info->bmiColors[i].rgbReserved = 0;
						d->Info->bmiColors[i].rgbRed = Pal[i].peRed;
						d->Info->bmiColors[i].rgbGreen = Pal[i].peGreen;
						d->Info->bmiColors[i].rgbBlue = Pal[i].peBlue;
					}
				}

				hBmp = CreateDIBSection(hDC, d->Info, DIB_RGB_COLORS, &d->pBits, NULL, 0);
				if (hBmp)
				{
					pMem = new GBmpMem;
					if (pMem)
					{
						if (d->UpsideDown)
						{
							pMem->Base = ((uchar*) d->pBits) + (LineLen * (y - 1));
						}
						else
						{
							pMem->Base = (uchar*) d->pBits;
						}
						pMem->x = x;
						pMem->y = y;
						pMem->Bits = Bits;
						pMem->Line = d->UpsideDown ? -LineLen : LineLen;
						pMem->Flags = 0;

						Status = TRUE;

						int NewOp = (pApp) ? Op() : GDC_SET;

						if (	(Flags & GDC_OWN_APPLICATOR) AND
							!(Flags & GDC_CACHED_APPLICATOR))
						{
							DeleteObj(pApp);
						}

						for (int i=0; i<GDC_CACHE_SIZE; i++)
						{
							DeleteObj(pAppCache[i]);
						}

						if (NewOp < GDC_CACHE_SIZE AND !DrawOnAlpha())
						{
							pApp = (pAppCache[NewOp]) ? pAppCache[NewOp] : pAppCache[NewOp] = CreateApplicator(NewOp);
							Flags &= ~GDC_OWN_APPLICATOR;
							Flags |= GDC_CACHED_APPLICATOR;
						}
						else
						{
							pApp = CreateApplicator(NewOp);
							Flags &= ~GDC_CACHED_APPLICATOR;
							Flags |= GDC_OWN_APPLICATOR;
						}

						Clip.ZOff(X()-1, Y()-1);
					}
				}
				else
				{
					DWORD Error = GetLastError();
					int k=0;
				}
			}

			ReleaseDC(NULL, hDC);
		}

		if (Status)
		{
			if (KeepData)
			{
				GApplicator *MyApp = CreateApplicator(GDC_SET, Bits);
				if (MyApp)
				{
					GBmpMem Temp = *pOldMem;
					Temp.x = min(pMem->x, pOldMem->x);
					Temp.y = min(pMem->y, pOldMem->y);

					MyApp->SetSurface(pMem);
					MyApp->c = 0;
					MyApp->SetPtr(0, 0);
					MyApp->Rectangle(pMem->x, pMem->y);
					// MyApp->Blt(&Temp, pPalette, pPalette);

					DeleteObj(MyApp);
				}
				else
				{
					Status = FALSE;
				}
			}
		}

		if (!Status)
		{
			if (hBmp)
			{
				DeleteObject(hBmp);
				hBmp = 0;
			}
			DeleteObj(d->Info);
			DeleteObj(pMem)
		}
	}

	if (hOldBmp)
	{
		DeleteObject(hOldBmp);
		hOldBmp = 0;
	}
	DeleteObj(OldInfo);
	DeleteObj(pOldMem);

	return Status;
}


void GMemDC::Blt(int x, int y, GSurface *Src, GRect *a)
{
	if (Src->IsScreen())
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

			switch (Op())
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

			/*
			GPalette *Pal = Src->Palette();
			HPALETTE hSPal = 0;
			HPALETTE hDPal = 0;
			if (Pal)
			{
				PALETTEENTRY Entries[256];
				
				GetPaletteEntries(Pal->Handle(), 0, 256, Entries);
				
				hDPal = SelectPalette(hDestDC, Pal->Handle(), FALSE);
				hSPal = SelectPalette(hSrcDC, Pal->Handle(), FALSE);
			}
			*/

			BitBlt(hDestDC, x, y, b.X(), b.Y(), hSrcDC, b.x1, b.y1, RowOp);

			/*
			if (Pal)
			{
				hDPal = SelectPalette(hDestDC, hDPal, FALSE);
				hSPal = SelectPalette(hSrcDC, hSPal, FALSE);
			}
			*/
			
			Src->EndDC();
			EndDC();
		}
	}
	else
	{
		GSurface::Blt(x, y, Src, a);
	}
}

void GMemDC::StretchBlt(GRect *d, GSurface *Src, GRect *s)
{
	if (Src)
	{
		GRect DestR;
		if (d)
		{
			DestR = *d;
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

		switch (Op())
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

void GMemDC::HLine(int x1, int x2, int y, COLOUR a, COLOUR b)
{
	if (x1 > x2) LgiSwap(x1, x2);

	if (x1 < Clip.x1) x1 = Clip.x1;
	if (x2 > Clip.x2) x2 = Clip.x2;
	if (	x1 <= x2 AND
		y >= Clip.y1 AND
		y <= Clip.y2)
	{
		COLOUR Prev = pApp->c;

		pApp->SetPtr(x1, y);
		for (; x1 <= x2; x1++)
		{
			if (x1 & 1)
			{
				pApp->c = a;
			}
			else
			{
				pApp->c = b;
			}

			pApp->Set();
			pApp->IncX();
		}

		pApp->c = Prev;
	}
}

void GMemDC::VLine(int x, int y1, int y2, COLOUR a, COLOUR b)
{
	if (y1 > y2) LgiSwap(y1, y2);
	
	if (y1 < Clip.y1) y1 = Clip.y1;
	if (y2 > Clip.y2) y2 = Clip.y2;
	if (	y1 <= y2 AND
		x >= Clip.x1 AND
		x <= Clip.x2)
	{
		COLOUR Prev = pApp->c;

		pApp->SetPtr(x, y1);
		for (; y1 <= y2; y1++)
		{
			if (y1 & 1)
			{
				pApp->c = a;
			}
			else
			{
				pApp->c = b;
			}

			pApp->Set();
			pApp->IncY();
		}

		pApp->c = Prev;
	}
}

void GMemDC::SetOrigin(int x, int y)
{
	GSurface::SetOrigin(x, y);
	if (hDC)
	{
		SetWindowOrgEx(hDC, OriginX, OriginY, NULL);
	}
}
