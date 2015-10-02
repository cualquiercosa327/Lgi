/**
	\file
	\author Matthew Allen
	\date 4/3/1998
	\brief Draw mode effects
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "Gdc2.h"
#include "GPath.h"
#include "GPixelRops.h"
#include "GPalette.h"

// #define Div255(a)	DivLut[a]
#define Div255(a)	((a)/255)

template<typename T>
void CreatePaletteLut(T *c, GPalette *Pal, int Scale = 255)
{
	if (Scale < 255)
	{
		uchar *DivLut = Div255Lut;

		for (int i=0; i<256; i++)
		{
			GdcRGB *p = (Pal) ? (*Pal)[i] : 0;
			if (p)
			{
				c[i].r = DivLut[p->r * Scale];
				c[i].g = DivLut[p->g * Scale];
				c[i].b = DivLut[p->b * Scale];
			}
			else
			{
				c[i].r = DivLut[i * Scale];
				c[i].g = c[i].r;
				c[i].b = c[i].r;
			}
		}
	}
	else if (Scale)
	{
		for (int i=0; i<256; i++)
		{
			GdcRGB *p = (Pal) ? (*Pal)[i] : 0;
			if (p)
			{
				c[i].r = p->r;
				c[i].g = p->g;
				c[i].b = p->b;
			}
			else
			{
				c[i].r = i;
				c[i].g = i;
				c[i].b = i;
			}
		}
	}
	else
	{
		memset(c, 0, sizeof(*c) * 256);
	}
}

/// Alpha blending applicators
class GAlphaApp : public GApplicator
{
protected:
	uchar alpha, oma;
	int Bits, Bytes;
	uchar *Ptr;
	uchar *APtr;

	const char *GetClass() { return "GAlphaApp"; }

public:
	GAlphaApp()
	{
		Op = GDC_ALPHA;
		alpha = 0xFF;
		oma = 0;
		Bits = 8;
		Bytes = 1;
		Ptr = 0;
		APtr = 0;
	}

	int GetVar(int Var)
	{
		switch (Var)
		{
			case GAPP_ALPHA_A: return alpha;
		}
		return 0;
	}

	int SetVar(int Var, NativeInt Value)
	{
		switch (Var)
		{
			case GAPP_ALPHA_A:
			{
				int Old = alpha;
				alpha = Value;
				oma = 0xFF - alpha;
				return Old;
			}
			case GAPP_ALPHA_PAL:
			{

			}
		}
		return 0;
	}

	bool SetSurface(GBmpMem *d, GPalette *p, GBmpMem *a)
	{
		if (d && d->GetBits() == Bits)
		{
			Dest = d;
			Pal = p;
			Ptr = d->Base;
			Alpha = a;
			APtr = Alpha ? Alpha->Base : 0;

			return true;
		}
		return false;
	}

	void SetPtr(int x, int y)
	{
		LgiAssert(Dest && Dest->Base);
		Ptr = Dest->Base + ((y * Dest->Line) + (x * Bytes));
		if (APtr)
			APtr = Alpha->Base + ((y * Alpha->Line) + x);
	}

	void IncX()
	{
		Ptr += Bytes;
		if (APtr)
			APtr++;
	}

	void IncY()
	{
		Ptr += Dest->Line;
		if (APtr)
			APtr += Alpha->Line;
	}

	void IncPtr(int X, int Y)
	{
		Ptr += (Y * Dest->Line) + (X * Bytes);
		if (APtr)
			APtr += (Y * Dest->Line) + X;
	}

	COLOUR Get()
	{
		switch (Bytes)
		{
			case 1:
				return *((uint8*)Ptr);
			case 2:
				return *((uint16*)Ptr);
			case 3:
				return Rgb24(Ptr[2], Ptr[1], Ptr[0]);
			case 4:
				return *((uint32*)Ptr);
		}
		return 0;
	}
};

class GdcApp8Alpha : public GAlphaApp
{
	char Remap[256];
	uchar *DivLut;

public:
	GdcApp8Alpha();
	int SetVar(int Var, NativeInt Value);

	void Set();
	void VLine(int height);
	void Rectangle(int x, int y);
	bool Blt(GBmpMem *Src, GPalette *SPal, GBmpMem *SrcAlpha);
};

class GdcApp15Alpha : public GAlphaApp
{
public:
	GdcApp15Alpha()
	{
		Bits = 15; Bytes = 2;
	}

	void Set();
	void VLine(int height);
	void Rectangle(int x, int y);
	bool Blt(GBmpMem *Src, GPalette *SPal, GBmpMem *SrcAlpha);
};

class GdcApp16Alpha : public GAlphaApp
{
public:
	GdcApp16Alpha()
	{
		Bits = 16; Bytes = 2;
	}

	void Set();
	void VLine(int height);
	void Rectangle(int x, int y);
	bool Blt(GBmpMem *Src, GPalette *SPal, GBmpMem *SrcAlpha);
};

template<typename Pixel, GColourSpace ColourSpace>
class GdcAlpha : public GApplicator
{
protected:
	union {
		uint8 *u8;
		uint16 *u16;
		uint32 *u32;
		Pixel *p;
	};
	
	uint8 alpha, one_minus_alpha;
	
public:
	GdcAlpha()
	{
		p = NULL;
		alpha = 0xff;
		one_minus_alpha = 0;
	}

	const char *GetClass() { return "GdcAlpha"; }

	int GetVar(int Var)
	{
		switch (Var)
		{
			case GAPP_ALPHA_A: return alpha;
		}
		return 0;
	}

	int SetVar(int Var, NativeInt Value)
	{
		switch (Var)
		{
			case GAPP_ALPHA_A:
			{
				int Old = alpha;
				alpha = Value;
				one_minus_alpha = 0xFF - alpha;
				return Old;
			}
			case GAPP_ALPHA_PAL:
			{

			}
		}
		return 0;
	}

	bool SetSurface(GBmpMem *d, GPalette *p = 0, GBmpMem *a = 0)
	{
		if (d && d->Cs == ColourSpace)
		{
			Dest = d;
			Pal = p;
			u8 = d->Base;
			Alpha = a;

			return true;
		}
		else LgiAssert(0);
		
		return false;
	}
	
	void SetPtr(int x, int y)
	{
		u8 = Dest->Base + (Dest->Line * y);
		p += x;
	}
	
	void IncX()
	{
		p++;
	}
	
	void IncY()
	{
		u8 += Dest->Line;
	}
	
	void IncPtr(int X, int Y)
	{
		p += X;
		u8 += Dest->Line * Y;
	}

	COLOUR Get()
	{
		return Rgb24(p->r, p->g, p->b);
	}
};

template<typename Pixel, GColourSpace ColourSpace>
class GdcAlpha24 : public GdcAlpha<Pixel, ColourSpace>
{
public:
	const char *GetClass() { return "GdcAlpha24"; }

	#define InitComposite24() \
		uchar *DivLut = Div255Lut; \
		register uint8 a = alpha; \
		register uint8 oma = one_minus_alpha; \
		register int r = p24.r * a; \
		register int g = p24.g * a; \
		register int b = p24.b * a
	#define InitFlat24() \
		Pixel px; \
		px.r = p24.r; \
		px.g = p24.g; \
		px.b = p24.b
	#define Composite24(ptr) \
		ptr->r = DivLut[(oma * ptr->r) + r]; \
		ptr->g = DivLut[(oma * ptr->g) + g]; \
		ptr->b = DivLut[(oma * ptr->b) + b]

	void Set()
	{
		InitComposite24();
		Composite24(p);
	}
	
	void VLine(int height)
	{
		if (alpha == 255)
		{
			InitFlat24();
			while (height-- > 0)
			{
				*p = px;
				u8 += Dest->Line;
			}
		}
		else if (alpha > 0)
		{
			InitComposite24();
			while (height-- > 0)
			{
				Composite24(p);
				u8 += Dest->Line;
			}
		}
	}
	
	void Rectangle(int x, int y)
	{
		if (alpha == 0xff)
		{
			InitFlat24();
			while (y-- > 0)
			{
				register Pixel *s = p;
				register Pixel *e = s + x;
				while (s < e)
				{
					*p = px;
					s++;
				}
				u8 += Dest->Line;
			}
		}
		else if (alpha > 0)
		{
			InitComposite24();
			while (y-- > 0)
			{
				register Pixel *s = p;
				register Pixel *e = s + x;

				while (s < e)
				{
					Composite24(s);
					s++;
				}
				u8 += Dest->Line;
			}
		}
	}
	
	template<typename SrcPx>
	void CompositeBlt24(GBmpMem *Src)
	{
		uchar *Lut = Div255Lut;
		register uint8 a = alpha;
		register uint8 oma = one_minus_alpha;
		
		for (int y=0; y<Src->y; y++)
		{
			Pixel *dst = p;
			Pixel *dst_end = dst + Src->x;
			SrcPx *src = (SrcPx*)(Src->Base + (y * Src->Line));
			if (a == 0xff)
			{
				while (dst < dst_end)
				{
					// No source alpha, just copy blt
					dst->r = src->r;
					dst->g = src->g;
					dst->b = src->b;
					dst++;
					src++;
				}
			}
			else if (a > 0)
			{
				while (dst < dst_end)
				{
					// No source alpha, but apply our local alpha
					dst->r = Lut[(dst->r * oma) + (src->r * a)];
					dst->g = Lut[(dst->g * oma) + (src->g * a)];
					dst->b = Lut[(dst->b * oma) + (src->b * a)];
					dst++;
					src++;
				}
			}
			
			u8 += Dest->Line;
		}
	}
	
	template<typename SrcPx>
	void CompositeBlt32(GBmpMem *Src)
	{
		uchar *Lut = Div255Lut;
		register uint8 a = alpha;
		
		if (a == 0xff)
		{
			// Apply the source alpha only
			for (int y=0; y<Src->y; y++)
			{
				Pixel *dst = p;
				Pixel *dst_end = dst + Src->x;
				SrcPx *src = (SrcPx*)(Src->Base + (y * Src->Line));
				while (dst < dst_end)
				{
					register uint8 sa = src->a;
					register uint8 soma = 0xff - sa;
					dst->r = Lut[(dst->r * soma) + (src->r * sa)];
					dst->g = Lut[(dst->g * soma) + (src->g * sa)];
					dst->b = Lut[(dst->b * soma) + (src->b * sa)];
					dst++;
					src++;
				}
				
				u8 += Dest->Line;
			}
		}
		else if (a > 0)
		{
			// Apply source alpha AND our local alpha
			for (int y=0; y<Src->y; y++)
			{
				Pixel *dst = p;
				Pixel *dst_end = dst + Src->x;
				SrcPx *src = (SrcPx*)(Src->Base + (y * Src->Line));
				while (dst < dst_end)
				{
					register uint8 sa = Lut[a * src->a];
					register uint8 soma = 0xff - sa;
					dst->r = Lut[(dst->r * soma) + (src->r * sa)];
					dst->g = Lut[(dst->g * soma) + (src->g * sa)];
					dst->b = Lut[(dst->b * soma) + (src->b * sa)];
					dst++;
					src++;
				}
				
				u8 += Dest->Line;
			}
		}
	}

	bool Blt(GBmpMem *Src, GPalette *SPal, GBmpMem *SrcAlpha = 0)
	{
		if (!Src)
			return false;

		if (SrcAlpha)
		{
			LgiAssert(!"Impl me.");
		}
		else
		{
			switch (Src->Cs)
			{
				#define Blt24Case(name, size) \
					case Cs##name: \
						CompositeBlt##size<G##name>(Src); \
						break
				
				Blt24Case(Rgb24, 24);
				Blt24Case(Bgr24, 24);
				Blt24Case(Rgbx32, 24);
				Blt24Case(Bgrx32, 24);
				Blt24Case(Xrgb32, 24);
				Blt24Case(Xbgr32, 24);

				Blt24Case(Rgba32, 32);
				Blt24Case(Bgra32, 32);
				Blt24Case(Argb32, 32);
				Blt24Case(Abgr32, 32);
			}
		}
		
		return false;
	}
};

template<typename Pixel, GColourSpace ColourSpace>
class GdcAlpha32 : public GdcAlpha<Pixel, ColourSpace>
{
public:
	#define InitComposite32() \
		uchar *DivLut = Div255Lut; \
		register int a = DivLut[alpha * p32.a]; \
		register int r = p32.r * a; \
		register int g = p32.g * a; \
		register int b = p32.b * a; \
		register uint8 oma = 0xff - a
	#define InitFlat32() \
		Pixel px; \
		px.r = p32.r; \
		px.g = p32.g; \
		px.b = p32.b; \
		px.a = p32.a
	#define Composite32(ptr) \
		ptr->r = DivLut[(oma * ptr->r) + r]; \
		ptr->g = DivLut[(oma * ptr->g) + g]; \
		ptr->b = DivLut[(oma * ptr->b) + b]; \
		ptr->a = (a + ptr->a) - DivLut[a * ptr->a]

	const char *GetClass() { return "GdcAlpha32"; }

	void Set()
	{
		InitComposite32();
		Composite32(p);
	}
	
	void VLine(int height)
	{
		int sa = Div255Lut[alpha * p32.a];
		if (sa == 0xff)
		{
			InitFlat32();
			while (height-- > 0)
			{
				*p = px;
				u8 += Dest->Line;
			}
		}
		else if (sa > 0)
		{
			InitComposite32();
			while (height-- > 0)
			{
				Composite32(p);
				u8 += Dest->Line;
			}
		}
	}
	
	void Rectangle(int x, int y)
	{
		int sa = Div255Lut[alpha * p32.a];
		if (sa == 0xff)
		{
			// Fully opaque
			InitFlat32();
			while (y--)
			{
				Pixel *d = p;
				Pixel *e = d + x;
				while (d < e)
				{
					*d++ = px;
				}

				u8 += Dest->Line;
			}
		}
		else if (sa > 0)
		{
			// Translucent
			InitComposite32();
			while (y--)
			{
				Pixel *d = p;
				Pixel *e = d + x;
				while (d < e)
				{
					Composite32(d);
					d++;
				}

				u8 += Dest->Line;
			}
		}
	}
	
	bool Blt(GBmpMem *Src, GPalette *SPal, GBmpMem *SrcAlpha = 0)
	{
		if (!Src) return 0;
		register uchar *DivLut = Div255Lut;
		uchar lookup[256];
		register uint8 a = alpha;
		register uint8 oma = one_minus_alpha;
		for (int i=0; i<256; i++)
		{
			lookup[i] = DivLut[i * alpha];
		}

		if (SrcAlpha)
		{
			switch (Src->Cs)
			{
				default:
				{
					LgiAssert(!"Not impl.");
					break;
				}
				case CsIndex8:
				{
					System24BitPixel c[256];
					CreatePaletteLut(c, SPal);

					for (int y=0; y<Src->y; y++)
					{
						uchar *s = (uchar*) (Src->Base + (y * Src->Line));
						uchar *sa = (uchar*) (SrcAlpha->Base + (y * SrcAlpha->Line));
						System24BitPixel *sc;
						Pixel *d = p;
						uchar a, o;

						for (int x=0; x<Src->x; x++)
						{
							a = lookup[*sa++];
							if (a == 255)
							{
								sc = c + *s;
								d->r = sc->r;
								d->g = sc->g;
								d->b = sc->b;
								d->a = 255;
							}
							else if (a)
							{
								sc = c + *s;
								o = 0xff - a;
								d->r = DivLut[(d->r * o) + (sc->r * a)];
								d->g = DivLut[(d->g * o) + (sc->g * a)];
								d->b = DivLut[(d->b * o) + (sc->b * a)];
								d->a = (a + d->a) - DivLut[a * d->a];
							}

							s++;
							d++;
						}

						u8 += Dest->Line;
					}
					break;
				}
				case System15BitColourSpace:
				{
					for (int y=0; y<Src->y; y++)
					{
						ushort *s = (ushort*) (Src->Base + (y * Src->Line));
						uchar *sa = SrcAlpha->Base + (y * SrcAlpha->Line);
						Pixel *d = p;

						for (int x=0; x<Src->x; x++)
						{
							uchar a = lookup[*sa++];
							if (a == 255)
							{
								d->r = Rc15(*s);
								d->g = Gc15(*s);
								d->b = Bc15(*s);
							}
							else if (a)
							{
								uchar o = 255 - a;
								d->r = DivLut[(a * Rc15(*s)) + (o * d->r)];
								d->g = DivLut[(a * Gc15(*s)) + (o * d->g)];
								d->b = DivLut[(a * Bc15(*s)) + (o * d->b)];
							}

							s++;
							d++;
						}

						u8 += Dest->Line;
					}
					break;
				}
				case System16BitColourSpace:
				{
					for (int y=0; y<Src->y; y++)
					{
						ushort *s = (ushort*) (Src->Base + (y * Src->Line));
						uchar *sa = SrcAlpha->Base + (y * SrcAlpha->Line);
						Pixel *d = p;

						for (int x=0; x<Src->x; x++)
						{
							uchar a = lookup[*sa++];
							if (a == 255)
							{
								d->r = Rc16(*s);
								d->g = Gc16(*s);
								d->b = Bc16(*s);
							}
							else if (a)
							{
								uchar o = 255 - a;
								d->r = DivLut[(a * Rc16(*s)) + (o * d->r)];
								d->g = DivLut[(a * Gc16(*s)) + (o * d->g)];
								d->b = DivLut[(a * Bc16(*s)) + (o * d->b)];
							}

							s++;
							d++;
						}

						u8 += Dest->Line;
					}
					break;
				}
				case System24BitColourSpace:
				{
					for (int y=0; y<Src->y; y++)
					{
						uchar *sa = SrcAlpha->Base + (y * SrcAlpha->Line);
						Pixel *d = p;
						System24BitPixel *s = (System24BitPixel*) (Src->Base + (y * Src->Line));

						for (int x=0; x<Src->x; x++)
						{
							uchar a = lookup[*sa++];
							if (a == 255)
							{
								d->r = s->r;
								d->g = s->g;
								d->b = s->b;
							}
							else if (a)
							{
								uchar o = 255 - a;
								d->r = DivLut[(a * s->r) + (o * d->r)];
								d->g = DivLut[(a * s->g) + (o * d->g)];
								d->b = DivLut[(a * s->b) + (o * d->b)];
							}

							d++;
							s++;
						}

						u8 += Dest->Line;
					}
					break;
				}
				case System32BitColourSpace:
				{
					for (int y=0; y<Src->y; y++)
					{
						Pixel *d = p;
						Pixel *s = (Pixel*) (Src->Base + (y * Src->Line));
						uchar *sa = SrcAlpha->Base + (y * SrcAlpha->Line);

						for (int x=0; x<Src->x; x++)
						{
							uchar a = lookup[*sa++];
							if (a == 255)
							{
								d->r = s->r;
								d->g = s->g;
								d->b = s->b;
								d->a = 255;
							}
							else if (a)
							{
								uchar o = 255 - a;
								d->r = DivLut[(a * s->r) + (o * d->r)];
								d->g = DivLut[(a * s->g) + (o * d->g)];
								d->b = DivLut[(a * s->b) + (o * d->b)];
								d->a = (s->a + d->a) - DivLut[s->a * d->a];
							}

							d++;
							s++;
						}

						u8 += Dest->Line;
					}
					break;
				}
			}
		}
		else
		{
			switch (Src->Cs)
			{
				default:
				{
					GBmpMem Dst;
					Dst.Base = u8;
					Dst.x = Src->x;
					Dst.y = Src->y;
					Dst.Cs = Dest->Cs;
					Dst.Line = Dest->Line;				
					if (!LgiRopUniversal(&Dst, Src, true))
					{
						return false;
					}
					break;
				}
				case CsIndex8:
				{
					System24BitPixel c[256];
					CreatePaletteLut(c, SPal, alpha);

					for (int y=0; y<Src->y; y++)
					{
						uchar *s = (uchar*) (Src->Base + (y * Src->Line));
						System24BitPixel *sc;
						Pixel *d = p;

						if (alpha == 255)
						{
							for (int x=0; x<Src->x; x++)
							{
								sc = c + *s++;
								
								d->r = sc->r;
								d->g = sc->g;
								d->b = sc->b;
								d->a = 255;

								d++;
							}
						}
						else if (alpha)
						{
							for (int x=0; x<Src->x; x++)
							{
								sc = c + *s++;

								d->r = sc->r + DivLut[d->r * oma];
								d->g = sc->g + DivLut[d->g * oma];
								d->b = sc->b + DivLut[d->b * oma];
								d->a = (a + d->a) - DivLut[a * d->a];

								d++;
							}
						}

						u8 += Dest->Line;
					}
					break;
				}
				case CsRgb15:
				{
					for (int y=0; y<Src->y; y++)
					{
						ushort *s = (ushort*) (Src->Base + (y * Src->Line));
						ushort *e = s + Src->x;
						Pixel *d = p;

						while (s < e)
						{
							d->r = DivLut[(oma * d->r) + (a * Rc15(*s))];
							d->g = DivLut[(oma * d->g) + (a * Gc15(*s))];
							d->b = DivLut[(oma * d->b) + (a * Bc15(*s))];
							
							s++;
							d++;
						}

						u8 += Dest->Line;
					}
					break;
				}
				case CsRgb16:
				{
					for (int y=0; y<Src->y; y++)
					{
						ushort *s = (ushort*) (Src->Base + (y * Src->Line));
						Pixel *d = p;

						if (a == 255)
						{
							for (int x=0; x<Src->x; x++)
							{
								d->r = Rc16(*s);
								d->g = Gc16(*s);
								d->b = Bc16(*s);
								d->a = 255;

								d++;
								s++;
							}
						}
						else if (a)
						{
							for (int x=0; x<Src->x; x++)
							{
								d->r = DivLut[(d->r * oma) + (Rc16(*s) * a)];
								d->g = DivLut[(d->g * oma) + (Gc16(*s) * a)];
								d->b = DivLut[(d->b * oma) + (Bc16(*s) * a)];
								d->a = (a + d->a) - DivLut[a * d->a];

								d++;
								s++;
							}
						}

						u8 += Dest->Line;
					}
					break;
				}
				case CsBgr16:
				{
					register uint8 a = alpha;
					register uint8 oma = one_minus_alpha;

					for (int y=0; y<Src->y; y++)
					{
						GBgr16 *s = (GBgr16*) (Src->Base + (y * Src->Line));
						Pixel *d = p;

						#define Comp5BitTo8Bit(c) \
							(((uint8)(c) << 3) | ((c) >> 2))
						#define Comp6BitTo8Bit(c) \
							(((uint8)(c) << 2) | ((c) >> 4))

						if (a == 255)
						{
							for (int x=0; x<Src->x; x++)
							{
								d->r = Comp5BitTo8Bit(s->r);
								d->g = Comp6BitTo8Bit(s->g);
								d->b = Comp5BitTo8Bit(s->b);
								d->a = 255;

								d++;
								s++;
							}
						}
						else if (a)
						{
							for (int x=0; x<Src->x; x++)
							{
								d->r = DivLut[(d->r * oma) + (Comp5BitTo8Bit(s->r) * a)];
								d->g = DivLut[(d->g * oma) + (Comp6BitTo8Bit(s->g) * a)];
								d->b = DivLut[(d->b * oma) + (Comp5BitTo8Bit(s->b) * a)];
								d->a = (a + d->a) - DivLut[a * d->a];

								d++;
								s++;
							}
						}

						u8 += Dest->Line;
					}
					break;
				}
				case CsBgra64:
				{
					for (int y=0; y<Src->y; y++)
					{
						GBgra64 *s = (GBgra64*) (Src->Base + (y * Src->Line));
						Pixel *d = p;

						if (a == 255)
						{
							// 32bit alpha channel blt
							GBgra64 *end = s + Src->x;
							while (s < end)
							{
								if (s->a == 0xffff)
								{
									d->r = s->r >> 8;
									d->g = s->g >> 8;
									d->b = s->b >> 8;
									d->a = s->a >> 8;
								}
								else if (s->a)
								{
									uint8 o = (0xffff - s->a) >> 8;
									uint8 dc, sc;
									
									Rgb16to8PreMul(r);
									Rgb16to8PreMul(g);
									Rgb16to8PreMul(b);
								}

								d++;
								s++;
							}
						}
						else if (alpha)
						{
							// Const alpha + 32bit alpha channel blt
							LgiAssert(0);
							/*
							for (int x=0; x<Src->x; x++)
							{
								uchar a = lookup[s->a];
								uchar o = 255 - a;
								d->r = lookup[s->r] + DivLut[d->r * o];
								d->g = lookup[s->g] + DivLut[d->g * o];
								d->b = lookup[s->b] + DivLut[d->b * o];
								d->a = (d->a + a) - DivLut[d->a * a];
								d++;
								s++;
							}
							*/
						}

						u8 += Dest->Line;
					}
					break;
				}
			}
		}

		return true;
	}
};

GApplicator *GAlphaFactory::Create(GColourSpace Cs, int Op)
{
	if (Op != GDC_ALPHA)
		return NULL;

	switch (Cs)
	{
		#define Case(name, px) \
			case Cs##name: \
				return new GdcAlpha##px<G##name, Cs##name>()
		Case(Rgb24, 24);
		Case(Bgr24, 24);
		Case(Rgbx32, 24);
		Case(Bgrx32, 24);
		Case(Xrgb32, 24);
		Case(Xbgr32, 24);
		Case(Rgba32, 32);
		Case(Bgra32, 32);
		Case(Argb32, 32);
		Case(Abgr32, 32);
		#undef Case
		case CsIndex8:
			return new GdcApp8Alpha;
		case CsRgb15:
			return new GdcApp15Alpha;
		case CsRgb16:
		case CsBgr16:
			return new GdcApp16Alpha;
		default:
			LgiTrace("%s:%i - Unknown colour space: 0x%x %s\n",
					_FL,
					Cs,
					GColourSpaceToString(Cs));
			// LgiAssert(0);
			break;
	}

	return 0;
}

GdcApp8Alpha::GdcApp8Alpha()
{
	Bits = 8;
	Bytes = 1;
	Op = GDC_ALPHA;
	ZeroObj(Remap);
	DivLut = Div255Lut;
}

int GdcApp8Alpha::SetVar(int Var, NativeInt Value)
{
	int Status = GAlphaApp::SetVar(Var, Value);

	switch (Var)
	{
		case GAPP_ALPHA_PAL:
		{
			GPalette *Pal = (GPalette*)Value;

			if (Pal && alpha < 255)
			{
				GdcRGB *p = (*Pal)[0];
				GdcRGB *Col = (*Pal)[c&0xFF];

				for (int i=0; i<Pal->GetSize(); i++)
				{
					COLOUR Rgb = Rgb24(	Div255((oma * p[i].r) + (alpha * Col->r)),
										Div255((oma * p[i].g) + (alpha * Col->g)),
										Div255((oma * p[i].b) + (alpha * Col->b)));

					Remap[i] = Pal->MatchRgb(Rgb);
				}
			}
			else
			{
				for (int i=0; i<256; i++)
				{
					Remap[i] = c;
				}
			}
			break;
		}
	}

	return Status;
}

void GdcApp8Alpha::Set()
{
	*Ptr = Remap[*Ptr];
	if (APtr)
	{
		*APtr += DivLut[(255 - *APtr) * alpha];
	}
}

void GdcApp8Alpha::VLine(int y)
{
	while (y--)
	{
		*Ptr = Remap[*Ptr];
		Ptr += Dest->Line;

		if (APtr)
		{
			*APtr += DivLut[(255 - *APtr) * alpha];
			APtr += Alpha->Line;
		}
	}
}

void GdcApp8Alpha::Rectangle(int x, int y)
{
	while (y--)
	{
		uchar *p = Ptr;
		uchar *e = Ptr + x;
		while (p < e)
		{
			*p = Remap[*p];
			p++;
		}
		Ptr += Dest->Line;

		if (APtr)
		{
			uchar *a = APtr;
			e = a + x;
			while (a < e)
			{
				*a += DivLut[(255 - *a) * alpha];
				a++;
			}
			APtr += Alpha->Line;
		}
	}
}

bool GdcApp8Alpha::Blt(GBmpMem *Src, GPalette *SPal, GBmpMem *SrcAlpha)
{
	if (!Src)
		return false;
	if (!SPal)
		SPal = Pal;

	GPalette Grey;
	GPalette *DPal;
	if (Pal)
	{
		DPal = Pal;
	}
	else
	{
		Grey.CreateGreyScale();
		DPal = &Grey;
	}

	uchar *DivLut = Div255Lut;
	uchar *Lut = 0;

	uchar lookup[256];
	for (int i=0; i<256; i++)
	{
		lookup[i] = (i * (int)alpha) / 255;
	}

	if (SrcAlpha)
	{
		// Per pixel source alpha
		GdcRGB *SRgb = (*SPal)[0];
		GdcRGB *DRgb = (*DPal)[0];
		if (!SRgb || !DRgb) return false;

		switch (Src->Cs)
		{
			default:
			{
				LgiAssert(!"Not impl.");
				break;
			}
			case CsIndex8:
			{
				System24BitPixel sc[256];
				CreatePaletteLut(sc, SPal);
				
				System24BitPixel dc[256];
				CreatePaletteLut(dc, DPal);

				for (int y=0; y<Src->y; y++)
				{
					uchar *s = Src->Base + (y * Src->Line);
					uchar *sa = SrcAlpha->Base + (y * SrcAlpha->Line);
					uchar *d = Ptr;
					int r, g, b;

					Lut = DPal->MakeLut(15);

					for (int x=0; x<Src->x; x++)
					{
						uchar a = lookup[*sa];
						System24BitPixel *src = sc + *s;

						if (a == 255)
						{
							r = src->r;
							g = src->g;
							b = src->b;
						}
						else if (a)
						{
							uchar o = 0xff - a;
							System24BitPixel *dst = dc + *d;
							r = DivLut[(dst->r * o) + (src->r * a)];
							g = DivLut[(dst->g * o) + (src->g * a)];
							b = DivLut[(dst->b * o) + (src->b * a)];
						}

						*d++ = Lut[Rgb15(r, g, b)];
						sa++;
						s++;
					}

					Ptr += Dest->Line;
				}
				break;
			}
			case System15BitColourSpace:
			{
				System24BitPixel dc[256];
				CreatePaletteLut(dc, DPal);
				if (!Lut) Lut = DPal->MakeLut(15);

				for (int y=0; y<Src->y; y++)
				{
					ushort *s = (ushort*) (Src->Base + (y * Src->Line));
					uchar *sa = SrcAlpha->Base + (y * SrcAlpha->Line);
					uchar *d = Ptr;
					uchar *end = d + Src->x;

					while (d < end)
					{
						uchar a = lookup[*sa++];
						if (a == 255)
						{
							*d = Lut[*s];
						}
						else if (a)
						{
							uchar o = 255 - a;
							System24BitPixel *dst = dc + *d;
							int r = DivLut[(dst->r * o) + (Rc15(*s) * a)];
							int g = DivLut[(dst->g * o) + (Gc15(*s) * a)];
							int b = DivLut[(dst->b * o) + (Bc15(*s) * a)];
							*d = Lut[Rgb15(r, g, b)];
						}
						
						d++;
						s++;
					}

					Ptr += Dest->Line;
				}
				break;
			}
			case System16BitColourSpace:
			{
				System24BitPixel dc[256];
				CreatePaletteLut(dc, DPal);
				if (!Lut) Lut = DPal->MakeLut(15);

				for (int y=0; y<Src->y; y++)
				{
					ushort *s = (ushort*) (Src->Base + (y * Src->Line));
					uchar *sa = SrcAlpha->Base + (y * SrcAlpha->Line);
					uchar *d = Ptr;
					uchar *end = d + Src->x;

					while (d < end)
					{
						uchar a = lookup[*sa++];
						if (a == 255)
						{
							*d = Lut[Rgb16To15(*s)];
						}
						else if (a)
						{
							uchar o = 255 - a;
							System24BitPixel *dst = dc + *d;
							int r = DivLut[(dst->r * o) + (Rc16(*s) * a)];
							int g = DivLut[(dst->g * o) + (Gc16(*s) * a)];
							int b = DivLut[(dst->b * o) + (Bc16(*s) * a)];
							*d = Lut[Rgb15(r, g, b)];
						}
						
						d++;
						s++;
					}

					Ptr += Dest->Line;
				}
				break;
			}
			case CsBgr24:
			{
				GBgr24 dc[256];
				CreatePaletteLut(dc, DPal, 255);
				if (!Lut) Lut = DPal->MakeLut(15);

				for (int y=0; y<Src->y; y++)
				{
					GBgr24 *s = (GBgr24*) (Src->Base + (y * Src->Line));
					uchar *sa = SrcAlpha->Base + (y * SrcAlpha->Line);
					uchar *d = Ptr;

					for (int x=0; x<Src->x; x++)
					{
						uchar a = lookup[*sa++];
						if (a == 255)
						{
							*d = Lut[Rgb15(s->r, s->g, s->b)];
						}
						else if (a)
						{
							uchar o = 255 - a;
							GBgr24 *dst = dc + *d;
							int r = DivLut[(dst->r * o) + (s->r * a)];
							int g = DivLut[(dst->g * o) + (s->g * a)];
							int b = DivLut[(dst->b * o) + (s->b * a)];
							*d = Lut[Rgb15(r, g, b)];
						}

						d++;
						s++;
					}

					Ptr += Dest->Line;
				}
				break;
			}
			case System32BitColourSpace:
			{
				System24BitPixel dc[256];
				CreatePaletteLut(dc, DPal, 255);
				if (!Lut) Lut = DPal->MakeLut(15);

				for (int y=0; y<Src->y; y++)
				{
					System32BitPixel *s = (System32BitPixel*) (Src->Base + (y * Src->Line));
					uchar *sa = SrcAlpha->Base + (y * SrcAlpha->Line);
					uchar *d = Ptr;

					for (int x=0; x<Src->x; x++)
					{
						uchar a = lookup[*sa++];
						if (a == 255)
						{
							*d = Lut[Rgb15(s->r, s->g, s->b)];
						}
						else if (a)
						{
							uchar o = 255 - a;
							System24BitPixel *dst = dc + *d;
							int r = DivLut[(dst->r * o) + (s->r * a)];
							int g = DivLut[(dst->g * o) + (s->g * a)];
							int b = DivLut[(dst->b * o) + (s->b * a)];
							*d = Lut[Rgb15(r, g, b)];
						}

						d++;
						s++;
					}

					Ptr += Dest->Line;
				}
				break;
			}
		}
	}
	else
	{
		// Global alpha level
		GdcRGB *SRgb = (*SPal)[0];
		GdcRGB *DRgb = (*DPal)[0];
		if (!SRgb || !DRgb) return false;

		switch (Src->Cs)
		{
			default:
			{
				LgiTrace("%s:%i - Not impl.\n", _FL);
				break;
			}
			case CsIndex8:
			{
				if (alpha == 255)
				{
					// do a straight blt
					for (int y=0; y<Src->y; y++)
					{
						uchar *s = Src->Base + (y * Src->Line);
						uchar *d = Ptr;

						memcpy(d, s, Src->x);
						Ptr += Dest->Line;
					}
				}
				else if (alpha)
				{
					System24BitPixel sc[256];
					CreatePaletteLut(sc, SPal, alpha);
					
					System24BitPixel dc[256];
					CreatePaletteLut(dc, DPal, oma);

					if (!Lut) Lut = DPal->MakeLut(15);

					for (int y=0; y<Src->y; y++)
					{
						uchar *s = Src->Base + (y * Src->Line);
						uchar *d = Ptr;

						for (int x=0; x<Src->x; x++, s++, d++)
						{
							System24BitPixel *src = sc + *s;
							System24BitPixel *dst = dc + *d;
							int r = src->r + dst->r;
							int g = src->g + dst->g;
							int b = src->b + dst->b;
							*d = Lut[Rgb15(r, g, b)];
						}

						Ptr += Dest->Line;
					}
				}
				break;
			}
			case System15BitColourSpace:
			{
				System24BitPixel dc[256];
				CreatePaletteLut(dc, DPal, oma);
				
				if (!Lut) Lut = DPal->MakeLut(15);

				for (int y=0; y<Src->y; y++)
				{
					ushort *s = (ushort*) (Src->Base + (y * Src->Line));
					uchar *d = Ptr;
					uchar *e = d + Src->x;

					while (d < e)
					{
						System24BitPixel *dst = dc + *d;
						int r = dst->r + DivLut[Rc15(*s) * alpha];
						int g = dst->g + DivLut[Gc15(*s) * alpha];
						int b = dst->b + DivLut[Bc15(*s) * alpha];
						*d++ = Lut[Rgb15(r, g, b)];
						s++;						
					}

					Ptr += Dest->Line;
				}
				break;
			}
			case System16BitColourSpace:
			{
				System24BitPixel dc[256];
				CreatePaletteLut(dc, DPal, oma);
				
				if (!Lut) Lut = DPal->MakeLut(15);

				for (int y=0; y<Src->y; y++)
				{
					ushort *s = (ushort*) (Src->Base + (y * Src->Line));
					uchar *d = Ptr;

					for (int x=0; x<Src->x; x++, s++, d++)
					{
						System24BitPixel *dst = dc + *d;
						int r = dst->r + DivLut[Rc16(*s) * alpha];
						int g = dst->g + DivLut[Gc16(*s) * alpha];
						int b = dst->b + DivLut[Bc16(*s) * alpha];
						*d = Lut[Rgb15(r, g, b)];
					}

					Ptr += Dest->Line;
				}
				break;
			}
			case CsBgr24:
			{
				GBgr24 dc[256];
				CreatePaletteLut(dc, DPal, oma);

				if (!Lut) Lut = DPal->MakeLut(15);

				for (int y=0; y<Src->y; y++)
				{
					GBgr24 *s = (GBgr24*) (Src->Base + (y * Src->Line));
					uchar *d = Ptr;
					uchar *e = Ptr + Src->x;

					while (d < e)
					{
						GBgr24 *dst = dc + *d;
						int r = dst->r + DivLut[s->r * alpha];
						int g = dst->g + DivLut[s->g * alpha];
						int b = dst->b + DivLut[s->b * alpha];

						s++;
						*d++ = Lut[Rgb15(r, g, b)];
					}

					Ptr += Dest->Line;
				}
				break;
			}
			case System32BitColourSpace:
			{
				System24BitPixel dc[256];
				CreatePaletteLut(dc, DPal);
				if (!Lut) Lut = DPal->MakeLut(15);

				for (int y=0; y<Src->y; y++)
				{
					System32BitPixel *s = (System32BitPixel*) (Src->Base + (y * Src->Line));
					uchar *d = Ptr;
					uchar *end = d + Src->x;

					while (d < end)
					{
						uchar a = lookup[s->a];
						if (a)
						{
							System24BitPixel *dst = dc + *d;
							uchar o = 255 - a;
							int r = lookup[s->r] + DivLut[dst->r * o];
							int g = lookup[s->g] + DivLut[dst->g * o];
							int b = lookup[s->b] + DivLut[dst->b * o];
							*d = Lut[Rgb15(r, g, b)];
						}

						s++;
						d++;
					}

					Ptr += Dest->Line;
				}
				break;
			}
		}
	}

	return false;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
#define Setup15() \
	uchar *DivLut = Div255Lut; \
	int r = DivLut[Rc15(c)]; \
	int g = DivLut[Gc15(c)]; \
	int b = DivLut[Bc15(c)];

#define Comp15() \
	*p = Rgb15(	DivLut[r + (oma * Rc15(*p))], \
				DivLut[g + (oma * Gc15(*p))], \
				DivLut[b + (oma * Bc15(*p))] );

void GdcApp15Alpha::Set()
{
	ushort *p = (ushort*) Ptr;
	Setup15();
	Comp15();
}

void GdcApp15Alpha::VLine(int height)
{
	ushort *p = (ushort*) Ptr;
	Setup15();

	while (height--)
	{
		Comp15();
		p = (ushort*)(Ptr += Dest->Line);
	}
}

void GdcApp15Alpha::Rectangle(int x, int y)
{
	ushort *p = (ushort*) Ptr;
	Setup15();

	while (y--)
	{
		for (int n=0; n<x; n++, p++)
		{
			Comp15();
		}

		p = (ushort*)(Ptr += Dest->Line);
	}
}

bool GdcApp15Alpha::Blt(GBmpMem *Src, GPalette *SPal, GBmpMem *SrcAlpha)
{
	if (!Src) return 0;
	uchar *DivLut = Div255Lut;

	if (SrcAlpha)
	{
		uchar lookup[256];
		for (int i=0; i<256; i++)
		{
			lookup[i] = (i * (int)alpha) / 255;
		}

		switch (Src->Cs)
		{
			default:
			{
				LgiAssert(!"Not impl.");
				break;
			}
			case CsIndex8:
			{
				System24BitPixel c[256];
				CreatePaletteLut(c, SPal, 255);
				
				for (int y=0; y<Src->y; y++)
				{
					uchar *s = Src->Base + (y * Src->Line);
					uchar *sa = SrcAlpha->Base + (y * SrcAlpha->Line);
					ushort *d = (ushort*) Ptr;
					ushort *e = d + Src->x;

					while (d < e)
					{
						uchar a = lookup[*sa++];
						if (a == 255)
						{
							System24BitPixel *src = c + *s;
							*d = Rgb15(src->r, src->g, src->b);
						}
						else if (a)
						{
							System24BitPixel *src = c + *s;
							uchar o = 255 - a;
							*d = Rgb15(	DivLut[(o * Rc15(*d)) + (a * src->r)],
										DivLut[(o * Gc15(*d)) + (a * src->g)],
										DivLut[(o * Bc15(*d)) + (a * src->b)]);
						}
						
						d++;
						s++;
					}

					Ptr += Dest->Line;
				}
				break;
			}
			case CsRgb15:
			{
				for (int y=0; y<Src->y; y++)
				{
					ushort *s = (ushort*) (Src->Base + (y * Src->Line));
					uchar *src_a = SrcAlpha->Base + (y * SrcAlpha->Line);
					ushort *d = (ushort*) Ptr;
					ushort *e = d + Src->x;

					while (d < e)
					{
						uchar a = lookup[*src_a++];
						uchar oma = 255 - a;

						if (a == 255)
						{
							*d = *s;
						}
						else if (a)
						{
							*d = Rgb15
							(
								DivLut[(oma * Rc15(*d)) + (a * Rc15(*s))],
								DivLut[(oma * Gc15(*d)) + (a * Gc15(*s))],
								DivLut[(oma * Bc15(*d)) + (a * Bc15(*s))]
							);
						}

						d++;
						s++;
					}

					Ptr += Dest->Line;
				}
				break;
			}
			case CsRgb16:
			{
				for (int y=0; y<Src->y; y++)
				{
					ushort *s = (ushort*) (Src->Base + (y * Src->Line));
					uchar *src_a = SrcAlpha->Base + (y * SrcAlpha->Line);
					ushort *d = (ushort*) Ptr;

					for (int x=0; x<Src->x; x++)
					{
						uchar my_a = lookup[*src_a++];
						uchar my_oma = 255 - my_a;

						if (my_oma == 0)
						{
							*d = *s;
						}
						else if (my_oma < 255)
						{
							*d = Rgb15(	Div255((my_oma * Rc15(*d)) + (my_a * Rc16(*s))),
										Div255((my_oma * Gc15(*d)) + (my_a * Gc16(*s))),
										Div255((my_oma * Bc15(*d)) + (my_a * Bc16(*s))));
						}

						d++;
						s++;
					}

					Ptr += Dest->Line;
				}
				break;
			}
			case CsBgr24:
			{
				for (int y=0; y<Src->y; y++)
				{
					GBgr24 *s = (GBgr24*) (Src->Base + (y * Src->Line));
					uchar *sa = SrcAlpha->Base + (y * SrcAlpha->Line);
					ushort *d = (ushort*) Ptr;
					ushort *e = d + Src->x;

					while (d < e)
					{
						uchar a = lookup[*sa++];
						uchar o = 255 - a;

						if (a == 255)
						{
							*d = Rgb15(s->r, s->g, s->b);
						}
						else if (a)
						{
							*d = Rgb15(	Div255( (o * Rc15(*d)) + (a * s->r) ),
										Div255( (o * Gc15(*d)) + (a * s->g) ),
										Div255( (o * Bc15(*d)) + (a * s->b) ));
						}

						s++;
						d++;
					}

					Ptr += Dest->Line;
				}
				break;
			}
			case System32BitColourSpace:
			{
				for (int y=0; y<Src->y; y++)
				{
					System32BitPixel *s = (System32BitPixel*) (Src->Base + (y * Src->Line));
					uchar *sa = SrcAlpha->Base + (y * SrcAlpha->Line);
					ushort *d = (ushort*) Ptr;
					ushort *e = d + Src->x;

					while (d < e)
					{
						uchar a = lookup[*sa++];
						uchar o = 255 - a;

						if (a == 255)
						{
							*d = Rgb15(s->r, s->g, s->b);
						}
						else if (a)
						{
							*d = Rgb15(	Div255( (o * Rc15(*d)) + (a * s->r) ),
										Div255( (o * Gc15(*d)) + (a * s->g) ),
										Div255( (o * Bc15(*d)) + (a * s->b) ));
						}

						d++;
						s++;
					}

					Ptr += Dest->Line;
				}
				break;
			}
		}
	}
	else
	{
		switch (Src->Cs)
		{
			default:
			{
				LgiTrace("%s:%i - Not impl.\n", _FL);
				break;
			}
			case CsIndex8:
			{
				System24BitPixel c[256];
				if (alpha) CreatePaletteLut(c, SPal, alpha);
				
				for (int y=0; y<Src->y; y++)
				{
					ushort *d = (ushort*) Ptr;
					ushort *e = d + Src->x;
					uchar *s = Src->Base + (Src->Line * y);

					System24BitPixel *Src; 
					if (alpha == 255)
					{
						// copy
						while (d < e)
						{
							Src = c + *s++;
							*d++ = Rgb15(Src->r, Src->g, Src->b);
						}
					}
					else if (alpha)
					{
						// blend
						while (d < e)
						{
							Src = c + *s++;
							*d = Rgb15
							(
								DivLut[oma * Rc15(*d)] + Src->r,
								DivLut[oma * Gc15(*d)] + Src->g,
								DivLut[oma * Bc15(*d)] + Src->b
							);
							d++;
						}
					}

					Ptr += Dest->Line;
				}

				break;
			}
			case CsRgb15:
			{
				for (int y=0; y<Src->y; y++)
				{
					ushort *s = (ushort*) (Src->Base + (y * Src->Line));
					ushort *d = (ushort*) Ptr;
					ushort *e = d + Src->x;

					if (alpha == 255)
					{
						// copy
						while (d < e)
						{
							*d++ = *s++;
						}
					}
					else if (alpha)
					{
						// blend
						while (d < e)
						{
							*d = Rgb15
							(
								DivLut[(oma * Rc15(*d)) + (alpha * Rc15(*s))],
								DivLut[(oma * Gc15(*d)) + (alpha * Gc15(*s))],
								DivLut[(oma * Bc15(*d)) + (alpha * Bc15(*s))]
							);

							s++;
							d++;
						}
					}

					Ptr += Dest->Line;
				}
				break;
			}
			case CsRgb16:
			{
				// this code combines the colour bitmap with the destination
				// bitmap using the given alpha value
				for (int y=0; y<Src->y; y++)
				{
					ushort *s = (ushort*) (Src->Base + (y * Src->Line));
					ushort *d = (ushort*) Ptr;

					for (int x=0; x<Src->x; x++, d++, s++)
					{
						*d = Rgb15(	Div255((oma * Rc15(*d)) + (alpha * Rc16(*s))),
									Div255((oma * Gc15(*d)) + (alpha * Gc16(*s))),
									Div255((oma * Bc15(*d)) + (alpha * Bc16(*s))));
					}

					Ptr += Dest->Line;
				}
				break;
			}
			case CsBgr24:
			{
				for (int y=0; y<Src->y; y++)
				{
					GBgr24 *s = (GBgr24*) (Src->Base + (y * Src->Line));
					ushort *d = (ushort*) Ptr;
					ushort *e = d + Src->x;

					while (d < e)
					{
						*d = Rgb15
						(
							DivLut[(oma * Rc15(*d)) + (alpha * s->r)],
							DivLut[(oma * Gc15(*d)) + (alpha * s->g)],
							DivLut[(oma * Bc15(*d)) + (alpha * s->b)]
						);
						
						s++;
						d++;
					}

					Ptr += Dest->Line;
				}
				break;
			}
			case System32BitColourSpace:
			{
				for (int y=0; y<Src->y; y++)
				{
					System32BitPixel *s = (System32BitPixel*) (Src->Base + (y * Src->Line));
					ushort *d = (ushort*) Ptr;
					ushort *e = d + Src->x;

					while (d < e)
					{
						*d = Rgb15
						(
							DivLut[(oma * Rc15(*d)) + (alpha * s->r)],
							DivLut[(oma * Gc15(*d)) + (alpha * s->g)],
							DivLut[(oma * Bc15(*d)) + (alpha * s->b)]
						);
						
						s++;
						d++;
					}

					Ptr += Dest->Line;
				}
				break;
			}
		}
	}

	return false;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
#define Setup16()	\
	uchar *DivLut = Div255Lut; \
	int r = Rc16(c) * alpha; \
	int g = Gc16(c) * alpha; \
	int b = Bc16(c) * alpha;

#define Comp16() \
	*p = Rgb16(	DivLut[r + (oma * Rc16(*p))], \
				DivLut[g + (oma * Gc16(*p))], \
				DivLut[b + (oma * Bc16(*p))]);

void GdcApp16Alpha::Set()
{
	ushort *p = (ushort*) Ptr;
	Setup16();
	Comp16();
}

void GdcApp16Alpha::VLine(int height)
{
	ushort *p = (ushort*) Ptr;
	Setup16();

	while (height--)
	{
		Comp16();
		p = (ushort*) (Ptr += Dest->Line);
	}
}

void GdcApp16Alpha::Rectangle(int x, int y)
{
	ushort *p = (ushort*) Ptr;
	Setup16();

	while (y--)
	{
		for (int n=0; n<x; n++, p++)
		{
			Comp16();
		}

		p = (ushort*) (Ptr += Dest->Line);
	}
}

bool GdcApp16Alpha::Blt(GBmpMem *Src, GPalette *SPal, GBmpMem *SrcAlpha)
{
	if (!Src) return 0;
	uchar *DivLut = Div255Lut;
	uchar lookup[256];
	for (int i=0; i<256; i++)
	{
		lookup[i] = (i * (int)alpha) / 255;
	}

	if (SrcAlpha)
	{
		switch (Src->Cs)
		{
			default:
			{
				LgiAssert(!"Not impl.");
				break;
			}
			case CsIndex8:
			{
				System24BitPixel c[256];
				CreatePaletteLut(c, SPal, 255);

				for (int y=0; y<Src->y; y++)
				{
					uchar *s = Src->Base + (y * Src->Line);
					uchar *sa = SrcAlpha->Base + (y * SrcAlpha->Line);
					ushort *d = (ushort*) Ptr;
					ushort *EndD = d + Src->x;

					while (d < EndD)
					{
						uchar a = lookup[*sa++];

						if (a == 255)
						{
							System24BitPixel *src = c + *s;
							*d = Rgb16(src->r, src->g, src->b);
						}
						else if (a)
						{
							System24BitPixel *src = c + *s;
							uchar o = 255 - a;
							int r = DivLut[(Rc16(*d) * o) + (src->r * a)];
							int g = DivLut[(Gc16(*d) * o) + (src->g * a)];
							int b = DivLut[(Bc16(*d) * o) + (src->b * a)];
							*d = Rgb16(r, g, b);
						}

						d++;
						s++;
					}

					Ptr += Dest->Line;
				}
				break;
			}
			case System15BitColourSpace:
			{
				for (int y=0; y<Src->y; y++)
				{
					ushort *s = (ushort*) (Src->Base + (y * Src->Line));
					uchar *sa = SrcAlpha->Base + (y * SrcAlpha->Line);
					ushort *d = (ushort*) Ptr;
					ushort *e = d + Src->x;

					while (d < e)
					{
						uchar a = lookup[*sa++];
						uchar o = 255 - a;

						if (a == 255)
						{
							*d = Rgb15To16(*s);
						}
						else if (a)
						{
							*d = Rgb16(	Div255((o * Rc16(*d)) + (a * Rc15(*s))),
										Div255((o * Gc16(*d)) + (a * Gc15(*s))),
										Div255((o * Bc16(*d)) + (a * Bc15(*s))));
						}

						d++;
						s++;
					}

					Ptr += Dest->Line;
				}
				break;
			}
			case System16BitColourSpace:
			{
				for (int y=0; y<Src->y; y++)
				{
					ushort *s = (ushort*) ((uchar*)Src->Base + (y * Src->Line));
					uchar *src_a = SrcAlpha->Base + (y * SrcAlpha->Line);
					ushort *d = (ushort*) Ptr;

					for (int x=0; x<Src->x; x++)
					{
						uchar my_a = lookup[*src_a++];
						uchar my_oma = 255 - my_a;

						if (my_oma == 0)
						{
							*d = *s;
						}
						else if (my_oma < 255)
						{
							*d = Rgb16(	Div255((my_oma * Rc16(*d)) + (my_a * Rc16(*s))),
										Div255((my_oma * Gc16(*d)) + (my_a * Gc16(*s))),
										Div255((my_oma * Bc16(*d)) + (my_a * Bc16(*s))));
						}

						d++;
						s++;
					}

					Ptr += Dest->Line;
				}
				break;
			}
			case CsBgr24:
			#ifndef LINUX
			{
				for (int y=0; y<Src->y; y++)
				{
					GBgr24 *s = (GBgr24*) (Src->Base + (y * Src->Line));
					uchar *sa = SrcAlpha->Base + (y * SrcAlpha->Line);
					ushort *d = (ushort*) Ptr;
					ushort *end = d + Src->x;

					while (d < end)
					{
						uchar a = lookup[*sa++];
						if (a == 255)
						{
							*d = Rgb16(s->r, s->g, s->b);
						}
						else if (a)
						{
							uchar o = 255 - a;
							int r = DivLut[(Rc16(*d) * o) + (s->r * a)];
							int g = DivLut[(Gc16(*d) * o) + (s->g * a)];
							int b = DivLut[(Bc16(*d) * o) + (s->b * a)];
							*d = Rgb16(r, g, b);
						}

						s++;
						d++;
					}

					Ptr += Dest->Line;
				}
				break;
			}
			#endif
			case System32BitColourSpace:
			{
				for (int y=0; y<Src->y; y++)
				{
					System32BitPixel *s = (System32BitPixel*) (Src->Base + (y * Src->Line));
					uchar *sa = SrcAlpha->Base + (y * SrcAlpha->Line);
					ushort *d = (ushort*) Ptr;
					ushort *end = d + Src->x;

					while (d < end)
					{
						uchar a = lookup[*sa++];
						if (a == 255)
						{
							*d = Rgb16(s->r, s->g, s->b);
						}
						else if (a)
						{
							uchar o = 255 - a;
							int r = DivLut[(Rc16(*d) * o) + (s->r * a)];
							int g = DivLut[(Gc16(*d) * o) + (s->g * a)];
							int b = DivLut[(Bc16(*d) * o) + (s->b * a)];
							*d = Rgb16(r, g, b);
						}

						d++;
						s++;
					}

					Ptr += Dest->Line;
				}
				break;
			}
		}
	}
	else
	{
		switch (Src->Cs)
		{
			default:
			{
				LgiTrace("%s:%i - Not impl.\n", _FL);
				break;
			}
			case CsIndex8:
			{
				// this code uses the input bitmap as a mask to say where
				// to draw the current colour at the given alpha value
				System24BitPixel c[256];
				CreatePaletteLut(c, SPal, alpha);

				for (int y=0; y<Src->y; y++)
				{
					ushort *d = (ushort*) Ptr;
					uchar *s = Src->Base + (Src->Line * y);

					for (int x=0; x<Src->x; x++, d++, s++)
					{
						System24BitPixel *src = c + *s;
						int r = src->r + DivLut[Rc16(*d) * oma];
						int g = src->g + DivLut[Gc16(*d) * oma];
						int b = src->b + DivLut[Bc16(*d) * oma];
						*d = Rgb16(r, g, b);
					}

					Ptr += Dest->Line;
				}

				break;
			}
			case CsRgb15:
			{
				for (int y=0; y<Src->y; y++)
				{
					ushort *s = (ushort*) (Src->Base + (y * Src->Line));
					ushort *d = (ushort*) Ptr;
					ushort *e = d + Src->x;

					while (d < e)
					{
						*d = Rgb16
						(
							DivLut[(oma * Rc16(*d)) + (alpha * Rc15(*s))],
							DivLut[(oma * Gc16(*d)) + (alpha * Gc15(*s))],
							DivLut[(oma * Bc16(*d)) + (alpha * Bc15(*s))]
						);
						
						s++;
						d++;
					}

					Ptr += Dest->Line;
				}
				break;
			}
			case CsRgb16:
			case CsBgr16:
			{
				for (int y=0; y<Src->y; y++)
				{
					ushort *s = (ushort*) (Src->Base + (y * Src->Line));
					ushort *d = (ushort*) Ptr;

					for (int x=0; x<Src->x; x++, d++, s++)
					{
						*d = Rgb16(	Div255((oma * Rc16(*d)) + (alpha * Rc16(*s))),
									Div255((oma * Gc16(*d)) + (alpha * Gc16(*s))),
									Div255((oma * Bc16(*d)) + (alpha * Bc16(*s))));
					}

					Ptr += Dest->Line;
				}
				break;
			}
			case CsBgr24:
			{
				for (int y=0; y<Src->y; y++)
				{
					GBgr24 *s = (GBgr24*) (Src->Base + (y * Src->Line));
					ushort *d = (ushort*) Ptr;
					ushort *e = d + Src->x;

					while (d < e)
					{
						*d = Rgb16(	Div255((oma * Rc16(*d)) + (alpha * s->r)),
										Div255((oma * Gc16(*d)) + (alpha * s->g)),
										Div255((oma * Bc16(*d)) + (alpha * s->b)));
						d++;
						s++;
					}

					Ptr += Dest->Line;
				}
				break;
			}
			case System32BitColourSpace:
			{
				for (int y=0; y<Src->y; y++)
				{
					System32BitPixel *s = (System32BitPixel*) (Src->Base + (y * Src->Line));
					ushort *d = (ushort*) Ptr;
					ushort *end = d + Src->x;

					if (alpha == 255)
					{
						while (d < end)
						{
							if (s->a)
							{
								uchar o = 255 - s->a;
								int r = DivLut[(Rc16(*d) * o) + (s->r * s->a)];
								int g = DivLut[(Rc16(*d) * o) + (s->g * s->a)];
								int b = DivLut[(Rc16(*d) * o) + (s->b * s->a)];
								*d = Rgb16(r, g, b);
							}

							s++;
							d++;
						}
					}
					else if (alpha)
					{
						while (d < end)
						{
							uchar a = lookup[s->a];
							if (a == 255)
							{
								*d = Rgb16(s->r, s->g, s->b);
							}
							else if (a)
							{
								uchar o = 255 - a;
								int r = lookup[s->r] + DivLut[o * Rc16(*d)];
								int g = lookup[s->g] + DivLut[o * Gc16(*d)];
								int b = lookup[s->b] + DivLut[o * Bc16(*d)];
								*d = Rgb16(r, g, b);
							}

							s++;
							d++;
						}
					}

					Ptr += Dest->Line;
				}
				break;
			}
		}
	}

	return false;
}

/*
bool GdcApp24Alpha::Blt(GBmpMem *Src, GPalette *SPal, GBmpMem *SrcAlpha)
{
	if (!Src) return false;
	uchar *DivLut = Div255Lut;
	uchar lookup[256];
	for (int i=0; i<256; i++)
	{
		lookup[i] = (i * (int)alpha) / 255;
	}

	if (SrcAlpha)
	{
		switch (Src->Cs)
		{
			default:
			{
				LgiAssert(!"Not impl.");
				break;
			}
			case CsIndex8:
			{
				System24BitPixel c[256];
				CreatePaletteLut(c, SPal, 255);

				for (int y=0; y<Src->y; y++)
				{
					uchar *s = Src->Base + (y * Src->Line);
					uchar *e = s + Src->x;
					uchar *sa = SrcAlpha->Base + (y * SrcAlpha->Line);
					System24BitPixel *d = (System24BitPixel*) Ptr;

					while (s < e)
					{
						uchar a = lookup[*sa++];
						uchar o = 255 - a;

						if (a == 255)
						{
							System24BitPixel *src = c + *s;
							*d = *src;
						}
						else if (a)
						{
							System24BitPixel *src = c + *s;
							d->r = DivLut[(o * d->r) + (a * src->r)];
							d->g = DivLut[(o * d->g) + (a * src->g)];
							d->b = DivLut[(o * d->b) + (a * src->b)];
						}

						s++;
						d++;
					}

					Ptr += Dest->Line;
				}
				break;
			}
			case System15BitColourSpace:
			{
				for (int y=0; y<Src->y; y++)
				{
					ushort *s = (ushort*) (Src->Base + (y * Src->Line));
					ushort *e = s + Src->x;
					uchar *sa = SrcAlpha->Base + (y * SrcAlpha->Line);
					System24BitPixel *d = (System24BitPixel*) Ptr;

					while (s < e)
					{
						uchar a = lookup[*sa++];

						if (a == 255)
						{
							d->r = Bc15(*s);
							d->g = Gc15(*s);
							d->b = Rc15(*s);
						}
						else if (a)
						{
							uchar o = 255 - a;
							d->r = Div255((o * d->r) + (a * Rc15(*s)));
							d->g = Div255((o * d->g) + (a * Gc15(*s)));
							d->b = Div255((o * d->b) + (a * Bc15(*s)));
						}

						s++;
						d++;
					}

					Ptr += Dest->Line;
				}
				break;
			}
			case System16BitColourSpace:
			{
				for (int y=0; y<Src->y; y++)
				{
					ushort *s = (ushort*) (Src->Base + (y * Src->Line));
					ushort *e = s + Src->x;
					uchar *sa = SrcAlpha->Base + (y * SrcAlpha->Line);
					System24BitPixel *d = (System24BitPixel*) Ptr;

					while (s < e)
					{
						uchar a = lookup[*sa++];

						if (a == 255)
						{
							d->r = Bc16(*s);
							d->g = Gc16(*s);
							d->b = Rc16(*s);
						}
						else if (a)
						{
							uchar o = 255 - a;
							d->r = Div255((o * d->r) + (a * Rc16(*s)));
							d->g = Div255((o * d->g) + (a * Gc16(*s)));
							d->b = Div255((o * d->b) + (a * Bc16(*s)));
						}

						s++;
						d++;
					}

					Ptr += Dest->Line;
				}
				break;
			}
			case CsBgr24:
			{
				for (int y=0; y<Src->y; y++)
				{
					uchar *s = Src->Base + (y * Src->Line);
					uchar *src_a = SrcAlpha->Base + (y * SrcAlpha->Line);
					uchar *d = Ptr;

					for (int x=0; x<Src->x; x++)
					{
						uchar my_a = lookup[*src_a++];
						uchar my_oma = 255 - my_a;

						if (my_oma == 0)
						{
							d[0] = s[0];
							d[1] = s[1];
							d[2] = s[2];
						}
						else if (my_oma < 255)
						{
							d[0] = Div255( (my_oma * d[0]) + (my_a * s[0]) );
							d[1] = Div255( (my_oma * d[1]) + (my_a * s[1]) );
							d[2] = Div255( (my_oma * d[2]) + (my_a * s[2]) );
						}

						d += Bytes;
						s += Bytes;
					}

					Ptr += Dest->Line;
				}
				break;
			}
			case System32BitColourSpace:
			{
				for (int y=0; y<Src->y; y++)
				{
					System32BitPixel *s = (System32BitPixel*) (Src->Base + (y * Src->Line));
					System32BitPixel *e = s + Src->x;
					uchar *sa = SrcAlpha->Base + (y * SrcAlpha->Line);
					System24BitPixel *d = (System24BitPixel*) Ptr;

					while (s < e)
					{
						uchar a = lookup[*sa++];
						if (a == 255)
						{
							d->r = s->r;
							d->g = s->g;
							d->b = s->b;
						}
						else if (a)
						{
							uchar o = 255 - a;
							d->r = DivLut[(d->r * o) + (s->r * a)];
							d->g = DivLut[(d->g * o) + (s->g * a)];
							d->b = DivLut[(d->b * o) + (s->b * a)];
						}

						s++;
						d++;
					}

					Ptr += Dest->Line;
				}
				break;
			}
		}
	}
	else
	{
		switch (Src->Cs)
		{
			default:
			{
				LgiAssert(!"Not impl.");
				break;
			}
			case CsIndex8:
			{
				System24BitPixel c[256];
				CreatePaletteLut(c, SPal, alpha);

				for (int y=0; y<Src->y; y++)
				{
					uchar *s = Src->Base + (y * Src->Line);
					uchar *e = s + Src->x;
					System24BitPixel *d = (System24BitPixel*) Ptr;

					while (s < e)
					{
						System24BitPixel *src = c + *s++;
						d->r = src->r + DivLut[d->r * oma];
						d->g = src->g + DivLut[d->g * oma];
						d->b = src->b + DivLut[d->b * oma];
						d++;
					}

					Ptr += Dest->Line;
				}
				break;
			}
			case System15BitColourSpace:
			{
				for (int y=0; y<Src->y; y++)
				{
					ushort *s = (ushort*) (Src->Base + (y * Src->Line));
					ushort *e = s + Src->x;
					System24BitPixel *d = (System24BitPixel*) Ptr;

					while (s < e)
					{
						d->r = DivLut[(oma * d->r) + (alpha * Rc15(*s))];
						d->g = DivLut[(oma * d->g) + (alpha * Gc15(*s))];
						d->b = DivLut[(oma * d->b) + (alpha * Bc15(*s))];
						
						s++;
						d++;
					}

					Ptr += Dest->Line;
				}
				break;
			}
			case System16BitColourSpace:
			{
				for (int y=0; y<Src->y; y++)
				{
					ushort *s = (ushort*) (Src->Base + (y * Src->Line));
					System24BitPixel *d = (System24BitPixel*) Ptr;

					if (alpha == 255)
					{
						for (int x=0; x<Src->x; x++)
						{
							d->r = Rc16(*s);
							d->g = Gc16(*s);
							d->b = Bc16(*s);

							s++;
							d++;
						}
					}
					else if (alpha)
					{
						for (int x=0; x<Src->x; x++)
						{
							d->r = DivLut[(oma * d->r) + (alpha * Rc15(*s))];
							d->g = DivLut[(oma * d->g) + (alpha * Gc16(*s))];
							d->b = DivLut[(oma * d->b) + (alpha * Bc16(*s))];
							
							s++;
							d++;
						}
					}

					Ptr += Dest->Line;
				}
				break;
			}
			case CsBgr24:
			{
				for (int y=0; y<Src->y; y++)
				{
					GBgr24 *s = (GBgr24*) (Src->Base + (y * Src->Line));
					System24BitPixel *d = (System24BitPixel*) Ptr;

					if (alpha == 255)
					{
						memcpy(d, s, Src->x * 3);
					}
					else if (alpha)
					{
						for (int x=0; x<Src->x; x++)
						{
							d->r = DivLut[(oma * d->r) + (alpha * s->r)];
							d->g = DivLut[(oma * d->g) + (alpha * s->g)];
							d->b = DivLut[(oma * d->b) + (alpha * s->b)];

							s++;
							d++;
						}
					}

					Ptr += Dest->Line;
				}
				break;
			}
			case CsBgr48:
			{
				for (int y=0; y<Src->y; y++)
				{
					GBgr48 *s = (GBgr48*) (Src->Base + (y * Src->Line));
					System24BitPixel *d = (System24BitPixel*) Ptr;

					if (alpha == 255)
					{
						GBgr48 *e = s + Src->x;
						while (s < e)
						{
							d->r = s->r >> 8;
							d->g = s->g >> 8;
							d->b = s->b >> 8;
							d++;
							s++;
						}
					}
					else if (alpha)
					{
						for (int x=0; x<Src->x; x++)
						{
							d->r = DivLut[(oma * d->r) + (alpha * (s->r >> 8))];
							d->g = DivLut[(oma * d->g) + (alpha * (s->g >> 8))];
							d->b = DivLut[(oma * d->b) + (alpha * (s->b >> 8))];

							s++;
							d++;
						}
					}

					Ptr += Dest->Line;
				}
				break;
			}
			case System32BitColourSpace:
			{
				for (int y=0; y<Src->y; y++)
				{
					System32BitPixel *s = (System32BitPixel*) (Src->Base + (y * Src->Line));
					System24BitPixel *d = (System24BitPixel*) Ptr;

					if (alpha == 255)
					{
						for (int x=0; x<Src->x; x++)
						{
							if (s->a)
							{
								uchar o = 255 - s->a;
								d->r = DivLut[(s->r * s->a) + (d->r * o)];
								d->g = DivLut[(s->g * s->a) + (d->g * o)];
								d->b = DivLut[(s->b * s->a) + (d->b * o)];
							}

							s++;
							d++;
						}
					}
					else if (alpha)
					{
						for (int x=0; x<Src->x; x++)
						{
							uchar a = lookup[s->a];
							if (a == 255)
							{
								d->r = s->r;
								d->g = s->g;
								d->b = s->b;
							}
							else if (a)
							{
								uchar o = 255 - a;
								d->r = lookup[s->r] + DivLut[o * d->r];
								d->g = lookup[s->g] + DivLut[o * d->g];
								d->b = lookup[s->b] + DivLut[o * d->b];
							}

							s++;
							d++;
						}
					}

					Ptr += Dest->Line;
				}
				break;
			}
		}
	}

	return false;
}
*/
