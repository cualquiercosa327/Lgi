#include "Lgi.h"

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

void LgiWideBorder(GSurface *pDC, GRect &r, int Type)
{
	if (!pDC) return;
	COLOUR Old = pDC->Colour();
	COLOUR VLow = LC_SHADOW;
	COLOUR Low = LC_LOW;
	COLOUR High = LC_HIGH;
	COLOUR VHigh = LC_LIGHT;

	switch (Type)
	{
		case SUNKEN:
		{
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
		case RAISED:
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
		case CHISEL:
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
	}

	r.Size(2, 2);
	pDC->Colour(Old);
}

void LgiThinBorder(GSurface *pDC, GRect &r, int Type)
{
	if (!pDC) return;
	COLOUR Old = pDC->Colour();

	switch (Type)
	{
		case SUNKEN:
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
		case RAISED:
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
			else if (p > This->Pos AND p < Next->Pos)
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
	if (pNew AND pNew->Create(pDC->X(), pDC->Y(), Bits))
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
					Grey.R = Grey.G = Grey.B = c & 0xFF;
					p = &Grey;
				}

				switch (DstBits)
				{
					case 16:
					{
						return Rgb16(p->R, p->G, p->B);
					}
					case 24:
					{
						return Rgb24(p->R, p->G, p->B);
					}
					case 32:
					{
						return Rgb32(p->R, p->G, p->B);
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
						if (LgiGetOs() == LGI_OS_WINNT)
						{
							return Rgb24To16(c);
						}

						int r = max(R24(c) - 1, 0) >> 3;
						int g = max(G24(c) - 1, 0) >> 2;
						int b = max(B24(c) - 1, 0) >> 3;

						return (r << 11) | (g << 5) | (b);
					}
					case 32:
					{
						return Rgb24To32(c);
					}
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

GColourSpace GBitsToColourSpace(int Bits)
{
	switch (Bits)
	{
		case 8:
			return CsIndex8;
		case 15:
			return CsRgb15;
		case 16:
			return CsRgb16;
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

////////////////////////////////////////////////////////////////////////
GSurface *GInlineBmp::Create()
{
	GSurface *pDC = new GMemDC;
	if (pDC->Create(X, Y, 32))
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
						d->r = Rc16(a);
						d->g = Gc16(a);
						d->b = Bc16(a);
						d->a = 255;
						d++;
						
						if (d >= e)
							break;

						uint16 b = n & 0xffff;
						b = LgiSwap16(b);
						d->r = Rc16(b);
						d->g = Gc16(b);
						d->b = Bc16(b);
						d->a = 255;
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
						*out = Rgb16To32(*in);						
						in++;
						out++;
					}
					break;
				}
				#endif
				case 24:
				{
					System32BitPixel *out = (System32BitPixel*)(*pDC)[y];
					System32BitPixel *end = out + X;
					System24BitPixel *in = (System24BitPixel*)addr;
					while (out < end)
					{
						out->r = in->r;
						out->g = in->g;
						out->b = in->b;
						out->a = 255;
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

