#include "Lgi.h"
#include "GPalette.h"

/////////////////////////////////////////////////////////////////////////////
// Mem ops
void Add64(ulong *DestH, ulong *DestL, ulong SrcH, ulong SrcL)
{
	*DestH += SrcH + (*DestL & SrcL & 0x80000000) ? 1 : 0;
	*DestL += SrcL;
}

void MemSet(void *d, int c, uint s)
{
	if (d && s > 0) memset(d, c, s);
}

void MemCpy(void *d, void *s, uint l)
{
	if (d && s && l > 0) memcpy(d, s, l);
}

void MemAnd(void *d, void *s, uint l)
{
	uchar *D = (uchar*) d;
	uchar *S = (uchar*) s;
	if (D && S)
	{
		for (; l > 0; l--)
		{
			*D++ &= *S++;
		}
	}
}

void MemXor(void *d, void *s, uint l)
{
	uchar *D = (uchar*) d;
	uchar *S = (uchar*) s;
	if (D && S)
	{
		for (; l > 0; l--)
		{
			*D++ ^= *S++;
		}
	}
}

void MemOr(void *d, void *s, uint l)
{
	uchar *D = (uchar*) d;
	uchar *S = (uchar*) s;
	if (D && S)
	{
		for (; l > 0; l--)
		{
			*D++ |= *S++;
		}
	}
}

//////////////////////////////////////////////////////////////////////
// Drawing functions
void LgiDrawBox(GSurface *pDC, GRect &r, bool Sunken, bool Fill)
{
	if (Fill)
	{
		pDC->Colour(LC_MED, 24);
		pDC->Rectangle(r.x1+1, r.y1+1, r.x2-1, r.y2-1);
	}

	pDC->Colour((Sunken) ? LC_LIGHT : LC_LOW, 24);
	pDC->Line(r.x2, r.y2, r.x2, r.y1);
	pDC->Line(r.x2, r.y2, r.x1, r.y2);

	pDC->Colour((Sunken) ? LC_LOW : LC_LIGHT, 24);
	pDC->Line(r.x1, r.y1, r.x1, r.y2);
	pDC->Line(r.x1, r.y1, r.x2, r.y1);
}

void LgiWideBorder(GSurface *pDC, GRect &r, LgiEdge Type)
{
	if (!pDC) return;
	COLOUR Old = pDC->Colour();
	COLOUR VLow = LC_SHADOW;
	COLOUR Low = LC_LOW;
	COLOUR High = LC_HIGH;
	COLOUR VHigh = LC_LIGHT;
	
	switch (Type)
	{
		case EdgeXpSunken:
		{
			// XP theme
			pDC->Colour(Low, 24);
			pDC->Line(r.x1, r.y1, r.x2-1, r.y1);
			pDC->Line(r.x1, r.y1, r.x1, r.y2-1);

			pDC->Colour(VLow, 24);
			pDC->Line(r.x1+1, r.y1+1, r.x2-2, r.y1+1);
			pDC->Line(r.x1+1, r.y1+1, r.x1+1, r.y2-2);

			pDC->Colour(High, 24);
			pDC->Line(r.x2-1, r.y2-1, r.x2-1, r.y1+1);
			pDC->Line(r.x2-1, r.y2-1, r.x1+1, r.y2-1);

			pDC->Colour(VHigh, 24);
			pDC->Line(r.x2, r.y2, r.x2, r.y1);
			pDC->Line(r.x2, r.y2, r.x1, r.y2);
			break;
		}
		case EdgeXpRaised:
		{
			pDC->Colour(VHigh, 24);
			pDC->Line(r.x1, r.y1, r.x2-1, r.y1);
			pDC->Line(r.x1, r.y1, r.x1, r.y2-1);

			pDC->Colour(High, 24);
			pDC->Line(r.x1+1, r.y1+1, r.x2-1, r.y1+1);
			pDC->Line(r.x1+1, r.y1+1, r.x1+1, r.y2-1);

			pDC->Colour(Low, 24);
			pDC->Line(r.x2-1, r.y2-1, r.x2-1, r.y1+1);
			pDC->Line(r.x2-1, r.y2-1, r.x1+1, r.y2-1);

			pDC->Colour(VLow, 24);
			pDC->Line(r.x2, r.y2, r.x2, r.y1);
			pDC->Line(r.x2, r.y2, r.x1, r.y2);
			break;
		}
		case EdgeXpChisel:
		{
			pDC->Colour(Low, 24);
			pDC->Line(r.x1, r.y1, r.x2-1, r.y1);
			pDC->Line(r.x1, r.y1, r.x1, r.y2-1);

			pDC->Colour(VHigh, 24);
			pDC->Line(r.x1+1, r.y1+1, r.x2-2, r.y1+1);
			pDC->Line(r.x1+1, r.y1+1, r.x1+1, r.y2-2);

			pDC->Colour(Low, 24);
			pDC->Line(r.x2-1, r.y2-1, r.x2-1, r.y1+1);
			pDC->Line(r.x2-1, r.y2-1, r.x1+1, r.y2-1);

			pDC->Colour(VHigh, 24);
			pDC->Line(r.x2, r.y2, r.x2, r.y1);
			pDC->Line(r.x2, r.y2, r.x1, r.y2);
			break;
		}
		case EdgeWin7Sunken:
		case EdgeWin7FocusSunken:
		{
			bool Focus = Type == EdgeWin7FocusSunken;
			
			// Win7 theme
			GColour Ws(LC_WORKSPACE, 24);
			GColour Corner1, Corner2, Corner3, Corner4, Corner5;
			GColour Top, Left, Right, Bottom;
			if (Focus)
			{
				Corner1.Rgb(0xad, 0xc4, 0xd7);
				Corner2.Rgb(0x5c, 0x93, 0xbc);
				Corner3.Rgb(0xc6, 0xde, 0xee);
				Corner4.Rgb(0xda, 0xe4, 0xed);
				Corner5.Rgb(0xc6, 0xde, 0xee);
				
				Left.Rgb(0xb5, 0xcf, 0xe7);
				Top.Rgb(0x3d, 0x7b, 0xad);
				Right.Rgb(0xa4, 0xc9, 0xe3);
				Bottom.Rgb(0xb7, 0xd9, 0xed);
			}
			else
			{
				Corner1.Rgb(0xd6, 0xd7, 0xd9);
				Corner2.Rgb(0xbb, 0xbd, 0xc2);
				Corner3.Rgb(0xe9, 0xec, 0xf0);
				Corner4.Rgb(0xeb, 0xeb, 0xee);
				Corner5.Rgb(0xe9, 0xec, 0xf0);
				
				Left.Rgb(0xe2, 0xe3, 0xea);
				Top.Rgb(0xab, 0xad, 0xb3);
				Right.Rgb(0xdb, 0xdf, 0xe6);
				Bottom.Rgb(0xe3, 0xe9, 0xef);
			}
			
			// Top corners
			pDC->Colour(Corner1);
			pDC->Set(r.x1, r.y1);
			pDC->Set(r.x2, r.y1);
			pDC->Colour(Corner2);
			pDC->Set(r.x1+1, r.y1);
			pDC->Set(r.x2-1, r.y1);
			pDC->Colour(Corner3);
			pDC->Set(r.x1+1, r.y1+1);
			pDC->Set(r.x2-1, r.y1+1);

			// Bottom corners.
			pDC->Colour(Corner4);
			pDC->Set(r.x1, r.y2);
			pDC->Set(r.x2, r.y2);
			pDC->Colour(Corner5);
			pDC->Set(r.x1+1, r.y2-1);
			pDC->Set(r.x2-1, r.y2-1);
			
			// Left edge
			pDC->Colour(Left);
			pDC->Line(r.x1, r.y1+1, r.x1, r.y2-1);
			pDC->Colour(Ws);
			pDC->Line(r.x1+1, r.y1+2, r.x1+1, r.y2-2);
			
			// Top edge
			pDC->Colour(Top);
			pDC->Line(r.x1+2, r.y1, r.x2-2, r.y1);
			pDC->Colour(Ws);
			pDC->Line(r.x1+2, r.y1+1, r.x2-2, r.y1+1);
			
			// Right edge
			pDC->Colour(Right);
			pDC->Line(r.x2, r.y1+1, r.x2, r.y2-1);	
			pDC->Colour(Ws);
			pDC->Line(r.x2-1, r.y1+2, r.x2-1, r.y2-2);	

			// Bottom edge
			pDC->Colour(Bottom);
			pDC->Line(r.x1+1, r.y2, r.x2-1, r.y2);
			pDC->Colour(Ws);
			pDC->Line(r.x1+2, r.y2-1, r.x2-2, r.y2-1);
			break;
		}
		default:
		{
			return;
		}	
	}

	r.Size(2, 2);
	pDC->Colour(Old);
}

void LgiThinBorder(GSurface *pDC, GRect &r, LgiEdge Type)
{
	if (!pDC) return;
	COLOUR Old = pDC->Colour();

	switch (Type)
	{
		case EdgeXpSunken:
		case EdgeWin7FocusSunken:
		case EdgeWin7Sunken:
		{
			pDC->Colour(LC_LIGHT, 24);
			pDC->Line(r.x2, r.y2, r.x2, r.y1);
			pDC->Line(r.x2, r.y2, r.x1, r.y2);

			pDC->Colour(LC_LOW, 24);
			pDC->Line(r.x1, r.y1, r.x1, r.y2);
			pDC->Line(r.x1, r.y1, r.x2, r.y1);

			r.Size(1, 1);
			break;
		}
		case EdgeXpRaised:
		{
			pDC->Colour(LC_LOW, 24);
			pDC->Line(r.x2, r.y2, r.x2, r.y1);
			pDC->Line(r.x2, r.y2, r.x1, r.y2);

			pDC->Colour(LC_LIGHT, 24);
			pDC->Line(r.x1, r.y1, r.x1, r.y2);
			pDC->Line(r.x1, r.y1, r.x2, r.y1);

			r.Size(1, 1);
			break;
		}
		default:
		{
			LgiAssert(0);
			break;
		}
	}
	
	pDC->Colour(Old);
}

void LgiFlatBorder(GSurface *pDC, GRect &r, int Width)
{
	pDC->Colour(LC_MED, 24);
	if (Width < 1 || r.X() < (2 * Width) || r.Y() < (2 * Width))
	{
		pDC->Rectangle(&r);
		r.ZOff(-1, -1);
	}
	else
	{
		pDC->Rectangle(r.x1, r.y1, r.x2, r.y1+Width-1);
		pDC->Rectangle(r.x1, r.y2-Width+1, r.x2, r.y2);
		pDC->Rectangle(r.x1, r.y1+Width, r.x1+Width-1, r.y2-Width);
		pDC->Rectangle(r.x2-Width+1, r.y1+Width, r.x2, r.y2-Width);
		r.Size(Width, Width);
	}
}

void LgiFillGradient(GSurface *pDC, GRect &r, bool Vert, GArray<GColourStop> &Stops)
{
	int CurStop = 0;
	GColourStop *This = Stops.Length() > CurStop ? &Stops[CurStop] : 0;
	CurStop++;
	GColourStop *Next = Stops.Length() > CurStop ? &Stops[CurStop] : 0;

	int Limit = Vert ? r.Y() : r.X();
	for (int n=0; n<Limit; n++)
	{
		COLOUR c = Rgb32(0, 0, 0);
		float p = (float)n/Limit;
		if (This)
		{
			DoStop:
			if (!Next || p <= This->Pos)
			{
				// Just this
				c = This->Colour;
			}
			else if (p > This->Pos && p < Next->Pos)
			{
				// Interpolate between this and next
				float d = Next->Pos - This->Pos;
				float t = (Next->Pos - p) / d;
				float n = (p - This->Pos) / d;
				uint8 r = (uint8) ((R32(This->Colour) * t) + (R32(Next->Colour) * n));
				uint8 g = (uint8) ((G32(This->Colour) * t) + (G32(Next->Colour) * n));
				uint8 b = (uint8) ((B32(This->Colour) * t) + (B32(Next->Colour) * n));
				uint8 a = (uint8) ((A32(This->Colour) * t) + (A32(Next->Colour) * n));
				c = Rgba32(r, g, b, a);
			}
			else if (p >= Next->Pos)
			{
				// Get next colour stops
				This = Stops.Length() > CurStop ? &Stops[CurStop] : 0;
				CurStop++;
				Next = Stops.Length() > CurStop ? &Stops[CurStop] : 0;
				goto DoStop;
			}

			pDC->Colour(c, 32);
			if (Vert)
				pDC->Line(r.x1, r.y1 + n, r.x2, r.y1 + n);
			else
				pDC->Line(r.x1 + n, r.y1, r.x1 + n, r.y2);
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////
// Other Gdc Stuff
GSurface *ConvertDC(GSurface *pDC, int Bits)
{
	GSurface *pNew = new GMemDC;
	if (pNew && pNew->Create(pDC->X(), pDC->Y(), Bits))
	{
		pNew->Blt(0, 0, pDC);
		DeleteObj(pDC);
		return pNew;
	}
	return pDC;
}

GColour GdcMixColour(GColour c1, GColour c2, float HowMuchC1)
{
	float HowMuchC2 = 1.0 - HowMuchC1;

	uint8 r = (uint8) ((c1.r()*HowMuchC1) + (c2.r()*HowMuchC2));
	uint8 g = (uint8) ((c1.g()*HowMuchC1) + (c2.g()*HowMuchC2));
	uint8 b = (uint8) ((c1.b()*HowMuchC1) + (c2.b()*HowMuchC2));
	uint8 a = (uint8) ((c1.a()*HowMuchC1) + (c2.a()*HowMuchC2));
	
	return GColour(r, g, b, a);
}

COLOUR GdcMixColour(COLOUR c1, COLOUR c2, float HowMuchC1)
{
	double HowMuchC2 = 1.0 - HowMuchC1;
	uint8 r = (uint8) ((R24(c1)*HowMuchC1) + (R24(c2)*HowMuchC2));
	uint8 g = (uint8) ((G24(c1)*HowMuchC1) + (G24(c2)*HowMuchC2));
	uint8 b = (uint8) ((B24(c1)*HowMuchC1) + (B24(c2)*HowMuchC2));
	return Rgb24(r, g, b);
}

COLOUR GdcGreyScale(COLOUR c, int Bits)
{
	COLOUR c24 = CBit(24, c, Bits);

	int r = R24(c24) * 76;
	int g = G24(c24) * 150;
	int b = B24(c24) * 29;
	
	return (r + g + b) >> 8;
}

COLOUR CBit(int DstBits, COLOUR c, int SrcBits, GPalette *Pal)
{
	if (SrcBits == DstBits)
	{
		return c;
	}
	else
	{
		COLOUR c24 = 0;

		switch (SrcBits)
		{
			case 8:
			{
				GdcRGB Grey, *p = 0;
				if (!Pal || !(p = (*Pal)[c]))
				{
					Grey.r = Grey.g = Grey.b = c & 0xFF;
					p = &Grey;
				}

				switch (DstBits)
				{
					case 16:
					{
						return Rgb16(p->r, p->g, p->b);
					}
					case 24:
					{
						return Rgb24(p->r, p->g, p->b);
					}
					case 32:
					{
						return Rgb32(p->r, p->g, p->b);
					}
				}
				break;
			}
			case 15:
			{
				int R = R15(c);
				int G = G15(c);
				int B = B15(c);

				R = R | (R >> 5);
				G = G | (G >> 5);
				B = B | (B >> 5);

				switch (DstBits)
				{
					case 8:
					{
						if (Pal)
						{
							return Pal->MatchRgb(Rgb24(R, G, B));
						}
						break;
					}
					case 16:
					{
						return Rgb16(R, G, B);
					}
					case 24:
					{
						return Rgb24(R, G, B);
					}
					case 32:
					{
						return Rgb32(R, G, B);
					}
				}
				break;
			}
			case 16:
			{
				/*
				int R = ((c>>8)&0xF8) | ((c>>13)&0x7);
				int G = ((c>>3)&0xFC) | ((c>>9)&0x3);
				int B = ((c<<3)&0xF8) | ((c>>2)&0x7);
				*/
				int R = (c >> 8) & 0xF8;
				int G = (c >> 3) & 0xFC;
				int B = (c << 3) & 0xF8;

				switch (DstBits)
				{
					case 8:
					{
						if (Pal)
						{
							return Pal->MatchRgb(Rgb24(R, G, B));
						}
						break;
					}
					case 15:
					{
						return Rgb16To15(c);
					}
					case 24:
					{
						return Rgb24(R, G, B);
					}
					case 32:
					{
						return Rgb32(R, G, B);
					}
				}
				break;
			}

			case 24:
			{
				switch (DstBits)
				{
					case 8:
					{
						if (Pal)
						{
							return Pal->MatchRgb(c);
						}
						break;
					}
					case 15:
					{
						return Rgb24To15(c);
					}
					case 16:
					{
						if (LgiGetOs() == LGI_OS_WIN32 ||
							LgiGetOs() == LGI_OS_WIN64)
						{
							return Rgb24To16(c);
						}

						int r = max(R24(c) - 1, 0) >> 3;
						int g = max(G24(c) - 1, 0) >> 2;
						int b = max(B24(c) - 1, 0) >> 3;

						return (r << 11) | (g << 5) | (b);
					}
					case 24:
					case 48:
					{
						return c;
					}
					case 32:
					case 64:
					{
						return Rgb24To32(c);
					}
					default:
						LgiAssert(0);
						break;
				}
				break;
			}

			case 32:
			{
				switch (DstBits)
				{
					case 8:
					{
						if (Pal)
						{
							return Pal->MatchRgb(Rgb32To24(c));
						}
						break;
					}
					case 15:
					{
						return Rgb32To15(c);
					}
					case 16:
					{
						return Rgb32To16(c);
					}
					case 24:
					{
						return Rgb32To24(c);
					}
				}
				break;
			}
		}
	}

	return c;
}

/////////////////////////////////////////////////////////////////////////////
const char *GColourSpaceToString(GColourSpace cs)
{
	#define CS_STR_BUF 4
	static int Cur = 0;
	static char Buf[CS_STR_BUF][16];
	static const char *CompTypes[] =
	{
		"N", // None
		"I", // Index
		"R", // Red
		"G", // Green
		"B", // Blue
		"A", // Alpha
		"X", // Pad
		"H", // Hue
		"S", // Sat
		"L", // Lum
		"C", // Cyan
		"M", // Magenta
		"Y", // Yellow
		"B", // Black
		"?",
		"?",
	};

	char *start = Buf[Cur++], *s = start;
	int total = 0;
	bool first = true;
	if (Cur >= CS_STR_BUF)
		Cur = 0;
	
	*s++ = 'C';
	*s++ = 's';
// printf("Converting Cs to String: 0x%x\n", cs);
	for (int i=3; i>=0; i--)
	{
		int c = (((uint32)cs) >> (i << 3)) & 0xff;
// printf("    c[%i] = 0x%x\n", i, c);
		if (c)
		{
			GComponentType type = (GComponentType)(c >> 4);
			int size = c & 0xf;
			if (first)
			{
				*s++ = CompTypes[type][0];
				first = false;
			}
			else
			{
				*s++ = tolower(CompTypes[type][0]);
			}
			
			total += size ? size : 16;
		}
	}

	s += sprintf_s(s, 4, "%i", total);	
	return start;
}

int GColourSpaceChannels(GColourSpace Cs)
{
	int Channels = 0;
	
	while (Cs)
	{
		uint8 c = Cs & 0xff;
		if (!c)
			break;
		
		Channels++;
		((int&)Cs) >>= 8;
	}
	
	return Channels;
}

bool GColourSpaceHasAlpha(GColourSpace Cs)
{
	while (Cs)
	{
		uint8 c = Cs & 0xff;
		GComponentType type = (GComponentType)(c >> 4);
		if (type == CtAlpha)
			return true;
		
		((int&)Cs) >>= 8;
	}
	
	return false;
}

int GColourSpaceToBits(GColourSpace ColourSpace)
{
	uint32 c = ColourSpace;
	int bits = 0;
	while (c)
	{
		if (c & 0xf0)
		{
			int n = c & 0xf;
			bits += n ? n : 16;
		}
		c >>= 8;
	}
	return bits;
}

GColourSpace GStringToColourSpace(const char *c)
{
	if (!c)
		return CsNone;

	if (!_strnicmp(c, "Cs", 2))
	{
		// "Cs" style colour space
		c += 2;
		
		const char *n = c;
		while (*n && !IsDigit(*n))
			n++;
		int Depth = ::atoi(n);
		
		if (!_strnicmp(c, "Index", 5))
		{
			if (Depth == 8)
				return CsIndex8;
		}
		else
		{
			GArray<GComponentType> Comp;
			while (*c)
			{
				switch (tolower(*c))
				{
					case 'r':
						Comp.Add(CtRed);
						break;
					case 'g':
						Comp.Add(CtGreen);
						break;
					case 'b':
						Comp.Add(CtBlue);
						break;
					case 'a':
						Comp.Add(CtAlpha);
						break;
					case 'x':
						Comp.Add(CtPad);
						break;
					case 'c':
						Comp.Add(CtCyan);
						break;
					case 'm':
						Comp.Add(CtMagenta);
						break;
					case 'y':
						Comp.Add(CtYellow);
						break;
					case 'k':
						Comp.Add(CtBlack);
						break;
					case 'h':
						Comp.Add(CtHue);
						break;
					case 'l':
						Comp.Add(CtLuminance);
						break;
					case 's':
						Comp.Add(CtSaturation);
						break;
				}
				c++;
			}
			
			GColourSpaceBits a;
			ZeroObj(a);
			if (Comp.Length() == 3)
			{
				a[0].Type(Comp[0]);
				a[1].Type(Comp[1]);
				a[2].Type(Comp[2]);
				if (Depth == 24)
				{
					a[0].Size(8);
					a[1].Size(8);
					a[2].Size(8);
				}
				else if (Depth == 16)
				{
					a[0].Size(5);
					a[1].Size(6);
					a[2].Size(5);
				}
				else if (Depth == 15)
				{
					a[0].Size(5);
					a[0].Size(5);
					a[0].Size(5);
				}
				else if (Depth == 48)
				{
					a[0].Size(0);
					a[1].Size(0);
					a[2].Size(0);
				}
				else return CsNone;
			}
			else if (Comp.Length() == 4)
			{
				a[0].Type(Comp[0]);
				a[1].Type(Comp[1]);
				a[2].Type(Comp[2]);
				a[3].Type(Comp[3]);
				if (Depth == 32)
				{
					a[0].Size(8);
					a[1].Size(8);
					a[2].Size(8);
					a[3].Size(8);
				}
				else if (Depth == 64)
				{
					a[0].Size(0);
					a[1].Size(0);
					a[2].Size(0);
					a[3].Size(0);
				}
				else return CsNone;
			}
			
			GColourSpace Cs = (GColourSpace)a.All;
			return Cs;
		}
	}
	else if (!_strnicmp(c, "System", 6))
	{
		// "System" colour space
		c += 6;
		int Depth = ::atoi(c);
		if (Depth)
			return GBitsToColourSpace(Depth);
	}
	
	return CsNone;
}

GColourSpace GBitsToColourSpace(int Bits)
{
	switch (Bits)
	{
		case 8:
			return CsIndex8;
		case 15:
			return System15BitColourSpace;
		case 16:
			return System16BitColourSpace;
		case 24:
			return System24BitColourSpace;
		case 32:
			return System32BitColourSpace;
		default:
			LgiAssert(!"Unknown colour space.");
			break;
	}
	
	return CsNone;
}

bool GColourSpaceTest()
{
	union {
		uint8 b4[4];
		uint32 u32;
		GBgrx32 bgrx32;
		GXrgb32 xrgb32;
		GRgba32 rgba32;
	};
	
	bool LeastSigBit = LEAST_SIG_BIT_FIRST;
	bool LeastSigByte = LEAST_SIG_BYTE_FIRST;

	b4[0] = 1;
	b4[1] = 2;
	b4[2] = 3;
	b4[3] = 4;
	#if 0 // def LGI_SDL
	printf("LeastSigBit=%i, LeastSigByte=%i, u32=%08x\n", LeastSigBit, LeastSigByte, u32);
	printf("bgrx32=%i,%i,%i,%i\n", bgrx32.b, bgrx32.g, bgrx32.r, bgrx32.pad);
	printf("xrgb32=%i,%i,%i,%i\n", xrgb32.pad, xrgb32.r, xrgb32.g, xrgb32.b);
	#endif
	if (bgrx32.b != 1 ||
		bgrx32.pad != 4 ||
		xrgb32.pad != 1 ||
		xrgb32.b != 4)
	{
		LgiAssert(!"32bit colour space byte ordering wrong. Flip the value of LEAST_SIG_BYTE_FIRST");
		return false;
	}
	
	union {
		uint16 u16;
		GRgb16 rgb16;
	};
	
	rgba32.r = 0xff;
	rgba32.g = 0;
	rgba32.b = 0;
	rgba32.a = 0;
	u16 = 0xf800;
	#if 0 // def LGI_SDL
	printf("rgba32=%i,%i,%i,%i or %08x\n", rgba32.r, rgba32.g, rgba32.b, rgba32.a, u32);
	printf("rgb16=%i,%i,%i or %04x\n", rgb16.r, rgb16.g, rgb16.b, u16);
	#endif
	if (rgb16.r != 0x1f ||
		rgb16.b != 0x0)
	{
		LgiAssert(!"16bit colour space bit ordering wrong. Flip the value of LEAST_SIG_BIT_FIRST");
		return false;
	}
	
	return true;
}

////////////////////////////////////////////////////////////////////////
GSurface *GInlineBmp::Create(uint32 TransparentPx)
{
	GSurface *pDC = new GMemDC;
	if (pDC->Create(X, Y, System32BitColourSpace))
	{
		int Line = X * Bits / 8;
		for (int y=0; y<Y; y++)
		{
			void *addr = ((uchar*)Data) + (y * Line);

			switch (Bits)
			{
				#if defined(MAC)
				case 16:
				{
					uint32 *s = (uint32*)addr;
					System32BitPixel *d = (System32BitPixel*) (*pDC)[y];
					System32BitPixel *e = d + X;
					while (d < e)
					{
						uint32 n = LgiSwap32(*s);
						s++;
						
						uint16 a = n >> 16;
						a = LgiSwap16(a);
						if (TransparentPx == a)
						{
							d->r = 0;
							d->g = 0;
							d->b = 0;
							d->a = 0;
						}
						else
						{
							d->r = Rc16(a);
							d->g = Gc16(a);
							d->b = Bc16(a);
							d->a = 255;
						}
						d++;
						
						if (d >= e)
							break;

						uint16 b = n & 0xffff;
						b = LgiSwap16(b);
						if (TransparentPx == b)
						{
							d->r = 0;
							d->g = 0;
							d->b = 0;
							d->a = 0;
						}
						else
						{
							d->r = Rc16(b);
							d->g = Gc16(b);
							d->b = Bc16(b);
							d->a = 255;
						}
						d++;
					}
					break;
				}
				#else
				case 16:
				{
					uint32 *out = (uint32*)(*pDC)[y];
					uint16 *in = (uint16*)addr;
					uint16 *end = in + X;
					while (in < end)
					{
						if (*in == TransparentPx)
							*out = 0;
						else
							*out = Rgb16To32(*in);						
						in++;
						out++;
					}
					break;
				}
				#endif
				case 24:
				{
					register uint8 r, g, b;
					r = R24(TransparentPx);
					g = R24(TransparentPx);
					b = B24(TransparentPx);
					
					System32BitPixel *out = (System32BitPixel*)(*pDC)[y];
					System32BitPixel *end = out + X;
					System24BitPixel *in = (System24BitPixel*)addr;
					while (out < end)
					{
						if (in->r == r &&
							in->g == g &&
							in->b == b)
						{
							out->r = 0;
							out->g = 0;
							out->b = 0;
							out->a = 0;
						}
						else
						{
							out->r = in->r;
							out->g = in->g;
							out->b = in->b;
							out->a = 255;
						}
						out++;
						in++;
					}
					break;
				}
				case 32:
				{
					memcpy((*pDC)[y], addr, Line);
					break;
				}
				default:
				{
					LgiAssert(!"Not a valid bit depth.");
					break;
				}
			}
		}
	}

	return pDC;
}

///////////////////////////////////////////////////////////////////////////////////////
#include "GPixelRops.h"

bool GUniversalBlt(	GColourSpace OutCs,
					uint8 *OutPtr,
					int OutLine,
					
					GColourSpace InCs,
					uint8 *InPtr,
					int InLine,
					
					int width,
					int height)
{
	if (OutCs == CsNone ||
		OutPtr == NULL ||
		OutLine == 0 ||
		InCs == CsNone ||
		InPtr == NULL ||
		InLine == 0)
	{
		LgiAssert(!"Invalid params");
		return false;
	}
	
	if (InCs == OutCs)
	{
		int Depth = GColourSpaceToBits(InCs);
		int PixelBytes = Depth >> 3;
		int ScanBytes = width * PixelBytes;
		LgiAssert(Depth % 8 == 0);
		
		// No conversion so just copy memory
		for (int y=0; y<height; y++)
		{
			memcpy(OutPtr, InPtr, ScanBytes);
			OutPtr += OutLine;
			InPtr += InLine;
		}
		
		return true;
	}
	
	bool InAlpha = GColourSpaceHasAlpha(InCs);
	bool OutAlpha = GColourSpaceHasAlpha(OutCs);
	if (InAlpha && OutAlpha)
	{
		// Compositing
		for (int y=0; y<height; y++)
		{
			switch (CsMapping(OutCs, InCs))
			{
				#define CompCase(OutType, InType) \
					case CsMapping(Cs##OutType, Cs##InType): \
						CompositeNonPreMul32((G##OutType*)OutPtr, (G##InType*)InPtr, width); \
						break

				CompCase(Rgba32, Bgra32);
				CompCase(Rgba32, Abgr32);
				CompCase(Rgba32, Argb32);

				CompCase(Bgra32, Rgba32);
				CompCase(Bgra32, Abgr32);
				CompCase(Bgra32, Argb32);

				CompCase(Argb32, Rgba32);
				CompCase(Argb32, Bgra32);
				CompCase(Argb32, Abgr32);

				CompCase(Abgr32, Rgba32);
				CompCase(Abgr32, Bgra32);
				CompCase(Abgr32, Argb32);
				
				#undef CompCase

				default:
					LgiAssert(!"Not impl.");
					return false;
			}

			OutPtr += OutLine;
			InPtr += InLine;
		}
	}
	else if (OutAlpha)
	{
		// Dest Alpha
		for (int y=0; y<height; y++)
		{
			switch (CsMapping(OutCs, InCs))
			{
				#define CopyCase(OutType, InType) \
					case CsMapping(Cs##OutType, Cs##InType): \
						Copy24to32((G##OutType*)OutPtr, (G##InType*)InPtr, width); \
						break

				CopyCase(Rgba32, Rgb24);
				CopyCase(Rgba32, Bgr24);
				CopyCase(Rgba32, Rgbx32);
				CopyCase(Rgba32, Bgrx32);
				CopyCase(Rgba32, Xrgb32);
				CopyCase(Rgba32, Xbgr32);

				CopyCase(Bgra32, Rgb24);
				CopyCase(Bgra32, Bgr24);
				CopyCase(Bgra32, Rgbx32);
				CopyCase(Bgra32, Bgrx32);
				CopyCase(Bgra32, Xrgb32);
				CopyCase(Bgra32, Xbgr32);

				CopyCase(Argb32, Rgb24);
				CopyCase(Argb32, Bgr24);
				CopyCase(Argb32, Rgbx32);
				CopyCase(Argb32, Bgrx32);
				CopyCase(Argb32, Xrgb32);
				CopyCase(Argb32, Xbgr32);

				CopyCase(Abgr32, Rgb24);
				CopyCase(Abgr32, Bgr24);
				CopyCase(Abgr32, Rgbx32);
				CopyCase(Abgr32, Bgrx32);
				CopyCase(Abgr32, Xrgb32);
				CopyCase(Abgr32, Xbgr32);
				
				#undef CopyCase
			
				default:
					LgiAssert(!"Not impl.");
					return false;
			}

			OutPtr += OutLine;
			InPtr += InLine;
		}
	}
	else
	{
		// No Dest Alpha
		for (int y=0; y<height; y++)
		{
			switch (CsMapping(OutCs, InCs))
			{
				#define CopyCase(OutType, InType) \
					case CsMapping(Cs##OutType, Cs##InType): \
						Copy24((G##OutType*)OutPtr, (G##InType*)InPtr, width); \
						break

				CopyCase(Rgb24, Bgr24);
				CopyCase(Rgb24, Rgba32);
				CopyCase(Rgb24, Bgra32);
				CopyCase(Rgb24, Argb32);
				CopyCase(Rgb24, Abgr32);
				CopyCase(Rgb24, Rgbx32);
				CopyCase(Rgb24, Bgrx32);
				CopyCase(Rgb24, Xrgb32);
				CopyCase(Rgb24, Xbgr32);

				CopyCase(Bgr24, Rgb24);
				CopyCase(Bgr24, Rgba32);
				CopyCase(Bgr24, Bgra32);
				CopyCase(Bgr24, Argb32);
				CopyCase(Bgr24, Abgr32);
				CopyCase(Bgr24, Rgbx32);
				CopyCase(Bgr24, Bgrx32);
				CopyCase(Bgr24, Xrgb32);
				CopyCase(Bgr24, Xbgr32);

				CopyCase(Rgbx32, Rgb24);
				CopyCase(Rgbx32, Bgr24);
				CopyCase(Rgbx32, Rgba32);
				CopyCase(Rgbx32, Bgra32);
				CopyCase(Rgbx32, Argb32);
				CopyCase(Rgbx32, Abgr32);
				CopyCase(Rgbx32, Rgbx32);
				CopyCase(Rgbx32, Bgrx32);
				CopyCase(Rgbx32, Xrgb32);
				CopyCase(Rgbx32, Xbgr32);
				
				CopyCase(Bgrx32, Rgb24);
				CopyCase(Bgrx32, Bgr24);
				CopyCase(Bgrx32, Rgba32);
				CopyCase(Bgrx32, Bgra32);
				CopyCase(Bgrx32, Argb32);
				CopyCase(Bgrx32, Abgr32);
				CopyCase(Bgrx32, Rgbx32);
				CopyCase(Bgrx32, Bgrx32);
				CopyCase(Bgrx32, Xrgb32);
				CopyCase(Bgrx32, Xbgr32);

				CopyCase(Xrgb32, Rgb24);
				CopyCase(Xrgb32, Bgr24);
				CopyCase(Xrgb32, Rgba32);
				CopyCase(Xrgb32, Bgra32);
				CopyCase(Xrgb32, Argb32);
				CopyCase(Xrgb32, Abgr32);
				CopyCase(Xrgb32, Rgbx32);
				CopyCase(Xrgb32, Bgrx32);
				CopyCase(Xrgb32, Xrgb32);
				CopyCase(Xrgb32, Xbgr32);
				
				CopyCase(Xbgr32, Rgb24);
				CopyCase(Xbgr32, Bgr24);
				CopyCase(Xbgr32, Rgba32);
				CopyCase(Xbgr32, Bgra32);
				CopyCase(Xbgr32, Argb32);
				CopyCase(Xbgr32, Abgr32);
				CopyCase(Xbgr32, Rgbx32);
				CopyCase(Xbgr32, Bgrx32);
				CopyCase(Xbgr32, Xrgb32);
				CopyCase(Xbgr32, Xbgr32);
				
				#undef CopyCase
			
				default:
					LgiAssert(!"Not impl.");
					return false;
			}

			OutPtr += OutLine;
			InPtr += InLine;
		}
	}
	
	
	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////
#include "GRops.h"

bool LgiRopRgb(uint8 *d, GColourSpace DstCs, uint8 *s, GColourSpace SrcCs, int x)
{
	// This is just a huge switch statement that takes care of all possible combinations
	// of src and dst RGB colour space formats. The 'GRopsCases.cpp' code is generated
	// automatically by 'generate_rops_cases.py' in the same folder as this code.
	#define JointCs(dst, src) (((uint64)(dst) << 32) | (src))
	switch (JointCs(DstCs, SrcCs))
	{
		#include "../../src/common/Gdc2/GRopsCases.cpp"
		default:
			LgiAssert(!"Unsupported pixel conversion.");
			return false;
	}
	return true;
}

/// Universal bit blt method
bool LgiRopUniversal(GBmpMem *Dst, GBmpMem *Src)
{
	if (!Dst || !Src)
		return false;

	// Work out conversion area...
	int Cx = min(Dst->x, Src->x);
	int Cy = min(Dst->y, Src->y);

	// Size of src and dst pixels:
	int SrcBits = GColourSpaceToBits(Src->Cs);
	int DstBits = GColourSpaceToBits(Dst->Cs);

	if (Dst->Cs == Src->Cs)
	{
		// No conversion idiom
		uint8 *d = Dst->Base;
		uint8 *s = Src->Base;
		int Bytes = (SrcBits * Cx) >> 3;
		for (int y=0; y<Cy; y++)
		{
			memcpy(d, s, Bytes);
			d += Dst->Line;
			s += Src->Line;
		}
		return true;
	}

	if (SrcBits > 8 && DstBits > 8)
	{
		bool Overlap = Dst->Overlap(Src);
		uint8 *d = Dst->Base;
		uint8 *s = Src->Base;

		for (int y=0; y<Cy; y++)
		{
			if (!LgiRopRgb(d, Dst->Cs, s, Src->Cs, Cx))
				return false;
			d += Dst->Line;
			s += Src->Line;
		}
		return true;
	}
	else
	{
	}

	LgiAssert(!"Unsupported pixel conversion.");
	return false;
}

////////////////////////////////////////////////////////////////////////////////
int LgiScreenDpi()
{
	return 96; // A reasonable default.
}
