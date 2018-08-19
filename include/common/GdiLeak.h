#ifndef _GDI_LEAK_H_
#define _GDI_LEAK_H_

/*
BeginPaint
CreateBrushIndirect
CreateCompatibleDC
CreateDIBPatternBrush
CreateDIBPatternBrushPt
CreateFont
CreateFontIndirect
CreateHatchBrush
CreatePatternBrush
CreatePen
CreatePenIndirect
CreateSolidBrush
DeleteDC
DeleteObject
EndPaint
ExtCreatePen
GetDC
GetWindowDC
ReleaseDC
SelectObject
CreateDIBSection
CreateRectRgn
CreatePalette
*/

#if 0 && defined(WIN32)
#define GDI_LEAK_DETECT	1

// Macros to redirect calls to our lib
#define BeginPaint(hwnd, lpPaint)					Lgi_BeginPaint(__FILE__, __LINE__, hwnd, lpPaint)
#define CreateBrushIndirect(lplb)					Lgi_CreateBrushIndirect(__FILE__, __LINE__, lplb)
#define CreateCompatibleDC(hdc)						Lgi_CreateCompatibleDC(__FILE__, __LINE__, hdc)
#define CreateDIBPatternBrush(dib, col)				Lgi_CreateDIBPatternBrush(__FILE__, __LINE__, dib, col)
#define CreateDIBPatternBrushPt(dib, use)			Lgi_CreateDIBPatternBrushPt(__FILE__, __LINE__, dib, use)
#define CreateFontA(a, b, c, d, e, f, g, h, j, i, k, l, m, n) Lgi_CreateFont(__FILE__, __LINE__, a, b, c, d, e, f, g, h, j, i, k, l, m, n)
#define CreateFontIndirectA(f)						Lgi_CreateFontIndirect(__FILE__, __LINE__, f)
#define CreateHatchBrush(a, b)						Lgi_CreateHatchBrush(__FILE__, __LINE__, a, b)
#define CreatePatternBrush(a)						Lgi_CreatePatternBrush(__FILE__, __LINE__, a)
#define CreatePen(a, b, c)							Lgi_CreatePen(__FILE__, __LINE__, a, b, c)
#define CreatePenIndirect(a)						Lgi_CreatePenIndirect(__FILE__, __LINE__, a)
#define CreateSolidBrush(a)							Lgi_CreateSolidBrush(__FILE__, __LINE__, a)
#define DeleteDC(a)									Lgi_DeleteDC(__FILE__, __LINE__, a)
#define DeleteObject(a)								Lgi_DeleteObject(__FILE__, __LINE__, a)
#define EndPaint(a, b)								Lgi_EndPaint(__FILE__, __LINE__, a, b)
#define ExtCreatePen(a, b, c, d, e)					Lgi_ExtCreatePen(__FILE__, __LINE__, a, b, c, d, e)
#define GetDC(a)									Lgi_GetDC(__FILE__, __LINE__, a)
#define GetWindowDC(a)								Lgi_GetWindowDC(__FILE__, __LINE__, a)
#define ReleaseDC(a, b)								Lgi_ReleaseDC(__FILE__, __LINE__, a, b)
#define SelectObject(a, b)							Lgi_SelectObject(__FILE__, __LINE__, a, b)
#define CreateDIBSection(a, b, c, e, f, g)			Lgi_CreateDIBSection(__FILE__, __LINE__, a, b, c, e, f, g)
#define CreateRectRgn(a, b, c, d)					Lgi_CreateRectRgn(__FILE__, __LINE__, a, b, c, d)
#define CreatePalette(a)							Lgi_CreatePalette(__FILE__, __LINE__, a)

// Our lib wrappers
LgiFunc HDC Lgi_BeginPaint(char *File, int Line, HWND hWnd, LPPAINTSTRUCT lpPaint);
LgiFunc HBRUSH Lgi_CreateBrushIndirect(char *File, int Line, CONST LOGBRUSH *LogBrush);
LgiFunc HDC Lgi_CreateCompatibleDC(char *File, int Line, HDC hdc);
LgiFunc HBRUSH Lgi_CreateDIBPatternBrush(char *File, int Line, HGLOBAL hglbDIBPacked, UINT fuColorSpec);
LgiFunc HBRUSH Lgi_CreateDIBPatternBrushPt(char *File, int Line, CONST VOID *lpPackedDIB, UINT iUsage);
LgiFunc HFONT Lgi_CreateFont
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
);
LgiFunc HFONT Lgi_CreateFontIndirect(char *File, int Line, CONST LOGFONT *lplf);
LgiFunc HBRUSH Lgi_CreateHatchBrush(char *File, int Line, int fnStyle, COLORREF clrref);
LgiFunc HBRUSH Lgi_CreatePatternBrush(char *File, int Line, HBITMAP hbmp);
LgiFunc HPEN Lgi_CreatePen(char *File, int Line, int fnPenStyle, int nWidth, COLORREF crColor);
LgiFunc HPEN Lgi_CreatePenIndirect(char *File, int Line, CONST LOGPEN *lplgpn);
LgiFunc HBRUSH Lgi_CreateSolidBrush(char *File, int Line, COLORREF crColor);
LgiFunc BOOL Lgi_DeleteDC(char *File, int Line, HDC hdc);
LgiFunc BOOL Lgi_DeleteObject(char *File, int Line, HGDIOBJ hObject);
LgiFunc BOOL Lgi_EndPaint(char *File, int Line, HWND hWnd, CONST PAINTSTRUCT *lpPaint);
LgiFunc HPEN Lgi_ExtCreatePen(char *File, int Line, DWORD dwPenStyle, DWORD dwWidth, CONST LOGBRUSH *lplb, DWORD dwStyleCount, CONST DWORD *lpStyle);
LgiFunc HDC Lgi_GetDC(char *File, int Line, HWND hWnd);
LgiFunc HDC Lgi_GetWindowDC(char *File, int Line, HWND hWnd);
LgiFunc int Lgi_ReleaseDC(char *File, int Line, HWND hWnd, HDC hDC);
LgiFunc HGDIOBJ Lgi_SelectObject(char *File, int Line, HDC hdc, HGDIOBJ hgdiobj);
LgiFunc HBITMAP Lgi_CreateDIBSection(char *File, int Line, HDC hdc, CONST BITMAPINFO *pbmi, UINT iUsage, VOID **ppvBits, HANDLE hSection, DWORD dwOffset);
LgiFunc HRGN Lgi_CreateRectRgn(char *File, int Line, int a, int b, int c, int d);
LgiFunc HPALETTE Lgi_CreatePalette(char *File, int Line, CONST LOGPALETTE *lplgpl);

#endif

#endif