/*hdr
**	FILE:		GTypeFace.cpp
**	AUTHOR:		Matthew Allen
**	DATE:		5/5/97
**	DESCRIPTION:	GDC v2.xx True Type Font Support
**
**	Copyright (C) 1997, Matthew Allen
**		fret@memecode.com
*/

/****************************** Includes ************************************************************************************/

#include <stdio.h>
#include <conio.h>
#include <stdlib.h>
#include <string.h>
#include <dos.h>
#include <math.h>

#include "Lgi.h"

/****************************** Defines *************************************************************************************/


/****************************** Globals *************************************************************************************/


/****************************** Locals **************************************************************************************/

GSurface *pTest = 0;
int CapA = 0;

/****************************** Inline Assembler Routines *******************************************************************/


/****************************** Helper Functions ****************************************************************************/

ulong
CalcTableChecksum(ulong *Table, ulong Length)
{
	ulong Sum = 0L;
	ulong *EndPtr = Table + ((Length + 3) & ~3) / sizeof(ulong);

	while (Table < EndPtr)
		Sum += *Table++;

	return Sum;
}

ushort *ReadUShortArray(GFile &F, int Size)
{
	ushort *Data = new ushort[Size];
	if (Data)
	{
		for (int i=0; i<Size; i++)
		{
			F >> Data[i];
		}
	}
	return Data;
}

short *ReadShortArray(GFile &F, int Size)
{
	short *Data = new short[Size];
	if (Data)
	{
		for (int i=0; i<Size; i++)
		{
			F >> Data[i];
		}
	}
	return Data;
}

int RoundNearest(double n)
{
	double i;
	double f = modf(n, &i);
	if (f >= 0.5)
	{
		return (int)i + 1;
	}
	return (int) i;
}

/****************************** Classes *************************************************************************************/

GFont::GFont()
{
	hFont = 0;
	Widths = 0;
}

GFont::~GFont()
{
	DeleteArray(Widths);
	if (hFont)
	{
		DeleteObject(hFont);
	}
}

void GFont::SetWidths()
{
	DeleteArray(Widths);
	if (hFont)
	{
		HDC hDC = CreateCompatibleDC(0);
		HFONT hOld = (HFONT) SelectObject(hDC, hFont);

		Widths = new float[256];
		if (Widths)
		{
			for (int i=0; i<256; i++)
			{
				uchar c = i;
				SIZE Size;
				if (GetTextExtentPoint32(hDC, (char*)&c, 1, &Size))
				{
					Widths[c] = Size.cx;
				}
				else
				{
					Widths[c] = 0;
				}
			}
		}

		SelectObject(hDC, hOld);
		DeleteDC(hDC);
	}
}

bool GFont::Create(	char *Face,
					int Height,
					int Width,
					int Weight,
					uint32 Italic,
					uint32 Underline,
					uint32 StrikeOut,
					uint32 CharSet,
					int Escapement,
					int Orientation,
					uint32 OutputPrecision,
					uint32 ClipPrecision,
					uint32 Quality,
					uint32 PitchAndFamily)
{
	if (hFont)
	{
		DeleteObject(hFont);
		hFont = 0;
	}

	if (Height > 0)
	{
		HDC hDC = GetDC(GetDesktopWindow());
		Height = -MulDiv(Height, GetDeviceCaps(hDC, LOGPIXELSY), 72);
		ReleaseDC(GetDesktopWindow(), hDC);
	}

	hFont = ::CreateFont(	Height,
							Width,
							Escapement,
							Orientation,
							Weight,
							Italic,
							Underline,
							StrikeOut,
							CharSet,
							OutputPrecision,
							ClipPrecision,
							Quality,
							PitchAndFamily,
							Face);

	SetWidths();

	return (hFont != 0);
}

bool GFont::CreateFont(LOGFONT *LogFont)
{
	if (hFont)
	{
		DeleteObject(hFont);
		hFont = 0;
	}

	if (LogFont)
	{
		hFont = CreateFontIndirect(LogFont);
		SetWidths();
	}

	return (hFont != 0);
}

void GFont::Text(GSurface *pDC, int x, int y, char *Str, int Len, GRect *r)
{
	if (pDC AND hFont AND Str)
	{
		HDC hDC = pDC->StartDC();
		HFONT hOldFont = (HFONT) SelectObject(hDC, hFont);

		SetTextColor(hDC, ForeCol&0xFFFFFF);
		SetBkColor(hDC, BackCol&0xFFFFFF);
		SetBkMode(hDC, (Trans) ? TRANSPARENT : OPAQUE);

		if (Len < 0)
		{
			Len = strlen(Str);
		}
		if (Len > 0)
		{
			if (TabSize)
			{
				TabbedTextOut(hDC, x, y, Str, Len, 1, &TabSize, 0);
			}
			else
			{
				RECT rc = {0, 0, 0, 0};
				if (r) rc = *r;

				ExtTextOut(	hDC,
							x,
							y,
							((Trans) ? 0 : ETO_OPAQUE) |
							((r) ? ETO_CLIPPED : 0),
							(r) ? &rc : 0,
							Str,
							Len,
							NULL);
			}
		}
	
		SelectObject(hDC, hOldFont);
		pDC->EndDC();
	}
}

void GFont::TextW(GSurface *pDC, int x, int y, ushort *Str, int Len, GRect *r)
{
	if (pDC AND hFont AND Str)
	{
		HDC hDC = pDC->StartDC();
		HFONT hOldFont = (HFONT) SelectObject(hDC, hFont);

		SetTextColor(hDC, ForeCol&0xFFFFFF);
		SetBkColor(hDC, BackCol&0xFFFFFF);
		SetBkMode(hDC, (Trans) ? TRANSPARENT : OPAQUE);

		if (Len < 0)
		{
			for (Len=0; Str[Len]!=0; Len++);
		}
		if (Len > 0)
		{
			if (TabSize)
			{
				if (NOT TabbedTextOutW(hDC,
										x, y,
										Str,
										Len,
										1,
										&TabSize,
										0))
				{
					goto FailOver;
				}
			}
			else
			{
				FailOver:
				ExtTextOutW(hDC,
							x, y,
							((Trans) ? 0 : ETO_OPAQUE) |
							((r) ? ETO_CLIPPED : 0),
							(r) ? &((RECT)*r) : 0,
							Str,
							Len,
							NULL);
			}
		}
	
		SelectObject(hDC, hOldFont);
		pDC->EndDC();
	}
}

void GFont::Size(int *x, int *y, char *Str, int Len, int Flags)
{
	HDC hDC = CreateCompatibleDC(NULL);
	HFONT hFnt = (HFONT) SelectObject(hDC, hFont);
	SIZE Size;
	int Sx = 0;
	
	if (Str)
	{
		if (Len < 0) Len = strlen(Str);

		if (TabSize)
		{
			for (int i=0; i<Len; i++)
			{
				if (Str[i] == 9)
				{
					Sx = ((Sx + TabSize) / TabSize) * TabSize;
				}
				else
				{
					GetTextExtentPoint32(hDC, Str+i, 1, &Size);
					Sx += Size.cx;
				}
			}
		}
		else
		{
			GetTextExtentPoint32(hDC, Str, Len, &Size);
			Sx = Size.cx;
		}
	}

	if (x) *x = Sx;
	if (y) *y = Size.cy;

	SelectObject(hDC, hFnt);
	DeleteDC(hDC);
}

char *strnchr(char *s, char c, int Len)
{
	if (s)
	{
		for (int i=0; i<Len; i++)
		{
			if (s[i] == c)
			{
				return s + i;
			}
		}
	}

	return 0;
}

int GFont::CharAt(int x, char *Str, int Len)
{
	if (x < 0)
	{
		return 0;
	}

	if (Widths)
	{
		float Cx = 0.0;
		int Chars = -1;
		for (int i=0; i<Len; i++)
		{
			float Nx;
			if (Str[i] == 9)
			{
				Nx = (Cx - ((int)Cx % TabSize)) + TabSize;
			}
			else
			{
				Nx = Cx + Widths[(uchar)Str[i]];
			}

			if (x >= Cx AND x < Nx)
			{
				Chars = i;
				break;
			}
			Cx = Nx;
		}

		return (Chars < 0) ? Len : Chars;
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void *TtfObj::FindTag(char *t)
{
	TtfTable *tbl = (Parent) ? Parent->FindTag(t) : 0;
	return (tbl) ? tbl->Table : 0;
}

bool TtfFileHeader::Read(GFile &F)
{
	F >> Version;
	F >> NumTables;
	F >> SearchRange;
	F >> EntrySelector;
	F >> RangeShift;
	return NOT F.GetStatus();
}

bool TtfFileHeader::Write(GFile &F)
{
	F << Version;
	F << NumTables;
	F << SearchRange;
	F << EntrySelector;
	F << RangeShift;
	return NOT F.GetStatus();
}

void TtfFileHeader::Dump()
{
	printf("Version: %i.%i\n", Version >> 16, Version & 0xFFFF);
	printf("NumTables: %i\n", NumTables);
	printf("SearchRange: %i\n", SearchRange);
	printf("EntrySelector: %i\n", EntrySelector);
	printf("RangeShift: %i\n", RangeShift);
	printf("\n");
}

bool TtfTable::Read(GFile &F)
{
	F.Read(Tag, sizeof(Tag));
	F >> CheckSum;
	F >> Offset;
	F >> Length;
	return NOT F.GetStatus();
}

bool TtfTable::Write(GFile &F)
{
	F.Write(Tag, sizeof(Tag));
	F << CheckSum;
	F << Offset;
	F << Length;
	return NOT F.GetStatus();
}

void TtfTable::Dump()
{
	printf("Tag: %4.4s\n", Tag);
	printf("CheckSum: 0x%08.8X\n", CheckSum);
	printf("Offset: %i\n", Offset);
	printf("Length: %i\n", Length);
	printf("\n");
}

bool TtfHeader::Read(GFile &F)
{
	int32 h, l;

	F >> Version;
	F >> Revision;
	F >> CheckSumAdjustment;
	F >> MagicNumber;
	F >> Flags;
	F >> UnitsPerEm;

	F >> h;
	F >> l;
	InternationalDate = ((((quad)h)<<32) | l);

	F >> h;
	F >> l;
	Modified = ((((quad)h)<<32) | l);

	F >> xMin;
	F >> yMin;
	F >> xMax;
	F >> yMax;
	F >> MacStyle;
	F >> LowestRecPPEM;
	F >> FontDirectionHint;
	F >> IndexToLocFormat;
	F >> GlyphDataFormat;

	return NOT F.GetStatus();
}

bool TtfHeader::Write(GFile &F)
{
	F << Version;
	F << Revision;
	F << CheckSumAdjustment;
	F << MagicNumber;
	F << Flags;
	F << UnitsPerEm;
	F << ((int32) (InternationalDate >> 32));
	F << ((int32) InternationalDate);
	F << ((int32) (Modified >> 32));
	F << ((int32) Modified);
	F << xMin;
	F << yMin;
	F << xMax;
	F << yMax;
	F << MacStyle;
	F << LowestRecPPEM;
	F << FontDirectionHint;
	F << IndexToLocFormat;
	F << GlyphDataFormat;

	return NOT F.GetStatus();
}

void TtfHeader::Dump()
{
	printf("Version: %i.%i\n", Version>>16, Version&0xFFFF);
	printf("Manufacture Revision: %i.%i\n", Revision>>16, Revision&0xFFFF);
	printf("CheckSumAdjustment: 0x%08.8X\n", CheckSumAdjustment);
	printf("MagicNumber: 0x%08.8X (%s)\n", MagicNumber, (MagicNumber == 0x5F0F3CF5) ? "Valid" : "Invalid");

	printf("Flags:\n");
	printf("   %i - baseline for font at y=0\n", (Flags & 0x0001) != 0);
	printf("   %i - left sidebearing at x=0\n", (Flags & 0x0002) != 0);
	printf("   %i - instructions may depend on point size\n", (Flags & 0x0004) != 0);
	printf("   %i - force ppem to integer values for all internal scaler math\n", (Flags & 0x0008) != 0);
	printf("   %i - instructions may alter advance width\n", (Flags & 0x0010) != 0);

	printf("UnitsPerEm: %i\n", UnitsPerEm);
	// Quad	InternationalDate;
	// Quad	Modified;
	printf("xMin: %i\n", xMin);
	printf("yMin: %i\n", yMin);
	printf("xMax: %i\n", xMax);
	printf("yMax: %i\n", yMax);
	printf("MacStyle:\n");
	if (MacStyle & 0x01)
		printf("   BOLD\n");
	if (MacStyle & 0x02)
		printf("   ITALIC\n");
	if (NOT (MacStyle & 0x03))
		printf("   none\n");
	printf("LowestRecPPEM: %i\n", LowestRecPPEM);

	char *DirText[5] = {
		"(-2) Like -1 but also contains neutrals",
		"(-1) Only strongly right to left",
		"(0) Fully mixed directional glyphs",
		"(1) Only strongly left to right",
		"(2) Like 1 but also contains neutrals"};

	if (abs(FontDirectionHint) <= 2)
	{
		printf("FontDirectionHint:\n   %s\n", DirText[FontDirectionHint+2]);
	}
	else
	{
		printf("FontDirectionHint:\n   error\n");
	}

	printf("IndexToLocFormat: %s\n", (IndexToLocFormat) ? "LONG" : "SHORT");
	printf("GlyphDataFormat: %i\n", GlyphDataFormat);
	printf("\n");
}

bool TtfMaxProfile::Read(GFile &F)
{
	F >> Version;
	F >> NumGlyphs;
	F >> MaxPoints;
	F >> MaxContours;
	F >> MaxCompositePoints;
	F >> MaxCompositeContours;
	F >> MaxZones;
	F >> MaxTwilightPoints;
	F >> MaxStorage;
	F >> MaxFunctionDefs;
	F >> MaxInstructionDefs;
	F >> MaxStackElements;
	F >> MaxSizeOfInstructions;
	F >> MaxComponentElements;
	F >> MaxComponentDepth;

	return NOT F.GetStatus();
}

bool TtfMaxProfile::Write(GFile &F)
{
	F << Version;
	F << NumGlyphs;
	F << MaxPoints;
	F << MaxContours;
	F << MaxCompositePoints;
	F << MaxCompositeContours;
	F << MaxZones;
	F << MaxTwilightPoints;
	F << MaxStorage;
	F << MaxFunctionDefs;
	F << MaxInstructionDefs;
	F << MaxStackElements;
	F << MaxSizeOfInstructions;
	F << MaxComponentElements;
	F << MaxComponentDepth;

	return NOT F.GetStatus();
}

void TtfMaxProfile::Dump()
{
	printf("Version: %i.%i\n", Version >> 16, Version & 0xFFFF);
	printf("NumGlyphs: %i\n", NumGlyphs);
	printf("MaxPoints: %i\n", MaxPoints);
	printf("MaxContours: %i\n", MaxContours);
	printf("MaxCompositePoints: %i\n", MaxCompositePoints);
	printf("MaxCompositeContours: %i\n", MaxCompositeContours);
	printf("MaxZones: %i\n", MaxZones);
	printf("MaxTwilightPoints: %i\n", MaxTwilightPoints);
	printf("MaxStorage: %i\n", MaxStorage);
	printf("MaxFunctionDefs: %i\n", MaxFunctionDefs);
	printf("MaxInstructionDefs: %i\n", MaxInstructionDefs);
	printf("MaxStackElements: %i\n", MaxStackElements);
	printf("MaxSizeOfInstructions: %i\n", MaxSizeOfInstructions);
	printf("MaxComponentElements: %i\n", MaxComponentElements);
	printf("MaxComponentDepth: %i\n", MaxComponentDepth);
	printf("\n");
}

TtfLocation::TtfLocation()
{
	Entries = 0;
	Type = 0;
	Data = 0;
}

TtfLocation::~TtfLocation()
{	
	if (Type)
	{
		if (Data)
		{
			delete (ulong*) Data;
		}
	}
	else
	{
		if (Data)
		{
			delete (ushort*) Data;
		}
	}
}

int TtfLocation::operator [](int i)
{
	if (i >= 0 AND i < Entries)
	{
		if (Type)
		{
			return ((ulong*) Data)[i];
		}
		else
		{
			return ((ushort*) Data)[i];
		}
	}

	return 0;
}

bool TtfLocation::Read(GFile &F)
{
	bool Status = FALSE;
	TtfHeader *Header = (TtfHeader*) FindTag("head");
	TtfMaxProfile *Profile = (TtfMaxProfile*) FindTag("maxp");

	if (Header AND Profile)
	{
		Type = Header->IndexToLocFormat;
		Entries = Profile->NumGlyphs;

		if (Type)
		{
			ulong *Ptr = new ulong[Entries];
			Data = Ptr;

			for (int i=0; i<Entries; i++)
			{
				F >> Ptr[i];
			}
		}
		else
		{
			ushort *Ptr = new ushort[Entries];
			Data = Ptr;

			for (int i=0; i<Entries; i++)
			{
				F >> Ptr[i];
				Ptr[i] *= 2;
			}
		}

		Status = NOT F.GetStatus();
	}
	
	return Status;
}

bool TtfLocation::Write(GFile &F)
{
	bool Status = FALSE;
	TtfHeader *Header = (TtfHeader*) FindTag("head");
	TtfMaxProfile *Profile = (TtfMaxProfile*) FindTag("maxp");
	if (Header AND Profile AND Data)
	{
		if (Type)
		{
			ulong *Ptr = (ulong*) Data;
			for (int i=0; i<Entries; i++)
			{
				F << Ptr[i];
			}
		}
		else
		{
			ushort *Ptr = (ushort*) Data;
			for (int i=0; i<Entries; i++)
			{
				ushort k = Ptr[i] / 2;
				F << k;
			}
		}

		Status = NOT F.GetStatus();
	}
	
	return Status;
}

void TtfLocation::Dump()
{
	printf("Entries: %i\n", Entries);
	printf("Type: %s\n", (Type) ? "LONG" : "SHORT");
	for (int i=0; i<Entries; i++)
	{
		printf("[%i]: %i\n", i, (*this)[i]);
	}
	printf("\n");
}

TtfGlyph::TtfGlyph()
{
	Contours = 0;
	EndPtsOfContours = 0;
	Instructions = 0;
	Flags = 0;
	X = 0;
	Y = 0;

	Temp = 0;
}

TtfGlyph::~TtfGlyph()
{
	DeleteArray(EndPtsOfContours);
	DeleteArray(Instructions);
	DeleteArray(Flags);
	DeleteArray(X);
	DeleteArray(Y);

	DeleteArray(Temp);
}

#define OnCurve				0x0001
#define XShort				0x0002
#define YShort				0x0004
#define Repeat				0x0008
#define XSame				0x0010
#define YSame				0x0020

bool TtfGlyph::Read(GFile &F)
{
	Points = 0;

	F >> Contours;
	F >> xMin;
	F >> yMin;
	F >> xMax;
	F >> yMax;

	if (Contours >= 0)
	{
		EndPtsOfContours = new ushort[Contours];
		if (EndPtsOfContours)
		{
			for (int i=0; i<Contours; i++)
			{
				F >> EndPtsOfContours[i];
			}

			Points = EndPtsOfContours[Contours-1] + 1;
		}

		F >> InstructionLength;
		Instructions = new uchar[InstructionLength];
		if (Instructions)
		{
			F.Read(Instructions, InstructionLength);
		}

 		Flags = new uchar[Points];
		if (Flags)
		{
			for (int i=0; i<Points; i++)
			{
				uchar n;
				F >> n;
				if (n & Repeat)
				{
					uchar Times;

					F >> Times;
					
					memset(Flags + i, n, Times + 1);
					i += Times;
				}
				else
				{
					Flags[i] = n;
				}
			}
		}

		X = new int[Points];
		if (X)
		{
			int CurX = 0;

			for (int i=0; i<Points; i++)
			{
				if (Flags[i] & XShort)
				{
					uchar n;
					F >> n;

					if (Flags[i] & XSame)
					{
						CurX += n;
					}
					else
					{
						CurX -= n;
					}
				}
				else if (NOT (Flags[i] & XSame))
				{
					short n;
					F >> n;
					CurX += n;
				}

				X[i] = CurX;
			}
		}

		Y = new int[Points];
		if (Y)
		{
			int CurY = 0;

			for (int i=0; i<Points; i++)
			{
				if (Flags[i] & YShort)
				{
					uchar n;
					F >> n;
					if (Flags[i] & YSame)
					{
						CurY += n;
					}
					else
					{
						CurY -= n;
					}
				}
				else if (NOT (Flags[i] & YSame))
				{
					short n;
					F >> n;
					CurY += n;
				}

				Y[i] = CurY;
			}
		}
	}

	return NOT F.GetStatus();
}

bool TtfGlyph::Write(GFile &F)
{
	return FALSE;
}

void TtfGlyph::Dump()
{
	printf("Contours: %i\n", Contours);
	printf("Points: %i\n", Points);
	printf("xMin: %i\n", xMin);
	printf("yMin: %i\n", yMin);
	printf("xMax: %i\n", xMax);
	printf("yMax: %i\n", yMax);

	if (Contours >= 0)
	{
		printf("InstructionLength: %i\n", InstructionLength);
		if (EndPtsOfContours)
		{
			for (int i=0; i<Contours; i++)
			{
				printf("EndPts[%i]: %i\n", i, EndPtsOfContours[i]);
			}
		}
		
		for (int i=0; i<Points; i++)
		{
			char Bits[9];
			for (int n=1, k=7; n&0xFF; n<<=1, k--)
			{
				Bits[k] = (Flags[i] & n) ? '1' : '0';
			}
			Bits[8] = 0;
			
			printf("Point[%i] flags: %s  x: %i  y: %i\n", i, Bits, X[i], Y[i]);
		}
	}

	if (Temp)
	{
		uchar *p = Temp;
		for (int i=0; i<4; i++)
		{
			for (int n=0; n<16; n++)
			{
				printf("%02.2X ", *p++);
			}
			printf("\n");
		}
	}

	printf("\n");
}

void TtfGlyph::Draw(GSurface *pDC, int x, int y, int MaxSize)
{
	int XSize = xMax - xMin;
	int YSize = yMax - yMin;
	double Scale;

	if (XSize < YSize)
	{
		Scale = (double) MaxSize / YSize;
	}
	else
	{
		Scale = (double) MaxSize / XSize;
	}
}

int TtfGlyph::DrawEm(GSurface *pDC, int x, int y, int EmUnits, double PixelsPerEm)
{
	int XSize = xMax - xMin + 1;
	int YSize = yMax - yMin + 1;
	double Scale = PixelsPerEm / (double) EmUnits;

#define SCALEX(xs)		x + (Scale * (xs))
#define SCALEY(ys)		y - (Scale * (ys))

	if (Contours > 0)
	{
		{
			COLOUR c = pDC->Colour(0);

			pDC->Colour(GdcD->GetColour(0xFF0000));
			pDC->Box(	SCALEX(xMin),
					SCALEY(yMin),
					SCALEX(xMax),
					SCALEY(yMax));

			pDC->Colour(GdcD->GetColour(0xFF00));
			pDC->Line(	SCALEX(xMin),
					SCALEY(0),
					SCALEX(xMax),
					SCALEY(0));

			/*
			pDC->Line(	SCALEX(0),
					SCALEY(yMin),
					SCALEX(0),
					SCALEY(yMax));
			*/

			pDC->Colour(c);
		}

		// #define DRAW_POINTS
		#ifdef DRAW_POINTS
		pDC->Colour(GdcD->GetColour(0x808080));
		#endif

		COLOUR c = pDC->Colour(GdcD->GetColour(0xFF));

		int pt = 0;
		for (int n=0; n<Contours; n++)
		{
			int Start = pt;
	
			for (; pt<=EndPtsOfContours[n]; pt++)
			{
				int Next = (pt >= EndPtsOfContours[n]) ? Start : pt+1;
				int Prev = (pt > 0) ? pt-1 : EndPtsOfContours[n];
				int Plus2 = (pt >= EndPtsOfContours[n]-1) ? pt + 1 - EndPtsOfContours[n] + Start : pt + 2;
	
				if (Flags[pt] & OnCurve)
				{
					if (Flags[Next] & OnCurve)
					{
						pDC->Line(	SCALEX(X[pt]),
								SCALEY(Y[pt]),
								SCALEX(X[Next]),
								SCALEY(Y[Next]));
					}
				}
				else
				{
					// #define LINE
					#ifdef LINE
					pDC->Line(	SCALEX(X[pt]),
							SCALEY(Y[pt]),
							SCALEX(X[Next]),
							SCALEY(Y[Next]));
	
					pDC->Line(	SCALEX(X[pt]),
							SCALEY(Y[pt]),
							SCALEX(X[Prev]),
							SCALEY(Y[Prev]));
					#else
					GdcPt2 p[3];
	
					if (Flags[Prev] & OnCurve)
					{
						p[0].x = SCALEX(X[Prev]);
						p[0].y = SCALEY(Y[Prev]);
					}
					else
					{
						p[0].x = SCALEX((X[Prev]+X[pt])/2);
						p[0].y = SCALEY((Y[Prev]+Y[pt])/2);
					}
	
					p[1].x = SCALEX(X[pt]);
					p[1].y = SCALEY(Y[pt]);
					
					if (Flags[Next] & OnCurve)
					{
						p[2].x = SCALEX(X[Next]);
						p[2].y = SCALEY(Y[Next]);
					}
					else
					{
						p[2].x = SCALEX((X[Next]+X[pt])/2);
						p[2].y = SCALEY((Y[Next]+Y[pt])/2);
					}
	
					pDC->Bezier(5, p);
					#endif
				}
			}
		}

		pDC->Colour(c);

		#ifdef DRAW_POINTS
		pDC->Colour(GdcD->GetColour(0xFFFFFF));
		for (int p=0; p<Points; p++)
		{
			int sx = SCALEX(X[p]);
			int sy = SCALEY(Y[p]);

			if (Flags[p] & OnCurve)
			{
				pDC->Box(sx-2, sy-2, sx+2, sy+2);
			}
			else
			{
				pDC->Line(sx-2, sy-2, sx+2, sy+2);
				pDC->Line(sx+2, sy-2, sx-2, sy+2);
			}
		}
		#endif
	}

	return ceil(Scale * XSize);
}

TtfContour::TtfContour()
{
	Points = 0;
	Alloc = 0;
	Point = 0;
}

TtfContour::~TtfContour()
{
	DeleteArray(Point);
}

bool TtfContour::SetPoints(int p)
{
	if (p > Alloc)
	{
		int NewAlloc = (p + 31) & (~0x1F);
		CPt *New = new CPt[NewAlloc];
		if (New)
		{
			MemCpy(New, Point, sizeof(CPt)*Points);
			DeleteArray(Point);
			Point = New;
			Points = p;
			Alloc = NewAlloc;
		}
		else
		{
			return FALSE;
		}
	}
	else
	{
		Points = p;
	}
	return TRUE;
}

#define sqr(a)			((a)*(a))
#define Distance(a, b)		(sqrt(sqr(a.x-b.x)+sqr(a.y-b.y)))

void TtfContour::Bezier(CPt *p, double Threshold)
{
	int Start = Points;
	
	if (Points > 0 AND p)
	{
		int OldPts = 3;
		int NewPts = 0;
		CPt *BufA = new CPt[1024];
		CPt *BufB = new CPt[1024];
		CPt *Old = BufA;
		CPt *New = BufB;

		if (NOT Old OR NOT New) return;
		for (int n=0; n<OldPts; n++)
		{
			Old[n].x = p[n].x;
			Old[n].y = p[n].y;
		}

		do
		{
			NewPts = 1;
			New[0] = Old[0];
			for (int i=1; i<OldPts-1; i += 2)
			{
				New[NewPts].x = (Old[i].x + Old[i-1].x) / 2;
				New[NewPts].y = (Old[i].y + Old[i-1].y) / 2;
				NewPts += 2;

				New[NewPts].x = (Old[i].x + Old[i+1].x) / 2;
				New[NewPts].y = (Old[i].y + Old[i+1].y) / 2;
				NewPts--;

				New[NewPts].x = (New[NewPts+1].x + New[NewPts-1].x) / 2;
				New[NewPts].y = (New[NewPts+1].y + New[NewPts-1].y) / 2;
				NewPts += 2;

				New[NewPts++] = Old[i+1];
			}

			CPt *Temp = Old;
			Old = New;
			New = Temp;
			
			OldPts = NewPts;
		}
		while (Distance(Old[0], Old[1]) > Threshold);

		if (SetPoints(Points + OldPts - 1))
		{
			MemCpy(Point + Start, Old + 1, sizeof(CPt)*(OldPts-1));
		}

		DeleteArray(BufA);
		DeleteArray(BufB);
	}
}

void TtfContour::Setup(int *aX, int *aY, uchar *aFlags, int xo, int yo)
{
	x = aX;
	y = aY;
	Flags = aFlags;
	XOff = xo;
	YOff = yo;
}

bool TtfContour::Create(int Pts, double XScale, double YScale)
{
	#define ScaleX(xs)		(XScale * ((xs) - XOff))
	#define ScaleY(ys)		(YScale * (YOff - (ys)))

	bool Status = FALSE;

	if (x AND y AND Flags)
	{
		if (	(Flags[0] & OnCurve) AND
			SetPoints(1))
		{
			Point[0].x = ScaleX(x[0]);
			Point[0].y = ScaleY(y[0]);
		}

		for (int i=0; i<Pts; i++)
		{
			int Prev = (i > 0) ? i-1 : Pts-1;
			int Next = (i >= Pts-1) ? 0 : i+1;
			int Plus2 = (i+2 >= Pts) ? i+2-Pts : i+2;

			if (Flags[i] & OnCurve)
			{
				if (	(Flags[Next] & OnCurve) AND
					SetPoints(Points+1))
				{
					Point[Points-1].x = ScaleX(x[Next]);
					Point[Points-1].y = ScaleY(y[Next]);
				}
			}
			else
			{
				CPt Temp[3];

				if (Flags[Prev] & OnCurve)
				{
					Temp[0].x = ScaleX(x[Prev]);
					Temp[0].y = ScaleY(y[Prev]);
				}
				else
				{
					Temp[0].x = ScaleX((x[Prev]+x[i])/2);
					Temp[0].y = ScaleY((y[Prev]+y[i])/2);
				}

				Temp[1].x = ScaleX(x[i]);
				Temp[1].y = ScaleY(y[i]);
				
				if (Flags[Next] & OnCurve)
				{
					Temp[2].x = ScaleX(x[Next]);
					Temp[2].y = ScaleY(y[Next]);
				}
				else
				{
					Temp[2].x = ScaleX((x[Next]+x[i])/2);
					Temp[2].y = ScaleY((y[Next]+y[i])/2);
				}

				Bezier(Temp);
			}
		}

		Status = TRUE;
	}

	return Status;
}

void TtfContour::DebugDraw(GSurface *pDC, int Sx, int Sy)
{
	if (pDC)
	{
		int Last = pDC->Y()-1;

		pDC->Colour(GdcD->GetColour(0xFFFFFF));
		for (int i=0; i<Points; i++)
		{
			if (i < Points - 1)
			{
				pDC->Line(	Point[i].x * Sx,
						(double) Last - (Point[i].y * Sy),
						Point[i+1].x * Sx,
						(double) Last - (Point[i+1].y * Sy));
			}
			else
			{
				pDC->Line(	Point[i].x * Sx,
						(double) Last - (Point[i].y * Sy),
						Point[0].x * Sx,
						(double) Last - (Point[0].y * Sy));
			}
		}
	}
}

double Up(double i)
{
	double f, n;
	
	f = modf(i, &n);

	if (f > 0.5)
	{
		return n + 1.5;
	}
	
	return n + 0.5;
}

bool TtfContour::RasterX(int Size, DataList *XList)
{
	bool Status = FALSE;
	
	if (XList)
	{
		for (int i=0; i<Points; i++)
		{
			CPt *This = Point+i;
			CPt *Next = (i < Points - 1) ? Point+i+1 : Point;
			
			if (This->y > Next->y)
			{
				CPt *Temp = This;
				This = Next;
				Next = Temp;
			}
			
			if (This->x == Next->x)
			{
				for (double n=Up(This->y); n<Next->y; n+=1)
				{
					int k = (int)floor(n);
					if (k >= 0 AND k < Size)
					{
						XList[k].AddValue(This->x);
					}
					else
					{
						return FALSE;
					}
				}
			}
			else if (This->y != Next->y)
			{
				double m = (Next->y-This->y) / (Next->x-This->x);
				double b = This->y - (m*This->x);

				for (double n=Up(This->y); n<Next->y; n+=1)
				{
					int k = (int)floor(n);
					if (k >= 0 AND k < Size)
					{
						XList[k].AddValue((n-b)/m);
					}
					else
					{
						return FALSE;
					}
				}
			}
		}
		
		Status = TRUE;
	}

	return Status;
}

bool TtfContour::RasterY(int Size, DataList *YList)
{
	bool Status = FALSE;
	
	if (YList)
	{
		for (int i=0; i<Points; i++)
		{
			CPt *This = Point+i;
			CPt *Next = (i < Points - 1) ? Point+i+1 : Point;
			
			if (This->x > Next->x)
			{
				CPt *Temp = This;
				This = Next;
				Next = Temp;
			}
			
			if (This->y == Next->y)
			{
				for (double n=Up(This->x); n<Next->x; n+=1)
				{
					int k = (int)floor(n);
					if (k >= 0 AND k < Size)
					{
						YList[k].AddValue(This->y);
					}
					else
					{
						return FALSE;
					}
				}
			}
			else if (This->x != Next->x)
			{
				double m = (Next->y-This->y) / (Next->x-This->x);
				double b = This->y - (m*This->x);

				for (double n=Up(This->x); n<Next->x; n+=1)
				{
					int k = (int)floor(n);
					if (k >= 0 AND k < Size)
					{
						YList[k].AddValue(m*n+b);
					}
					else
					{
						return FALSE;
					}
				}
			}
		}
		
		Status = TRUE;
	}

	return Status;
}

/*
	Rasterization rules from the TTF specification:

Rule 1
	If a pixel's center falls within the glyph outline, that pixel is turned 
	on and becomes part of that glyph. 

Rule 2
	If a contour falls exactly on a pixel's center, that pixel is turned on.

Rule 3
	If a scan line between two adjacent pixel centers (either vertical or horizontal)
	is intersected by both an on-Transition contour and an off-Transition contour 
	and neither of the pixels was already turned on by rules 1 and 2, turn on the 
	left-most pixel (horizontal scan line) or the bottom-most pixel (vertical 
	scan line) 

Rule 4
	Apply Rule 3 only if the two contours continue to intersect other scan lines 
	in both directions. That is do not turn on pixels for 'stubs'. The scanline 
	segments that form a square with the intersected scan line segment are 
	examined to verify that they are intersected by two contours.  It is possible 
	that these could be different contours than the ones intersecting the 
	dropout scan line segment. This is very unlikely but may have to be 
	controlled with grid-fitting in some exotic glyphs.
*/

#define GRID_X			5
#define GRID_Y			5

bool TtfGlyph::Rasterize(GSurface *pDC, GRect *pDest, double xppem, double yppem, int BaseLine)
{
	bool Status = FALSE;
	TtfHeader *Header = (TtfHeader*) FindTag("head");

	if (Contours <= 0) return TRUE;

	if (pDC AND pDest AND EndPtsOfContours)
	{
		TtfContour *pCon = new TtfContour[Contours];
		if (pCon)
		{
			int StartPoint = 0;

			pDC->Colour(0xFF);

			DataList *XList = new DataList[pDest->Y() + 1];
			DataList *YList = new DataList[pDest->X() + 1];

			if (XList)
			{
				Status = TRUE;
				for (int i=0; i<Contours; i++)
				{
					pCon[i].Setup(		X + StartPoint,
								Y + StartPoint,
								Flags + StartPoint,
								xMin,
								BaseLine * Header->UnitsPerEm / yppem);

					Status &= pCon[i].Create(EndPtsOfContours[i]-StartPoint+1,
								xppem / Header->UnitsPerEm,
								yppem / Header->UnitsPerEm);

					Status &= pCon[i].RasterX(pDest->Y() + 1, XList);
					Status &= pCon[i].RasterY(pDest->X() + 1, YList);

					StartPoint = EndPtsOfContours[i] + 1;
				}

				if (Status)
				{
					// Walk X crossing list for Rules 1 & 2
					for (int y=0; y<pDest->Y(); y++)
					{
						DataList *L = XList + y;
						for (int i=0; i<XList[y].Size-1; i += 2)
						{
							int Len = 0;
							double Start = Up(L->Data[i]);

							for (double x = Start; x < L->Data[i+1]; x += 1.0)
							{
								pDC->Set(
									pDest->x1 + x,
									pDest->y1 + y);
								Len++;
							}

							if (NOT Len)
							{
								int x = (int) L->Data[i];
								if (	NOT pDC->Get(	pDest->x1 + x,
											pDest->y1 + y) AND
									NOT pDC->Get(	pDest->x1 + x + 1,
											pDest->y1 + y))
								{
									pDC->Set(	pDest->x1 + x,
											pDest->y1 + y);
								}
							}
						}
					}

					// Walk Y crossing list for Rule 3
					for (int x=0; x<pDest->X(); x++)
					{
						DataList *L = YList + x;
						for (int i=0; i<YList[x].Size-1; i += 2)
						{
							int BaseY = (int) L->Data[i];
							if (	(Up(L->Data[i]) > L->Data[i+1]) AND
								NOT pDC->Get(pDest->x1 + x, pDest->y1 + BaseY) AND
								NOT pDC->Get(pDest->x1 + x, pDest->y1 + (BaseY + 1)))
							{
								pDC->Set(	pDest->x1 + x,
										pDest->y1 + BaseY);
							}
						}
					}
				}

				/*
				if (pTest)
				{
					pTest->Colour(GdcD->GetColour(0));
					pTest->Rectangle(0, 0, pTest->X(), pTest->Y());
		
					for (int y=0; y<pDest->Y(); y++)
					{
						for (int x=0; x<pDest->X(); x++)
						{
							pTest->Colour((pDC->Get(pDest->x1 + x, Top - y)) ? 0xBBBBBB : 0);
							pTest->Rectangle(
								x * GRID_X,
								pTest->Y() - (y * GRID_Y) - 1,
								(x+1) * GRID_X,
								pTest->Y() - ((y+1) * GRID_Y) - 1);
						}
					}

					pTest->Colour(GdcD->GetColour(0x404040));
					for (int x = 0; x<pTest->X(); x+=GRID_X)
					{
						pTest->VLine(x, 0, pTest->Y());
					}
					for (y = pTest->Y()-1; y>=0; y-=GRID_Y)
					{
						pTest->HLine(0, pTest->X(), y);
					}

					for (int i=0; i<Contours; i++)
					{
						pCon[i].DebugDraw(pTest, GRID_X, GRID_Y);
					}

					static int cx = 0, cy = 0;

					pTest->Blt(cx, cy, pDC, pDest);
					
					cx += pDest->X() + 1;
					if (cx >= pTest->X() - 20)
					{
						cy += 40;
						cx = 0;
					}
				}
				*/
			}

			DeleteArray(pCon);
		}
	}

	return Status;
}

TtfCMapTable::TtfCMapTable()
{
	Map = 0;
}

TtfCMapTable::~TtfCMapTable()
{
	DeleteObj(Map);
}

bool TtfCMapTable::Read(GFile &F)
{
	F >> PlatformID;
	F >> EncodingID;
	F >> Offset;
	return NOT F.GetStatus();
}

bool TtfCMapTable::Write(GFile &F)
{
	F << PlatformID;
	F << EncodingID;
	F << Offset;
	return NOT F.GetStatus();
}

void TtfCMapTable::Dump()
{
	char *EncodeStr[] = {
		"Symbol",
		"Unicode",
		"ShiftJIS",
		"Big5",
		"PRC",
		"Wansung",
		"Johab"};

	printf("  PlatformID: %i\n", PlatformID);
	printf("  EncodingID: %i - %s\n", EncodingID, (EncodingID >= 0 AND EncodingID < 6) ? EncodeStr[EncodingID] : "Error");
	printf("  Offset: %i\n", Offset);
	printf("  Format: %i\n", Format);
	if (Map)
	{
		Map->Dump();
	}
	else
	{
		printf("No mapping object.\n");
	}
}

int TtfCMapByteEnc::operator[](int i)
{
	return (i >= 0 AND i < sizeof(Map)) ? Map[i] : 0;
}

bool TtfCMapByteEnc::Read(GFile &F)
{
	F >> Format;
	F >> Length;
	F >> Version;
	F.Read(Map, sizeof(Map));
	return NOT F.GetStatus();
}

bool TtfCMapByteEnc::Write(GFile &F)
{
	F << Format;
	F << Length;
	F << Version;
	F.Write(Map, sizeof(Map));
	return NOT F.GetStatus();
}

void TtfCMapByteEnc::Dump()
{
	printf("Format: %i\n", Format);
	printf("Length: %i\n", Length);
	printf("Version: %i.%i\n", Version>>8, Version&0xFF);
	for (int i=0; i<sizeof(Map); i++)
	{
		printf("   [%i]: %i\n", i, Map[i]);
	}
	printf("\n");
}

TtfCMapSegDelta::TtfCMapSegDelta()
{
	EndCount = 0;
	StartCount = 0;
	IdDelta = 0;
	IdRangeOffset = 0;
}

TtfCMapSegDelta::~TtfCMapSegDelta()
{
	DeleteArray(EndCount);
	DeleteArray(StartCount);
	DeleteArray(IdDelta);
	DeleteArray(IdRangeOffset);
}

int TtfCMapSegDelta::operator[](int c)
{
	for (int i=0; i<SegCount; i++)
	{
		if (c <= EndCount[i] AND c >= StartCount[i])
		{
			if (IdRangeOffset[i])
			{
				return *(IdRangeOffset[i]/2 + (c - StartCount[i]) + &IdRangeOffset[i]);
			}
			else
			{
				return c + IdDelta[i];
			}
		}
	}
	return 0;
}

bool TtfCMapSegDelta::Read(GFile &F)
{
	F >> Format;
	F >> Length;
	F >> Version;
	F >> SegCountX2;
	SegCount = SegCountX2 / 2;

	F >> SearchRange;
	F >> EntrySelector;
	F >> RangeShift;
	
	EndCount = ReadUShortArray(F, SegCount);
	F.Seek(2, SEEK_CUR);
	StartCount = ReadUShortArray(F, SegCount);
	IdDelta = ReadShortArray(F, SegCount);

	// This seems really bodgy but the doco doesn't say
	// how to calculate the size of this array
	IdCount = (Length - 16 - (SegCount * 8)) / 2;

	// the GlyphIdArray is appended to the
	// end of the IdRangeOffset array
	IdRangeOffset = ReadUShortArray(F, SegCount + IdCount);
	
	return	(NOT F.GetStatus()) AND
			EndCount AND
			StartCount AND
			IdDelta AND
			IdRangeOffset;
}

bool TtfCMapSegDelta::Write(GFile &F)
{
	return FALSE;
}

void TtfCMapSegDelta::Dump()
{
	printf("Format: %i\n", Format);
	printf("Length: %i\n", Length);
	printf("Version: %i.%i\n", Version>>8, Version&0xFF);
	printf("SegCount: %i\n", SegCount);
	printf("SearchRange: %i\n", SearchRange);
	printf("EntrySelector: %i\n", EntrySelector);
	printf("RangeShift: %i\n", RangeShift);
	
	printf("Segments: (StartCount, EndCount, IdDelta, IdRangeOffset)\n");
	for (int i=0; i<SegCount; i++)
	{
		printf("   [%i](%i, %i, %i, %i)\n", i, StartCount[i], EndCount[i], IdDelta[i], IdRangeOffset[i]);
	}

	printf("GlyphIdArray:\n");
	printf("IdCount: %i\n", IdCount);
	for (i=0; i<IdCount; i++)
	{
		printf("   [%i]: %i\n", i, IdRangeOffset[SegCount+i]);
	}
}

TtfCMap::TtfCMap()
{
	Fravorite = 0;
	Table = 0;
}

TtfCMap::~TtfCMap()
{
	Fravorite = 0;
	DeleteArray(Table);
}

bool TtfCMap::Read(GFile &F)
{
	bool Status = FALSE;
	int StartAddress = F.GetPosition();

	F >> Version;
	F >> Tables;
	Table = new TtfCMapTable[Tables];
	if (Table)
	{
		for (int i=0; i<Tables; i++)
		{
			Table[i].Read(F);
		}

		Fravorite = 0;
		for (i=0; i<Tables; i++)
		{
			F.Seek(StartAddress + Table[i].Offset, SEEK_SET);
			F >> Table[i].Format;
			F.Seek(-2, SEEK_CUR);
		
			switch (Table[i].Format)
			{
				case 0:
				{
					Table[i].Map = new TtfCMapByteEnc;
					if (NOT Fravorite)
					{
						Fravorite = Table[i].Map;
					}
					break;
				}
				case 4:
				{
					Table[i].Map = new TtfCMapSegDelta;
					Fravorite = Table[i].Map;
					break;
				}
			}

			if (Table[i].Map)
			{
				Table[i].Map->Read(F);
			}
		}

		Status = TRUE;
	}

	return Status;
}

bool TtfCMap::Write(GFile &F)
{
	bool Status = FALSE;

	F << Version;
	F << Tables;
	if (Table)
	{
		for (int i=0; i<Tables; i++)
		{
			Table[i].Write(F);
		}
		Status = TRUE;
	}

	return Status;
}

void TtfCMap::Dump()
{
	printf("Version: %i.%i\n", Version>>8, Version&0xFF);
	printf("Tables: %i\n", Tables);
	if (Table)
	{
		for (int i=0; i<Tables; i++)
		{
			printf("Table[%i]:\n", i);
			Table[i].Dump();
		}
	}
	printf("\n");
}

TtfRaster::TtfRaster()
{
	BaseLine = 0;
	XPixelsPerEm = 0;
	YPixelsPerEm = 0;
	Glyphs = 0;
	pSource = NULL;
	pDC = NULL;
}

TtfRaster::~TtfRaster()
{
	DeleteArray(BaseLine);
	DeleteArray(pSource);
	DeleteObj(pDC);
}

class TtfResizeDC : public GSurface {
public:
	bool ResizeTo(GSurface *pDC)
	{
		bool Status = FALSE;
		if (pDC AND pDC->GetBits() == 8 AND GetBits() == 8)
		{
			int XScale = X() / pDC->X();
			int YScale = Y() / pDC->Y();
			int Units = XScale * YScale;
			if (Units == 1)
			{
				pDC->Blt(0, 0, this);
			}
			else
			{
				for (int y=0; y<pDC->Y(); y++)
				{
					uchar *d = (*pDC)[y];
					for (int x=0; x<pDC->X(); x++)
					{
						int Total = 0;
	
						for (int yy=0; yy<YScale; yy++)
						{
							uchar *p = (*this)[y*YScale + yy] + x*XScale;
							for (int xx=0; xx<XScale; xx++)
							{
								Total += (*p) ? 1 : 0;
								p++;
							}
						}
	
						*d++ = (Total * 255) / Units;
					}
				}
			}
		}
		return Status;
	}
};

bool TtfRaster::Rasterize(double xPPEm, double yPPEm, int OverSample)
{
	bool Status = FALSE;
	TtfGlyph **Glyph = ((TtfGlyph**) FindTag("glyf"));
	TtfMaxProfile *Profile = (TtfMaxProfile*) FindTag("maxp");
	TtfHeader *Header = (TtfHeader*) FindTag("head");

	if (NOT Glyph OR NOT Profile OR NOT Header OR OverSample < 1) return FALSE;

	XPixelsPerEm = xPPEm;
	YPixelsPerEm = yPPEm;
	Glyphs = Profile->NumGlyphs;

	pSource = new GRect[Glyphs];
	BaseLine = new int[Glyphs];

	if (Glyph AND pSource AND BaseLine)
	{
		int TotalY = 0;
		int MaxX = 0;
		double XUnits = (double) XPixelsPerEm / Header->UnitsPerEm;
		double YUnits = (double) YPixelsPerEm / Header->UnitsPerEm;

		memset(pSource, 0, sizeof(GRect) * Glyphs);

		for (int i=0; i<Glyphs; i++)
		{
			int x = max(ceil(XUnits * Glyph[i]->GetX()), 1);
			int Top = ceil(YUnits * Glyph[i]->yMax);
			int Bottom = floor(YUnits * Glyph[i]->yMin);
			int y = max(Top - Bottom, 1);

			BaseLine[i] = Top;
			pSource[i].ZOff(x-1, y-1);
			pSource[i].Offset(0, TotalY);
			
			MaxX = max(MaxX, x);
			TotalY += y;
		}

		TtfResizeDC *pOver = new TtfResizeDC;
		if (pOver AND pOver->Create(MaxX * OverSample, TotalY * OverSample, 8))
		{
			pOver->Colour(0);
			pOver->Rectangle(0, 0, pOver->X(), pOver->Y());

			Status = TRUE;

			for (i=0; i<Glyphs; i++)
			{
				if (Glyph[i])
				{
					GRect a = *(pSource + i);
					GRect b;

					b.x1 = a.x1 * OverSample;
					b.y1 = a.y1 * OverSample;
					b.x2 = (a.x2 + 1) * OverSample - 1;
					b.y2 = (a.y2 + 1) * OverSample - 1;
					
					Status &= Glyph[i]->Rasterize(	pOver,
									&b,
									XPixelsPerEm * OverSample,
									YPixelsPerEm * OverSample,
									BaseLine[i] * OverSample);

					/*
					if (Getch() == 27)
					{
						break;
					}
					*/

					if (NOT Status)
					{
						// printf("Glyph[%i] failed\n", i);
						Status = TRUE;
					}
				}
			}

			pDC = new GSurface;
			if (pDC AND pDC->Create(MaxX, TotalY, 8))
			{
				if (OverSample > 1)
				{
					pOver->ResizeTo(pDC);
				}
				else
				{
					pDC->Blt(0, 0, pOver, NULL);
				}
			}
		}

		DeleteObj(pOver);
	}

	return Status;
}

void GdcTtf::Test(GSurface *pDC)
{
	if (pDC)
	{
		#define SIZE 55

		int k=0;
		int w = pDC->X() / SIZE;
		#ifdef DRAW_POINTS
		// int i = 37;
		int i = 220;
		#else
		for (int i=0; i<Profile.NumGlyphs; i++)
		#endif
		{
			if (Glyph[i])
			{
				int x = (k % w) * SIZE;
				int y = (k / w) * SIZE;
				k++;

				#ifdef DRAW_POINTS
				Glyph[i]->Draw(pDC, 10, 10, pDC->Y()-20);
				#else
				Glyph[i]->Draw(pDC, x, y, SIZE-2);
				#endif
			}
		}
	}
}




GdcTtf::GdcTtf()
{
	TableList = 0;
	Profile.NumGlyphs = 0;
	Glyph = 0;
	Rasters = 0;
	Raster = 0;

	ForeCol = 0;
	BackCol = 0xFFFFFFFF;
	Trans = TRUE;
}

GdcTtf::~GdcTtf()
{
	DeleteArray(TableList);

	if (Glyph)
	{
		for (int i=0; i<Profile.NumGlyphs; i++)
		{
			DeleteObj(Glyph[i]);
		}
		DeleteArray(Glyph);
	}

	if (Raster)
	{
		for (int i=0; i<Rasters; i++)
		{
			DeleteObj(Raster[i]);
		}
		DeleteArray(Raster);
	}
}

TtfRaster *GdcTtf::FindRaster(int Point, int XDpi, int YDpi)
{
	if (YDpi <= 0) YDpi = XDpi;

	TtfRaster *Best = NULL;
	if (Raster)
	{
		double XPixelsPerEm = ((double) Point * XDpi) / 72.0;
		double YPixelsPerEm = ((double) Point * YDpi) / 72.0;
		double BestX = 100000000.0;
		double BestY = 100000000.0;

		for (int i=0; i<Rasters; i++)
		{
			double XDistance = abs(Raster[i]->GetXPixelsPerEm() - XPixelsPerEm);
			double YDistance = abs(Raster[i]->GetYPixelsPerEm() - YPixelsPerEm);

			if (	XDistance < BestX OR
				(XDistance == BestX AND YDistance < BestY))
			{
				Best = Raster[i];
				BestX = abs(Best->GetXPixelsPerEm() - XPixelsPerEm);
				BestY = abs(Best->GetYPixelsPerEm() - YPixelsPerEm);
			}
		}
	}

	return Best;
}

TtfTable *GdcTtf::FindTag(char *t)
{
	if (NOT TableList) return NULL;

	for (int i=0; i<Tables; i++)
	{
		if (strncmp(t, TableList[i].Tag, 4) == 0)
		{
			return TableList + i;
		}
	}

	return NULL;
}

TtfTable *GdcTtf::SeekTag(char *t, GFile *F)
{
	TtfTable *Info = NULL;
	if (TableList)
	{
		Info = FindTag(t);
		if (Info AND F)
		{
			F->Seek(Info->Offset, SEEK_SET);
		}
	}
	return Info;
}

bool GdcTtf::Load(GFile &F)
{
	bool Status = FALSE;
	TtfFileHeader FileHeader;

	F.SetSwap(TRUE);
	if (FileHeader.Read(F))
	{
		int ObjectLoad = 0;

		Tables = FileHeader.NumTables;
		TableList = new TtfTable[Tables];
		if (TableList)
		{
			for (int i=0; i<Tables; i++)
			{
				if (TableList[i].Read(F))
				{
					// TableList[i].Dump();
				}
			}
		}

		Header.Parent = this;
		Profile.Parent = this;
		Location.Parent = this;
		Map.Parent = this;

		TtfTable *Info = SeekTag("head", &F);
		if (Info)
		{
			Info->Table = &Header;
			if (Header.Read(F))
			{
				ObjectLoad++;
				// Header.Dump();
			}
		}

		Info = SeekTag("maxp", &F);
		if (Info)
		{
			Info->Table = &Profile;
			if (Profile.Read(F))
			{
				ObjectLoad++;
				// Profile.Dump();
			}
		}

		Info = SeekTag("loca", &F);
		if (Info)
		{
			Info->Table = &Location;
			if (Location.Read(F))
			{
				ObjectLoad++;
				// Location.Dump();
			}

		}

		Info = SeekTag("glyf", &F);
		if (Info)
		{
			Glyph = new TtfGlyph*[Profile.NumGlyphs];
			Info->Table = Glyph;

			if (Glyph)
			{
				for (int i=0; i<Profile.NumGlyphs; i++)
				{
					if (Location[i] != Location[i+1])
					{
						Glyph[i] = new TtfGlyph;
						if (Glyph[i])
						{
							F.Seek(Info->Offset + Location[i], SEEK_SET);
							Glyph[i]->Parent = this;
							Glyph[i]->Read(F);

							/*
							printf("Glyph: %i (size: %i)\n", i, Location[i+1]-Location[i]);
							Glyph[i]->Dump();
							*/
						}
					}
					else
					{
						Glyph[i] = 0;
					}
				}
			}
		}

		Info = SeekTag("cmap", &F);
		if (Info)
		{
			Info->Table = &Map;
			if (Map.Read(F))
			{
				ObjectLoad++;
				// Map.Dump();
			}
		}

		Status = (ObjectLoad == 4);
	}

	return Status;
}

bool GdcTtf::Save(GFile &F)
{
	return FALSE;
}

bool GdcTtf::Rasterize(int Point, int StyleFlags, int OverSample, int XDpi, int YDpi)
{
	bool Status = FALSE;

	pTest = new GSurface;
	if (pTest)
	{
		pTest->Create(600, 800, 24);
	}
	else
	{
		return FALSE;
	}

	if (YDpi < 0) YDpi = XDpi;

	TtfRaster *New = new TtfRaster;
	if (New)
	{
		double Sx = ((double) Point * XDpi) / 72.0;
		double Sy = ((double) Point * YDpi) / 72.0;

		CapA = Map['A'];
		New->Parent = this;
		if (New->Rasterize(Sx, Sy, OverSample))
		{
			TtfRaster **Temp = new TtfRaster*[Rasters+1];
			if (Temp)
			{
				MemCpy(Temp, Raster, sizeof(TtfRaster*)*Rasters);
				Temp[Rasters] = New;
				DeleteArray(Raster);
				Raster = Temp;
				Rasters++;

				Status = TRUE;
			}
		}
		else
		{
			DeleteObj(New);
		}
	}

	DeleteObj(pTest);

	return Status;
}

#define TTF_SPACE 12

void GdcTtf::Size(int *x, int *y, char *Str, int Len, int Flags)
{
	int Point = 10;
	double PixelsPerEm = ((double) Point * 96.0) / 72.0;
	char *Ptr = Str;
	int CurX = 0;
	int CurY = 0;

	if (NOT Glyph) return;
	TtfRaster *Raster = FindRaster(Point, 96);
	if (Ptr AND Raster)
	{
		if (Len < 0) Len = strlen(Str);

		double r = PixelsPerEm / Header.UnitsPerEm;
		int Space = max((int) (PixelsPerEm / TTF_SPACE), 1);
		int BaseY = (r * Header.yMax);
		
		while (*Ptr)
		{
			int Index = Map[*Ptr];
			TtfGlyph *g = Glyph[Index];

			if (NOT g)
			{
				g = Glyph[0];
			}

			CurX += (r * g->GetX()) + Space;
			CurY = max(CurY, BaseY - Raster->BaseLine[Index] + (r * g->GetY()));
			Ptr++;
		}
	}

	if (x)
	{
		*x = CurX;
	}

	if (y)
	{
		*y = CurY;
	}
}

int GdcTtf::X(char *Str, int Len, int Flags)
{
	int x = 0;
	Size(&x, NULL, Str, Len, Flags);
	return x;
}

int GdcTtf::Y(char *Str, int Len, int Flags)
{
	int y = 0;
	Size(NULL, &y, Str, Len, Flags);
	return y;
}


void StretchBlt(	GSurface *pDest,
			int X,
			int Y,
			GSurface *pSrc,
			GRect *SRgn,
			int Scale)
{
	if (pDest AND pSrc AND SRgn)
	{
		for (int y=0; y<SRgn->Y(); y++)
		{
			for (int x=0; x<SRgn->X(); x++)
			{
				int Sx = X + (Scale * x);
				int Sy = Y + (Scale * y);
				COLOUR c = pSrc->Get(SRgn->x1+x, SRgn->y1+y);
		
				pDest->Colour(Rgb32(c, c, c));
				pDest->Rectangle(Sx+1, Sy+1, Sx+Scale-1, Sy+Scale-1);
			}
		}
	}

	/*
	pDest->Colour(Rgb32(255,255,255));
	pDest->Box(X, Y, X+(Scale*SRgn->X()), Y+(Scale*SRgn->Y()));
	*/
}

int GlyphX, GlyphY;
int SDiff, DDiff;

class GdcFontApp : public GApplicator {

	uchar *Ptr;
	int Bytes;

public:
	GdcFontApp()
	{
		Ptr = 0;
		Bytes = 0;
	}

	bool SetSurface(GBmpMem *d, GPalette *p, GBmpMem *a)
	{
		Dest = d;
		Pal = p;
		if (Dest)
		{
			Bytes = (Dest->Bits  + 7) / 8;
			Ptr = Dest->Base;
			return TRUE;
		}

		return FALSE;
	}

	void SetPtr(int x, int y)
	{
		Ptr = Dest->Base + ((y * Dest->Line) + (x * Bytes));
	}

	virtual void IncX()
	{
		Ptr += Bytes;
	}

	virtual void IncY()
	{
		Ptr += Dest->Line;
	}

	virtual void IncPtr(int X, int Y)
	{
		Ptr += (Dest->Line * Y) + (X + Bytes);
	}

	void Set() {}
	COLOUR Get() { return 0; }
	void VLine(int height) {}
	void Rectangle(int x, int y) {}

	bool Blt(GBmpMem *Src, GPalette *SPal, GBmpMem *SrcAlpha)
	{
		bool Status = FALSE;

		if (Src AND Dest)
		{
			GlyphX = Src->x;
			GlyphY = Src->y;
			SDiff = Src->Line - (Src->x * Src->Bits / 8);
			DDiff = Dest->Line - (Src->x * Dest->Bits / 8);

			Status = TRUE;

			// Destination	= the Colour we are writing the font on to
			// Source	= the pattern to write with
			// c		= the foreground colour

			/*
			switch (Dest->Bits)
			{
				case 8:
				{
					FntMaskBlt8(Ptr, Src->Base, c);
					break;
				}

				case 16:
				{
					FntMaskBlt16(Ptr, Src->Base, c);
					break;
				}
				
				case 24:
				{
					FntMaskBlt24(Ptr, Src->Base, R24(c), G24(c), B24(c));
					break;
				}

				case 32:
				{
					FntMaskBlt32(Ptr, Src->Base, c);
					break;
				}

				default:
				{
					Status = FALSE;
					break;
				}
			}
			*/
		}

		return Status;
	}
};

void GdcTtf::Text(GSurface *pDC, int x, int y, char *Str, int Len)
{
	if (pDC AND Str)
	{
		int Point = 10;
		double PixelsPerEm = ((double) Point * 96.0) / 72.0;
		int CurX = x;
		char *Ptr = Str;

		if (Len < 0) Len = strlen(Str);
		if (NOT Glyph OR NOT pDC) return;
		
		TtfRaster *Raster = FindRaster(Point, 96);
		if (Ptr AND Raster)
		{
			if (NOT Trans)
			{
				int Sx, Sy;
				Size(&Sx, &Sy, Str);
				
				COLOUR c = pDC->Colour(BackCol);
				pDC->Rectangle(x, y, x+Sx-1, y + Sy-1);
				pDC->Colour(c);
			}

			double r = PixelsPerEm / Header.UnitsPerEm;
			int CurY = y + (r * Header.yMax);
			int Space = max((int) (PixelsPerEm / TTF_SPACE), 1);

			GdcFontApp *Pen = new GdcFontApp;
			if (Pen)
			{
				GApplicator *pOldApp = pDC->Applicator();
				pDC->Applicator(Pen);
				Pen->c = ForeCol;

				while (*Ptr)
				{
					int Index = Map[*Ptr];
					TtfGlyph *g = Glyph[Index];

					if (NOT g)
					{
						g = Glyph[0];
					}

					pDC->Blt(CurX, CurY - Raster->BaseLine[Index], Raster->pDC, Raster->pSource + Index);
					// g->DrawEm(pDC, CurX - (r * g->xMin), CurY, Header.UnitsPerEm, PixelsPerEm);

					CurX += ceil(r * g->GetX()) + Space;
					Ptr++;
				}

				pDC->Applicator(pOldApp);
				DeleteObj(Pen);
			}
		}
	}
}

/*
#define SCALE 25
#define Scale(i)	(((int)(i)) * SCALE)

COLOUR c = pDC->Colour(GdcD->GetColour(Rgb24(48,48,48)));
for (int ny=0; ny<pDC->Y()/SCALE; ny++)
{
	pDC->Line(0, Scale(ny), pDC->X(), Scale(ny));
}
for (int nx=0; nx<pDC->X()/SCALE; nx++)
{
	pDC->Line(Scale(nx), 0, Scale(nx), pDC->Y());
}
pDC->Colour(c);

	StretchBlt(	pDC,
			Scale(CurX),
			Scale(CurY - (r*g->yMax)),
			Raster->pDC,
			Raster->pSource + Index,
			SCALE);

	g->DrawEm(	pDC,
			((double) CurX - (r * g->xMin)) * SCALE,
			((double) CurY) * SCALE,
			Header.UnitsPerEm,
			PixelsPerEm * SCALE);
*/

