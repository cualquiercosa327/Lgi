/*hdr
**	FILE:			GFont_W.cpp
**	AUTHOR:			Matthew Allen
**	DATE:			19/6/2002
**	DESCRIPTION:	Unicode Font Support
**
**	Copyright (C) 2002, Matthew Allen
**		fret@memecode.com
*/

//////////////////////////////////////////////////////////////////////
// Includes
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "Lgi.h"
#if WINNATIVE
#include "GFontSelect.h"
#include "GdiLeak.h"

/////////////////////////////////////////////////////////////////////////////
void GFont::_Measure(int &x, int &y, OsChar *Str, int Len)
{
	LgiAssert(Handle());

	HDC hDC = GetSurface() ? GetSurface()->Handle() : CreateCompatibleDC(0);
	HFONT hOldFont = (HFONT) SelectObject(hDC, Handle());

	SIZE Size;
	if (GetTextExtentPoint32W(hDC, Str, Len, &Size))
	{
		x = Size.cx;
		y = Size.cy;
	}
	else
	{
		x = y = 0;
	}

	SelectObject(hDC, hOldFont);
	if (!GetSurface())
		DeleteDC(hDC);
}

int GFont::_CharAt(int x, OsChar *Str, int Len)
{
	LgiAssert(Handle());

	INT Fit = 0;
	HDC hDC = CreateCompatibleDC(GetSurface()?GetSurface()->Handle():0);
	HFONT hOldFont = (HFONT) SelectObject(hDC, Handle());

	if (hOldFont)
	{
		SIZE Size = {0, 0};

		if (!GetTextExtentExPointW(hDC, Str, Len, x, &Fit, 0, &Size))
		{
			DWORD e = GetLastError();
			Fit = -1;
		}
		
		SelectObject(hDC, hOldFont);
	}
	else
	{
		DWORD e = GetLastError();
		Fit = -1;
		LgiAssert(0);
	}

	DeleteDC(hDC);
	return Fit;
}

void GFont::_Draw(GSurface *pDC, int x, int y, OsChar *Str, int Len, GRect *r, GColour &fore)
{
	LgiAssert(Handle());

	HDC hDC = pDC->StartDC();
	HFONT hOldFont = (HFONT) SelectObject(hDC, Handle());
	if (hOldFont)
	{
		bool IsTransparent = Transparent();
		
		SetTextColor(hDC, 0xff000000 | fore.GetNative());
		if (!IsTransparent)
			SetBkColor(hDC, Back().GetNative());
		SetBkMode(hDC, IsTransparent ? TRANSPARENT : OPAQUE);

		SIZE Size;
		if ((!IsTransparent && !r)
			||
			(GetOwnerUnderline()))
		{
			GetTextExtentPoint32W(hDC, Str, Len, &Size);
		}

		if (Transparent() && !r)
		{
			TextOutW(hDC, x, y, Str, Len);
		}
		else
		{
			RECT rc;
			if (r)
				rc = *r;
			else
			{
				rc.left = x;
				rc.top = y;
				rc.right = x + Size.cx;
				rc.top = y + Size.cy;
			}
			
			ExtTextOutW(hDC, x, y, ETO_CLIPPED | (Transparent()?0:ETO_OPAQUE), &rc, Str, Len, 0);
		}

		if (GetOwnerUnderline())
		{
			pDC->Colour(fore);
			pDC->Line(x, y + GetHeight() - 1, x + Size.cx + 1, y + GetHeight() - 1);
		}

		HANDLE h = SelectObject(hDC, hOldFont);
	}
	else
	{
		DWORD e = GetLastError();
		LgiAssert(0);
	}
	pDC->EndDC();

}
#endif

