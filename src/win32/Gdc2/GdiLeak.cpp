#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "LgiInc.h"
#include "GdiLeak.h"

#if GDI_LEAK_DETECT

#include "GMem.h"
#include "GArray.h"

enum HndType
{
	HndNull,
	HndAnyGdi,
	HndDcPaint,
	HndDcDelete,
	HndDcRelease,
	HndBrush,
	HndFont,
	HndPen,
	HndBitmap,
	HndRegion,
	HndPalette,
};

struct HndInfo
{
	HndType Type;
	uint32 Hnd;
	char *File;
	int Line;
};

static GArray<HndInfo> Handles;
LgiFunc void LgiTrace(char *Format, ...);

class GdiCleanup
{
public:
	~GdiCleanup()
	{
		if (Handles.Length() > 0)
		{
			for (int i=0; i<Handles.Length(); i++)
			{
				HndInfo *h = &Handles[i];
				LgiTrace("GdiResourceLeak: Type=%i @ %s:%i\n", h->Type, h->File, h->Line);
			}
		}
	}
} Cleanup;

static HndInfo *GetHandle(uint32 h)
{
	for (int i=0; i<Handles.Length(); i++)
	{
		if (Handles[i].Hnd == h)
		{
			return &Handles[i];
		}
	}

	return 0;
}

static void AddHandle(char *f, int l, uint32 hnd, HndType t)
{
	HndInfo *h = &Handles[Handles.Length()];
	if (h)
	{
		h->Type = t;
		h->File = f;
		h->Line = l;
		h->Hnd = hnd;
	}
}

static void DeleteHandle(char *f, int l, uint32 hnd, HndType t)
{
	int i;
	HndInfo *h = 0;
	
	if (!&Handles[0])
		return;

	for (i=0; i<Handles.Length(); i++)
	{
		HndInfo *k = &Handles[i];
		if (k->Hnd == hnd)
		{
			h = k;
			break;
		}
	}

	if (h)
	{
		if (h->Type == t OR (h->Type >= HndBrush AND t == HndAnyGdi))
		{
			if (Handles.Length() > 0)
			{
				Handles[i] = Handles[Handles.Length()-1];
			}
			Handles.Length(Handles.Length()-1);
		}
		else
		{
			// Wrong type of handle
			LgiAssert(0);
		}
	}
	else
	{
		// Handle wasn't allocated
		LgiAssert(0);
	}
}

HDC Lgi_BeginPaint(char *File, int Line, HWND hWnd, LPPAINTSTRUCT lpPaint)
{
	#undef BeginPaint

	HDC hdc = BeginPaint(hWnd, lpPaint);
	if (hdc)
	{
		AddHandle(File, Line, (uint32)hdc, HndDcPaint);
	}
	else
	{
		LgiAssert(0);
	}

	return hdc;
}

BOOL Lgi_EndPaint(char *File, int Line, HWND hWnd, CONST PAINTSTRUCT *lpPaint)
{
	#undef EndPaint
	DeleteHandle(File, Line, (uint32)lpPaint->hdc, HndDcPaint);

	// LgiTrace("Handles=%i\n", Handles.Length());

	return EndPaint(hWnd, lpPaint);
}

HDC Lgi_CreateCompatibleDC(char *File, int Line, HDC hInDC)
{
	#undef CreateCompatibleDC

	HDC hdc = CreateCompatibleDC(hInDC);
	if (hdc)
	{
		AddHandle(File, Line, (uint32)hdc, HndDcDelete);
	}
	else
	{
		LgiAssert(0);
	}

	return hdc;
}

BOOL Lgi_DeleteDC(char *File, int Line, HDC hdc)
{
	#undef DeleteDC
	DeleteHandle(File, Line, (uint32)hdc, HndDcDelete);
	return DeleteDC(hdc);
}

HDC Lgi_GetDC(char *File, int Line, HWND hWnd)
{
	#undef GetDC
	HDC hdc = GetDC(hWnd);
	if (hdc)
	{
		AddHandle(File, Line, (uint32)hdc, HndDcRelease);
	}
	else
	{
		LgiAssert(0);
	}
	return hdc;
}

HDC Lgi_GetWindowDC(char *File, int Line, HWND hWnd)
{
	#undef GetWindowDC
	HDC hdc = GetWindowDC(hWnd);
	if (hdc)
	{
		AddHandle(File, Line, (uint32)hdc, HndDcRelease);
	}
	else
	{
		LgiAssert(0);
	}
	return hdc;
}

int Lgi_ReleaseDC(char *File, int Line, HWND hWnd, HDC hDC)
{
	#undef ReleaseDC
	DeleteHandle(File, Line, (uint32)hDC, HndDcRelease);
	return ReleaseDC(hWnd, hDC);
}

HBRUSH Lgi_CreateBrushIndirect(char *File, int Line, CONST LOGBRUSH *LogBrush)
{
	#undef CreateBrushIndirect
	HBRUSH h = CreateBrushIndirect(LogBrush);
	if (h)
	{
		AddHandle(File, Line, (uint32)h, HndBrush);
	}
	else
	{
		LgiAssert(0);
	}
	return h;
}

HBRUSH Lgi_CreateDIBPatternBrush(char *File, int Line, HGLOBAL hglbDIBPacked, UINT fuColorSpec)
{
	#undef CreateDIBPatternBrush
	HBRUSH h = CreateDIBPatternBrush(hglbDIBPacked, fuColorSpec);
	if (h)
	{
		AddHandle(File, Line, (uint32)h, HndBrush);
	}
	else
	{
		LgiAssert(0);
	}
	return h;
}

HBRUSH Lgi_CreateDIBPatternBrushPt(char *File, int Line, CONST VOID *lpPackedDIB, UINT iUsage)
{
	#undef CreateDIBPatternBrushPt
	HBRUSH h = CreateDIBPatternBrushPt(lpPackedDIB, iUsage);
	if (h)
	{
		AddHandle(File, Line, (uint32)h, HndBrush);
	}
	else
	{
		LgiAssert(0);
	}
	return h;
}

HBRUSH Lgi_CreateHatchBrush(char *File, int Line, int fnStyle, COLORREF clrref)
{
	#undef CreateHatchBrush
	HBRUSH h = CreateHatchBrush(fnStyle, clrref);
	if (h)
	{
		AddHandle(File, Line, (uint32)h, HndBrush);
	}
	else
	{
		LgiAssert(0);
	}
	return h;
}

HBRUSH Lgi_CreatePatternBrush(char *File, int Line, HBITMAP hbmp)
{
	#undef CreatePatternBrush
	HBRUSH h = CreatePatternBrush(hbmp);
	if (h)
	{
		AddHandle(File, Line, (uint32)h, HndBrush);
	}
	else
	{
		LgiAssert(0);
	}
	return h;
}

HBRUSH Lgi_CreateSolidBrush(char *File, int Line, COLORREF crColor)
{
	#undef CreateSolidBrush
	HBRUSH h = CreateSolidBrush(crColor);
	if (h)
	{
		AddHandle(File, Line, (uint32)h, HndBrush);
	}
	else
	{
		LgiAssert(0);
	}
	return h;
}

HFONT Lgi_CreateFont
(
	char *File,
	int Line,
	int nHeight,
	int nWidth,
	int nEscapement,
	int nOrientation,
	int fnWeight,
	DWORD fdwItalic,
	DWORD fdwUnderline,
	DWORD fdwStrikeOut,
	DWORD fdwCharSet,
	DWORD fdwOutputPrecision,
	DWORD fdwClipPrecision,
	DWORD fdwQuality,
	DWORD fdwPitchAndFamily,
	LPCTSTR lpszFace
)
{
	#undef CreateFontA
	HFONT h = CreateFontA(nHeight, nWidth, nEscapement, nOrientation, fnWeight, fdwItalic, fdwUnderline, fdwStrikeOut, fdwCharSet, fdwOutputPrecision, fdwClipPrecision, fdwQuality, fdwPitchAndFamily, lpszFace);
	if (h)
	{
		AddHandle(File, Line, (uint32)h, HndFont);
	}
	else
	{
		LgiAssert(0);
	}
	return h;
}

HFONT Lgi_CreateFontIndirect(char *File, int Line, CONST LOGFONT *lplf)
{
	#undef CreateFontIndirectA
	HFONT h = CreateFontIndirectA(lplf);
	if (h)
	{
		AddHandle(File, Line, (uint32)h, HndFont);
	}
	else
	{
		LgiAssert(0);
	}
	return h;
}

HPEN Lgi_CreatePen(char *File, int Line, int fnPenStyle, int nWidth, COLORREF crColor)
{
	#undef CreatePen
	HPEN h = CreatePen(fnPenStyle, nWidth, crColor);
	if (h)
	{
		AddHandle(File, Line, (uint32)h, HndPen);
	}
	else
	{
		LgiAssert(0);
	}
	return h;
}

HPEN Lgi_CreatePenIndirect(char *File, int Line, CONST LOGPEN *lplgpn)
{
	#undef CreatePenIndirect
	HPEN h = CreatePenIndirect(lplgpn);
	if (h)
	{
		AddHandle(File, Line, (uint32)h, HndPen);
	}
	else
	{
		LgiAssert(0);
	}
	return h;
}

HPEN Lgi_ExtCreatePen(char *File, int Line, DWORD dwPenStyle, DWORD dwWidth, CONST LOGBRUSH *lplb, DWORD dwStyleCount, CONST DWORD *lpStyle)
{
	#undef ExtCreatePen
	HPEN h = ExtCreatePen(dwPenStyle, dwWidth, lplb, dwStyleCount, lpStyle);
	if (h)
	{
		AddHandle(File, Line, (uint32)h, HndPen);
	}
	else
	{
		LgiAssert(0);
	}
	return h;
}

BOOL Lgi_DeleteObject(char *File, int Line, HGDIOBJ hObject)
{
	#undef DeleteObject
	DeleteHandle(File, Line, (uint32)hObject, HndAnyGdi);
	return DeleteObject(hObject);
}

HGDIOBJ Lgi_SelectObject(char *File, int Line, HDC hdc, HGDIOBJ hgdiobj)
{
	#undef SelectObject
	HGDIOBJ h = SelectObject(hdc, hgdiobj);
	LgiAssert(h);
	return h;
}

HBITMAP Lgi_CreateDIBSection(char *File, int Line, HDC hdc, CONST BITMAPINFO *pbmi, UINT iUsage, VOID **ppvBits, HANDLE hSection, DWORD dwOffset)
{
	#undef CreateDIBSection
	HBITMAP h = CreateDIBSection(hdc, pbmi, iUsage, ppvBits, hSection, dwOffset);
	if (h)
	{
		AddHandle(File, Line, (uint32)h, HndBitmap);
	}
	else
	{
		LgiAssert(0);
	}
	return h;
}

HRGN Lgi_CreateRectRgn(char *File, int Line, int a, int b, int c, int d)
{
	#undef CreateRectRgn
	HRGN h = CreateRectRgn(a, b, c, d);
	if (h)
	{
		AddHandle(File, Line, (uint32)h, HndRegion);
	}
	else
	{
		LgiAssert(0);
	}
	return h;
}

HPALETTE Lgi_CreatePalette(char *File, int Line, CONST LOGPALETTE *lplgpl)
{
	#undef CreatePalette
	HPALETTE h = CreatePalette(lplgpl);
	if (h)
	{
		AddHandle(File, Line, (uint32)h, HndPalette);
	}
	else
	{
		LgiAssert(0);
	}
	return h;
}

#endif