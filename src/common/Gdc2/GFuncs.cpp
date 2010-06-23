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
	if (d AND s > 0) memset(d, c, s);
}

void MemCpy(void *d, void *s, uint l)
{
	if (d AND s AND l > 0) memcpy(d, s, l);
}

void MemAnd(void *d, void *s, uint l)
{
	uchar *D = (uchar*) d;
	uchar *S = (uchar*) s;
	if (D AND S)
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
	if (D AND S)
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
	if (D AND S)
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
	if (Width < 1 OR r.X() < (2 * Width) OR r.Y() < (2 * Width))
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
			if (!Next OR p <= This->Pos)
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
				uint8 r = (R32(This->Colour) * t) + (R32(Next->Colour) * n);
				uint8 g = (G32(This->Colour) * t) + (G32(Next->Colour) * n);
				uint8 b = (B32(This->Colour) * t) + (B32(Next->Colour) * n);
				uint8 a = (A32(This->Colour) * t) + (A32(Next->Colour) * n);
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

