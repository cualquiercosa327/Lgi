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

// #define Div255(a)	DivLut[a]
#define Div255(a)	((a)/255)

/// Alpha blending applicators
class GAlphaApp : public GApplicator
{
protected:
	uchar alpha, oma;
	int Bits, Bytes;
	uchar *Ptr;
	uchar *APtr;

	const char *GetClass() { return "GAlphaApp"; }

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

class GdcApp24Alpha : public GAlphaApp
{
public:
	GdcApp24Alpha()
	{
		Bits = 24;
		Bytes = 3;
	}

	void Set();
	void VLine(int height);
	void Rectangle(int x, int y);
	bool Blt(GBmpMem *Src, GPalette *SPal, GBmpMem *SrcAlpha);
};

class GdcApp32Alpha : public GAlphaApp
{
public:
	GdcApp32Alpha()
	{
		Bits = 32;
		Bytes = 4;
	}

	void Set();
	void VLine(int height);
	void Rectangle(int x, int y);
	bool Blt(GBmpMem *Src, GPalette *SPal, GBmpMem *SrcAlpha);
};

GApplicator *GAlphaFactory::Create(GColourSpace Cs, int Op)
{
	if (Op == GDC_ALPHA)
	{
		switch (Cs)
		{
			default:
				printf("%s:%i - Unknown colour space: 0x%x %s\n",
					_FL, Cs, GColourSpaceToString(Cs));
				LgiAssert(0);
				break;
			case CsIndex8:
				return new GdcApp8Alpha;
			case CsRgb15:
				return new GdcApp15Alpha;
			case CsRgb16:
			case CsBgr16:
				return new GdcApp16Alpha;
			case System24BitColourSpace:
				return new GdcApp24Alpha;
			case System32BitColourSpace:
				return new GdcApp32Alpha;
		}
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
		for (int X=0; X<x; X++)
		{
			*p++ = Remap[*p];
		}
		Ptr += Dest->Line;

		if (APtr)
		{
			uchar *a = APtr;
			for (int X=0; X<x; X++)
			{
				*a++ += DivLut[(255 - *a) * alpha];
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
				GRgb24 sc[256];
				CreatePaletteLut(sc, SPal);
				
				GRgb24 dc[256];
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
						GRgb24 *src = sc + *s;

						if (a == 255)
						{
							r = src->r;
							g = src->g;
							b = src->b;
						}
						else if (a)
						{
							uchar o = 0xff - a;
							GRgb24 *dst = dc + *d;
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
			case CsRgb15:
			{
				GRgb24 dc[256];
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
							GRgb24 *dst = dc + *d;
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
			case CsRgb16:
			{
				GRgb24 dc[256];
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
							GRgb24 *dst = dc + *d;
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
				GRgb24 dc[256];
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
							GRgb24 *dst = dc + *d;
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
				LgiAssert(!"Not impl.");
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
					GRgb24 sc[256];
					CreatePaletteLut(sc, SPal, alpha);
					
					GRgb24 dc[256];
					CreatePaletteLut(dc, DPal, oma);

					if (!Lut) Lut = DPal->MakeLut(15);

					for (int y=0; y<Src->y; y++)
					{
						uchar *s = Src->Base + (y * Src->Line);
						uchar *d = Ptr;

						for (int x=0; x<Src->x; x++, s++, d++)
						{
							GRgb24 *src = sc + *s;
							GRgb24 *dst = dc + *d;
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
			case CsRgb15:
			{
				GRgb24 dc[256];
				CreatePaletteLut(dc, DPal, oma);
				
				if (!Lut) Lut = DPal->MakeLut(15);

				for (int y=0; y<Src->y; y++)
				{
					ushort *s = (ushort*) (Src->Base + (y * Src->Line));
					uchar *d = Ptr;
					uchar *e = d + Src->x;

					while (d < e)
					{
						GRgb24 *dst = dc + *d;
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
			case CsRgb16:
			{
				GRgb24 dc[256];
				CreatePaletteLut(dc, DPal, oma);
				
				if (!Lut) Lut = DPal->MakeLut(15);

				for (int y=0; y<Src->y; y++)
				{
					ushort *s = (ushort*) (Src->Base + (y * Src->Line));
					uchar *d = Ptr;

					for (int x=0; x<Src->x; x++, s++, d++)
					{
						GRgb24 *dst = dc + *d;
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
				GRgb24 dc[256];
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
							GRgb24 *dst = dc + *d;
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
				GRgb24 c[256];
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
							GRgb24 *src = c + *s;
							*d = Rgb15(src->r, src->g, src->b);
						}
						else if (a)
						{
							GRgb24 *src = c + *s;
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
				LgiAssert(!"Not impl.");
				break;
			}
			case CsIndex8:
			{
				GRgb24 c[256];
				if (alpha) CreatePaletteLut(c, SPal, alpha);
				
				for (int y=0; y<Src->y; y++)
				{
					ushort *d = (ushort*) Ptr;
					ushort *e = d + Src->x;
					uchar *s = Src->Base + (Src->Line * y);

					GRgb24 *Src; 
					if (alpha == 255)
					{
						// copy
						while (d < e)
						{
							Src = c + *s++;
							*d = Rgb15(Src->r, Src->g, Src->b);
						}
					}
					else if (alpha)
					{
						// blend
						while (d < e)
						{
							Src = c + *s++;
							*d++ = Rgb15
							(
								DivLut[oma * Rc15(*d)] + Src->r,
								DivLut[oma * Gc15(*d)] + Src->g,
								DivLut[oma * Bc15(*d)] + Src->b
							);
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
				GRgb24 c[256];
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
							GRgb24 *src = c + *s;
							*d = Rgb16(src->r, src->g, src->b);
						}
						else if (a)
						{
							GRgb24 *src = c + *s;
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
			case CsRgb15:
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
			case CsRgb16:
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
				LgiAssert(!"Not impl.");
				break;
			}
			case CsIndex8:
			{
				// this code uses the input bitmap as a mask to say where
				// to draw the current colour at the given alpha value
				GRgb24 c[256];
				CreatePaletteLut(c, SPal, alpha);

				for (int y=0; y<Src->y; y++)
				{
					ushort *d = (ushort*) Ptr;
					uchar *s = Src->Base + (Src->Line * y);

					for (int x=0; x<Src->x; x++, d++, s++)
					{
						GRgb24 *src = c + *s;
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
						*d++ = Rgb16(	Div255((oma * Rc16(*d)) + (alpha * s->r)),
										Div255((oma * Gc16(*d)) + (alpha * s->g)),
										Div255((oma * Bc16(*d)) + (alpha * s->b)));
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

/////////////////////////////////////////////////////////////////////////////////////////////////////
#define Setup24()	\
	GRgb24 *p = (GRgb24*)Ptr; \
	uchar *DivLut = Div255Lut; \
	int r = R24(c) * alpha; \
	int g = G24(c) * alpha; \
	int b = B24(c) * alpha;

#define Comp24() \
	p->r = DivLut[(oma * p->r) + r]; \
	p->g = DivLut[(oma * p->g) + g]; \
	p->b = DivLut[(oma * p->b) + b]; \

void GdcApp24Alpha::Set()
{
	Setup24();
	Comp24();
}

void GdcApp24Alpha::VLine(int height)
{
	Setup24();

	while (height--)
	{
		Comp24();
		p = (GRgb24*) (Ptr += Dest->Line);
	}
}

void GdcApp24Alpha::Rectangle(int x, int y)
{
	Setup24();

	while (y--)
	{
		for (int n=0; n<x; n++)
		{
			Comp24();
			p++;
		}

		p = (GRgb24*) (Ptr += Dest->Line);
	}
}

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
				GRgb24 c[256];
				CreatePaletteLut(c, SPal, 255);

				for (int y=0; y<Src->y; y++)
				{
					uchar *s = Src->Base + (y * Src->Line);
					uchar *e = s + Src->x;
					uchar *sa = SrcAlpha->Base + (y * SrcAlpha->Line);
					GRgb24 *d = (GRgb24*) Ptr;

					while (s < e)
					{
						uchar a = lookup[*sa++];
						uchar o = 255 - a;

						if (a == 255)
						{
							GRgb24 *src = c + *s;
							*d = *src;
						}
						else if (a)
						{
							GRgb24 *src = c + *s;
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
			case CsRgb15:
			{
				for (int y=0; y<Src->y; y++)
				{
					ushort *s = (ushort*) (Src->Base + (y * Src->Line));
					ushort *e = s + Src->x;
					uchar *sa = SrcAlpha->Base + (y * SrcAlpha->Line);
					GRgb24 *d = (GRgb24*) Ptr;

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
			case CsRgb16:
			{
				for (int y=0; y<Src->y; y++)
				{
					ushort *s = (ushort*) (Src->Base + (y * Src->Line));
					ushort *e = s + Src->x;
					uchar *sa = SrcAlpha->Base + (y * SrcAlpha->Line);
					GRgb24 *d = (GRgb24*) Ptr;

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
					GRgb24 *d = (GRgb24*) Ptr;

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
				GRgb24 c[256];
				CreatePaletteLut(c, SPal, alpha);

				for (int y=0; y<Src->y; y++)
				{
					uchar *s = Src->Base + (y * Src->Line);
					uchar *e = s + Src->x;
					GRgb24 *d = (GRgb24*) Ptr;

					while (s < e)
					{
						GRgb24 *src = c + *s++;
						d->r = src->r + DivLut[d->r * oma];
						d->g = src->g + DivLut[d->g * oma];
						d->b = src->b + DivLut[d->b * oma];
						d++;
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
					ushort *e = s + Src->x;
					GRgb24 *d = (GRgb24*) Ptr;

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
			case CsRgb16:
			{
				for (int y=0; y<Src->y; y++)
				{
					ushort *s = (ushort*) (Src->Base + (y * Src->Line));
					GRgb24 *d = (GRgb24*) Ptr;

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
					GBgr24 *d = (GBgr24*) Ptr;

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
			case System32BitColourSpace:
			{
				for (int y=0; y<Src->y; y++)
				{
					System32BitPixel *s = (System32BitPixel*) (Src->Base + (y * Src->Line));
					GRgb24 *d = (GRgb24*) Ptr;

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

/////////////////////////////////////////////////////////////////////////////////////////////////////
#define Setup32()	\
	uchar *DivLut = Div255Lut; \
	int a = DivLut[A32(c) * alpha];	\
	int oma = 255 - a;	\
	int r = R32(c) * a; \
	int g = G32(c) * a; \
	int b = B32(c) * a;

#define Comp32() \
	p->r = DivLut[(oma * p->r) + r]; \
	p->g = DivLut[(oma * p->g) + g]; \
	p->b = DivLut[(oma * p->b) + b]; \
	p->a = (p->a + a) - DivLut[p->a * a];

void GdcApp32Alpha::Set()
{
	System32BitPixel *p = (System32BitPixel*) Ptr;
	Setup32();
	Comp32();
}

void GdcApp32Alpha::VLine(int height)
{
	Setup32();

	while (height--)
	{
		System32BitPixel *p = (System32BitPixel*) Ptr;
		Comp32();
		Ptr = (((uchar*) Ptr) + Dest->Line);
	}
}

void GdcApp32Alpha::Rectangle(int x, int y)
{
	Setup32();

	while (y--)
	{
		System32BitPixel *p = (System32BitPixel*) Ptr;
		for (int n=0; n<x; n++, p++)
		{
			Comp32();
		}

		Ptr = (((uchar*) Ptr) + Dest->Line);
	}
}

bool GdcApp32Alpha::Blt(GBmpMem *Src, GPalette *SPal, GBmpMem *SrcAlpha)
{
	if (!Src) return 0;
	uchar *DivLut = Div255Lut;
	uchar lookup[256];
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
				GRgb24 c[256];
				CreatePaletteLut(c, SPal);

				for (int y=0; y<Src->y; y++)
				{
					uchar *s = (uchar*) (Src->Base + (y * Src->Line));
					uchar *sa = (uchar*) (SrcAlpha->Base + (y * SrcAlpha->Line));
					GRgb24 *sc;
					System32BitPixel *d = (System32BitPixel*) Ptr;
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

					Ptr = (((uchar*) Ptr) + Dest->Line);
				}
				break;
			}
			case CsRgb15:
			{
				for (int y=0; y<Src->y; y++)
				{
					ushort *s = (ushort*) (Src->Base + (y * Src->Line));
					uchar *sa = SrcAlpha->Base + (y * SrcAlpha->Line);
					System32BitPixel *d = (System32BitPixel*) Ptr;

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

					Ptr += Dest->Line;
				}
				break;
			}
			case CsRgb16:
			{
				for (int y=0; y<Src->y; y++)
				{
					ushort *s = (ushort*) (Src->Base + (y * Src->Line));
					uchar *sa = SrcAlpha->Base + (y * SrcAlpha->Line);
					System32BitPixel *d = (System32BitPixel*) Ptr;

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

					Ptr += Dest->Line;
				}
				break;
			}
			case System24BitColourSpace:
			{
				for (int y=0; y<Src->y; y++)
				{
					uchar *sa = SrcAlpha->Base + (y * SrcAlpha->Line);
					System32BitPixel *d = (System32BitPixel*) Ptr;
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

					Ptr += Dest->Line;
				}
				break;
			}
			case System32BitColourSpace:
			{
				for (int y=0; y<Src->y; y++)
				{
					System32BitPixel *d = (System32BitPixel*) Ptr;
					System32BitPixel *s = (System32BitPixel*) (Src->Base + (y * Src->Line));
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
				GRgb24 c[256];
				CreatePaletteLut(c, SPal, alpha);

				for (int y=0; y<Src->y; y++)
				{
					uchar *s = (uchar*) (Src->Base + (y * Src->Line));
					GRgb24 *sc;
					System32BitPixel *d = (System32BitPixel*) Ptr;

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
							d->a = (alpha + d->a) - DivLut[alpha * d->a];

							d++;
						}
					}

					Ptr = (((uchar*) Ptr) + Dest->Line);
				}
				break;
			}
			case CsRgb15:
			{
				for (int y=0; y<Src->y; y++)
				{
					ushort *s = (ushort*) (Src->Base + (y * Src->Line));
					ushort *e = s + Src->x;
					System32BitPixel *d = (System32BitPixel*) Ptr;

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
			case CsRgb16:
			{
				for (int y=0; y<Src->y; y++)
				{
					ushort *s = (ushort*) (Src->Base + (y * Src->Line));
					System32BitPixel *d = (System32BitPixel*) Ptr;

					if (alpha == 255)
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
					else if (alpha)
					{
						for (int x=0; x<Src->x; x++)
						{
							d->r = DivLut[(d->r * oma) + (Rc16(*s) * alpha)];
							d->g = DivLut[(d->g * oma) + (Gc16(*s) * alpha)];
							d->b = DivLut[(d->b * oma) + (Bc16(*s) * alpha)];
							d->a = (alpha + d->a) - DivLut[alpha * d->a];

							d++;
							s++;
						}
					}

					Ptr = (((uchar*) Ptr) + Dest->Line);
				}
				break;
			}
			case CsBgr16:
			{
				for (int y=0; y<Src->y; y++)
				{
					GBgr16 *s = (GBgr16*) (Src->Base + (y * Src->Line));
					System32BitPixel *d = (System32BitPixel*) Ptr;

					#define Comp5BitTo8Bit(c) \
						(((uint8)(c) << 3) | ((c) >> 2))
					#define Comp6BitTo8Bit(c) \
						(((uint8)(c) << 2) | ((c) >> 4))

					if (alpha == 255)
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
					else if (alpha)
					{
						for (int x=0; x<Src->x; x++)
						{
							d->r = DivLut[(d->r * oma) + (Comp5BitTo8Bit(s->r) * alpha)];
							d->g = DivLut[(d->g * oma) + (Comp6BitTo8Bit(s->g) * alpha)];
							d->b = DivLut[(d->b * oma) + (Comp5BitTo8Bit(s->b) * alpha)];
							d->a = (alpha + d->a) - DivLut[alpha * d->a];

							d++;
							s++;
						}
					}

					Ptr = (((uchar*) Ptr) + Dest->Line);
				}
				break;
			}
			case System24BitColourSpace:
			{
				for (int y=0; y<Src->y; y++)
				{
					System24BitPixel *s = (System24BitPixel*) (Src->Base + (y * Src->Line));
					System32BitPixel *d = (System32BitPixel*) Ptr;

					if (alpha == 255)
					{
						for (int x=0; x<Src->x; x++)
						{
							d->r = s->r;
							d->g = s->g;
							d->b = s->b;
							d->a = 255;

							d++;
							s++;
						}
					}
					else if (alpha)
					{
						for (int x=0; x<Src->x; x++)
						{
							d->r = DivLut[(d->r * oma) + (s->r * alpha)];
							d->g = DivLut[(d->g * oma) + (s->g * alpha)];
							d->b = DivLut[(d->b * oma) + (s->b * alpha)];
							d->a = (d->a * alpha) - DivLut[d->a * alpha];

							d++;
							s++;
						}
					}

					Ptr = (((uchar*) Ptr) + Dest->Line);
				}
				break;
			}
			case System32BitColourSpace:
			{
				for (int y=0; y<Src->y; y++)
				{
					System32BitPixel *s = (System32BitPixel*) (Src->Base + (y * Src->Line));
					System32BitPixel *d = (System32BitPixel*) Ptr;

					if (alpha == 255)
					{
						// 32bit alpha channel blt
						for (int x=0; x<Src->x; x++)
						{
							if (s->a == 255)
							{
								*d = *s;
							}
							else if (s->a)
							{
								uchar o = 255 - s->a;
								int ra = (d->a + s->a) - DivLut[d->a * s->a];
								#define rop(c) d->c = (DivLut[DivLut[d->c * d->a] * o] + DivLut[s->c * s->a]) * 255 / ra;
								rop(r);
								rop(g);
								rop(b);
								#undef rop
								d->a = ra;
							}

							d++;
							s++;
						}
					}
					else if (alpha)
					{
						// Const alpha + 32bit alpha channel blt
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
					}

					Ptr = (((uchar*) Ptr) + Dest->Line);
				}
				break;
			}
		}
	}

	return true;
}

