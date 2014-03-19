/**
	\file
	\author Matthew Allen
	\date 20/3/1997
	\brief 8 bit primitives
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "Gdc2.h"
#include "GPalette.h"

/// 8 bit paletted applicators
class LgiClass GdcApp8 : public GApplicator
{
protected:
	uchar *Ptr;
	uchar *APtr;

public:
	GdcApp8()
	{
		Ptr = 0;
		APtr = 0;
	}

	const char *GetClass() { return "GdcApp8"; }
	bool SetSurface(GBmpMem *d, GPalette *p, GBmpMem *a);
	void SetPtr(int x, int y);
	void IncX();
	void IncY();
	void IncPtr(int X, int Y);
	COLOUR Get();
};

class LgiClass GdcApp8Set : public GdcApp8
{
public:
	const char *GetClass() { return "GdcApp8Set"; }
	void Set();
	void VLine(int height);
	void Rectangle(int x, int y);
	bool Blt(GBmpMem *Src, GPalette *SPal, GBmpMem *SrcAlpha);
};

class LgiClass GdcApp8And : public GdcApp8
{
public:
	const char *GetClass() { return "GdcApp8And"; }
	void Set();
	void VLine(int height);
	void Rectangle(int x, int y);
	bool Blt(GBmpMem *Src, GPalette *SPal, GBmpMem *SrcAlpha);
};

class LgiClass GdcApp8Or : public GdcApp8
{
public:
	const char *GetClass() { return "GdcApp8Or"; }
	void Set();
	void VLine(int height);
	void Rectangle(int x, int y);
	bool Blt(GBmpMem *Src, GPalette *SPal, GBmpMem *SrcAlpha);
};

class LgiClass GdcApp8Xor : public GdcApp8
{
public:
	const char *GetClass() { return "GdcApp8Xor"; }
	void Set();
	void VLine(int height);
	void Rectangle(int x, int y);
	bool Blt(GBmpMem *Src, GPalette *SPal, GBmpMem *SrcAlpha);
};

GApplicator *GApp8::Create(GColourSpace Cs, int Op)
{
	if (Cs == CsIndex8)
	{
		switch (Op)
		{
			case GDC_SET:
				return new GdcApp8Set;
			case GDC_AND:
				return new GdcApp8And;
			case GDC_OR:
				return new GdcApp8Or;
			case GDC_XOR:
				return new GdcApp8Xor;
		}
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////
bool GdcApp8::SetSurface(GBmpMem *d, GPalette *p, GBmpMem *a)
{
	if (d && d->Cs == CsIndex8)
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

void GdcApp8::SetPtr(int x, int y)
{
	LgiAssert(Dest && Dest->Base);
	Ptr = Dest->Base + ((y * Dest->Line) + x);
	if (Alpha)
		APtr = Alpha->Base + ((y * Alpha->Line) + x);
}

void GdcApp8::IncX()
{
	Ptr++;
	if (APtr)
		APtr++;
}

void GdcApp8::IncY()
{
	Ptr += Dest->Line;
	if (APtr)
		APtr += Alpha->Line;
}

void GdcApp8::IncPtr(int X, int Y)
{
	Ptr += (Y * Dest->Line) + X;
	if (APtr)
	{
		APtr += (Y * Dest->Line) + X;
	}
}

COLOUR GdcApp8::Get()
{
	return *Ptr;
}

// 8 bit set sub functions
void GdcApp8Set::Set()
{
	*Ptr = c;
	if (APtr)
	{
		*APtr = 0xff;
	}
}

void GdcApp8Set::VLine(int height)
{
	while (height--)
	{
		*Ptr = c;
		Ptr += Dest->Line;
		
		if (APtr)
		{
			*APtr = 0xff;
			APtr += Dest->Line;
		}
	}
}

void GdcApp8Set::Rectangle(int x, int y)
{
	COLOUR fill = c & 0xFF;

	fill = fill | (fill<<8) | (fill<<16) | (fill<<24);

#if 0 // defined(GDC_USE_ASM) && defined(_MSC_VER)

// this duplicates the colour twice in eax to allow us to fill
// four pixels per write. this means we are using the whole
// 32-bit bandwidth to the video card :)

	uchar *p = Ptr;
	int Line = Dest->Line;

	if (x > 0 && y > 0)
	{
		_asm {
			mov esi, p
			mov eax, fill
			mov edx, Line
		LoopY:	mov edi, esi
			add esi, edx
			mov ecx, x
			shr ecx, 2
			jz Odd
		LoopX:	mov [edi], eax
			add edi, 4
			dec ecx
			jnz LoopX
		Odd:	mov ecx, x
			and ecx, 3
			jz Next
		LoopO:	mov [edi], al
			inc edi
			dec ecx
			jnz LoopO
		Next:	dec y
			jnz LoopY
		}
	}

#else

	int x4, n;
	ulong *p;
	uchar *cp;

	while (y--)
	{	
		x4 = x >> 2;
		p = (ulong*) Ptr;

		for (n=0; n<x4; n++)
		{
			*p++ = fill;
		}

		cp = (uchar*) p;
		for (n=0; n<x%4; n++)
		{
			*cp++ = c;
		}

		Ptr += Dest->Line;

		if (APtr)
		{
			for (n=0; n<x; n++)
			{
				APtr[n] = 0xff;
			}
			APtr += Alpha->Line;
		}
	}

#endif
}

uchar Div51[256] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
	2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
	3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
	3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
	4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
	4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5};

uchar Mod51[256] = {
	0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
	20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37,
	38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 0,  1,  2,  3,  4,  5,  6,
	7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,
	26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43,
	44, 45, 46, 47, 48, 49, 50, 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12,
	13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
	31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48,
	49, 50, 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17,
	18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35,
	36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 0,  1,  2,  3,
	4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22,
	23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
	41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 0};

uchar DitherIndex8[64] = {
	0,  38,  9, 47,  2, 40, 11, 50,
	25, 12, 35, 22, 27, 15, 37, 24,
	6,  44,  3, 41,  8, 47,  5, 43,
	31, 19, 28, 15, 34, 21, 31, 18,
	1,  39, 11, 49,  0, 39, 10, 48,
	27, 14, 36, 23, 26, 13, 35, 23,
	7,  46,  4, 43,  7, 45,  3, 42,
	33, 20, 30, 17, 32, 19, 29, 16};

uchar Mul8[8] = {0, 8, 16, 24, 32, 40, 48, 56};
uchar Mul6[6] = {0, 6, 12, 18, 24, 30};
uchar Mul36[6] = {0, 36, 72, 108, 144, 180};

template<typename OutPx, typename InPx>
void GConvertIndexed(OutPx *out, InPx *in, int len, GColourSpace inCs, GPalette *pal)
{
	switch (inCs)
	{
		case CsIndex8:
		{
			OutPx p[256];
			for (int i=0; i<256; i++)
			{
				GdcRGB *rgb = pal ? (*pal)[i] : NULL;
				if (rgb)
				{
					p[i].r = rgb->r;
					p[i].g = rgb->g;
					p[i].b = rgb->b;
				}
				else
				{
					p[i].r = i;
					p[i].g = i;
					p[i].b = i;
				}
			}

			// Indexed colour
			InPx *end = in + len;
			while (in < end)
			{
				*out++ = p[*in++];
			}
			break;
		}
		default:
		{
			LgiAssert(0);
			break;
		}
	}
}

template<typename OutPx, typename InPx>
void GConvertRgb24(OutPx *out, InPx *in, int len, GColourSpace inCs, GPalette *pal)
{
	switch (inCs)
	{
		case CsRgb15:
		case CsBgr15:
		{
			// All 15 bit types
			InPx *end = in + len;
			while (in < end)
			{
				out->r = G5bitTo8Bit(in->r);
				out->g = G5bitTo8Bit(in->g);
				out->b = G5bitTo8Bit(in->b);
				in++;
				out++;
			}
			break;
		}
		case CsRgb16:
		case CsBgr16:
		{
			// All 16 bit types
			InPx *end = in + len;
			while (in < end)
			{
				out->r = G5bitTo8Bit(in->r);
				out->g = G6bitTo8Bit(in->g);
				out->b = G5bitTo8Bit(in->b);
				in++;
				out++;
			}
			break;
		}
		default:
		{
			// All straight RGB types fall into here
			InPx *end = in + len;
			while (in < end)
			{
				out->r = in->r;
				out->g = in->g;
				out->b = in->b;
				in++;
				out++;
			}
			break;
		}
	}
}

void Convert(System24BitPixel *Dst, GBmpMem *Src, int Line, GPalette *SPal)
{
	uchar *In = Src->Base + (Line * Src->Line);
	switch (Src->Cs)
	{
		#define ConvertCase(type) \
			case Cs##type: \
				GConvertRgb24<System24BitPixel, G##type>(Dst, (G##type*) In, Src->x, Src->Cs, NULL); \
				break
		
		case CsIndex8:
			GConvertIndexed<System24BitPixel, uint8>(Dst, In, Src->x, Src->Cs, SPal);
			break;
		ConvertCase(Rgb15);
		ConvertCase(Bgr15);
		ConvertCase(Rgb16);
		ConvertCase(Bgr16);
		ConvertCase(Rgb24);
		ConvertCase(Bgr24);
		ConvertCase(Rgba32);
		ConvertCase(Bgra32);
		ConvertCase(Argb32);
		ConvertCase(Abgr32);
		default:
			LgiAssert(0);
			break;
	}
}

bool GdcApp8Set::Blt(GBmpMem *Src, GPalette *SPal, GBmpMem *SrcAlpha)
{
	Dest->Flags &= ~GDC_UPDATED_PALETTE;
	if (Src)
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
				uchar *s = Src->Base;
				for (int y=0; y<Src->y; y++)
				{
					MemCpy(Ptr, s, Src->x);
					s += Src->Line;
					Ptr += Dest->Line;
				}
				break;
			}
			case System15BitColourSpace:
			case System16BitColourSpace:
			case System24BitColourSpace:
			case System32BitColourSpace:
			{
				switch (GdcD->GetOption(GDC_REDUCE_TYPE))
				{
					default:
					case REDUCE_NEAREST:
					{
						if (Pal)
						{
							// Create colour lookup table
							int LookupSize = 32 << 10;
							GAutoPtr<uchar> Mem(new uchar[LookupSize]);
							if (Mem)
							{
    							uchar *Lookup = Mem;
    							
								for (int i=0; i<LookupSize; i++)
								{
									int r = Rc15(i);
									int g = Gc15(i);
									int b = Bc15(i);

									Lookup[i] = Pal->MatchRgb(Rgb24(r, g, b));
								}

								// loop through all pixels and convert colour
								for (int y=0; y<Src->y; y++)
								{
									uchar *d = Ptr;
									Ptr += Dest->Line;

									switch (Src->Cs)
									{
										default:
										{
											LgiAssert(!"Not impl.");
											break;
										}
										case System15BitColourSpace:
										{
											ushort *s = (ushort*) (Src->Base + (y * Src->Line));
											for (int x=0; x<Src->x; x++)
											{
												*d++ = Lookup[*s++];
											}
											break;
										}
										case System16BitColourSpace:
										{
											ushort *s = (ushort*) (Src->Base + (y * Src->Line));
											for (int x=0; x<Src->x; x++)
											{
												*d++ = Lookup[Rgb16To15(*s)];
												s++;
											}
											break;
										}
										case System24BitColourSpace:
										{
											System24BitPixel *s = (System24BitPixel*) (Src->Base + (y * Src->Line));
											for (int x=0; x<Src->x; x++)
											{
												*d++ = Lookup[Rgb15(s->r, s->g, s->b)];
												s++;
											}
											break;
										}
										case System32BitColourSpace:
										{
											uint32 *s = (uint32*) (Src->Base + (y * Src->Line));
											for (int x=0; x<Src->x; x++)
											{
												*d++ = Lookup[Rgb32To15(*s)];
												s++;
											}
											break;
										}
									}
								}
							}
						}
						break;
					}
					case REDUCE_HALFTONE:
					{
						for (int y=0; y<Src->y; y++)
						{
							uchar *D = Ptr;
							int ym = y % 8;
							Ptr += Dest->Line;

							switch (Src->Cs)
							{
								default:
								{
									LgiAssert(!"Not impl.");
									break;
								}
								case CsRgb15:
								{
									ushort *S = (ushort*) (Src->Base + (y * Src->Line));
									for (int x=0; x<Src->x; x++)
									{
										int n = DitherIndex8[Mul8[x&7] + ym];
										int r = (*S >> 7) & 0xF8;
										int g = (*S >> 2) & 0xF8;
										int b = (*S << 3) & 0xF8;
										S++;
										
										*D++ =	(Div51[b] + (Mod51[b] > n)) +
			        							Mul6[Div51[g] + (Mod51[g] > n)] +
			        							Mul36[Div51[r] + (Mod51[r] > n)];
									}
									break;
								}
								case CsRgb16:
								{
									ushort *S = (ushort*) (Src->Base + (y * Src->Line));
									for (int x=0; x<Src->x; x++)
									{
										int n = DitherIndex8[Mul8[x&7] + ym];
										int r = (*S >> 8) & 0xF8;
										int g = (*S >> 3) & 0xFC;
										int b = (*S << 3) & 0xF8;
										S++;
										
										*D++ =	(Div51[b] + (Mod51[b] > n)) +
			        							Mul6[Div51[g] + (Mod51[g] > n)] +
			        							Mul36[Div51[r] + (Mod51[r] > n)];
									}
									break;
								}
								case System24BitColourSpace:
								{
									System24BitPixel *s = (System24BitPixel*) (Src->Base + (y * Src->Line));

									for (int x=0; x<Src->x; x++, s++)
									{
										int n = DitherIndex8[Mul8[x&7] + ym];
										
										*D++ =	(Div51[s->b] + (Mod51[s->b] > n)) +
			        							Mul6[Div51[s->g] + (Mod51[s->g] > n)] +
			        							Mul36[Div51[s->r] + (Mod51[s->r] > n)];
									}
									break;
								}
								case System32BitColourSpace:
								{
									System32BitPixel *s = (System32BitPixel*) (Src->Base + (y * Src->Line));
									for (int x=0; x<Src->x; x++, s++)
									{
										int n = DitherIndex8[Mul8[x&7] + ym];
										
										*D++ =	(Div51[s->b] + (Mod51[s->b] > n)) +
			        							Mul6[Div51[s->g] + (Mod51[s->g] > n)] +
			        							Mul36[Div51[s->r] + (Mod51[s->r] > n)];
									}
									break;
								}
							}
						}
						break;
					}
					case REDUCE_ERROR_DIFFUSION:
					{
						// dist = sqrt(0.299*SQR(r2-r1) + 0.587*SQR(g2-g1) + 0.114*SQR(b2-b1))

						// Floyd - Steinberg error diffusion... roughly anyway

						if (!Pal) break;

						GSurface *pBuf = new GMemDC;
						if (pBuf && pBuf->Create(Src->x+2, 2, 24))
						{
							// Clear buffer
							pBuf->Colour(0, 24);
							pBuf->Rectangle();

							// Create lookup table for converting RGB -> palette
							int LookupSize = 32 << 10; // 32KB
							uchar *Lookup = new uchar[LookupSize];
							if (Lookup)
							{
								int i;
								for (i=0; i<LookupSize; i++)
								{
									int r = Rc15(i);
									int g = Gc15(i);
									int b = Bc15(i);

									Lookup[i] = Pal->MatchRgb(Rgb24(r, g, b));
								}

								// Error diffusion buffer
								System24BitPixel *Buffer[2];
								Buffer[0] = ((System24BitPixel*) (*pBuf)[0]) + 1;
								Buffer[1] = ((System24BitPixel*) (*pBuf)[0]) + 1;

								// Depth converter function
								
								// Convert the first scan line
								Convert(Buffer[0], Src, 0, SPal);

								// Loop through pixels
								for (int y=0; y<Src->y; y++)
								{
									System24BitPixel *src = Buffer[0];
									System24BitPixel *next = Buffer[1];
									uchar *dst = Ptr;

									if (y < Src->y - 1)
									{
										// Convert the next scanline
										Convert(Buffer[1], Src, y + 1, SPal);
									}

									for (int x=0; x<Src->x; x++, dst++, src++, next++)
									{
										*dst = Lookup[Rgb15(src->r, src->g, src->b)];

										// cube nearest colour calc
										// *D = (IndexLookup[S[2]]*36) + (IndexLookup[S[1]]*6) + IndexLookup[S[0]];

										GdcRGB *P = (*Pal)[*dst];

										#define Diffuse(c)									\
										{													\
											int nError = src->c - P->c;						\
											if (nError != 0)								\
											{												\
												int nRemainder = 0;							\
												int n;										\
																							\
												/* next pixel: 7/16th's */					\
												n = src[1].c + ((nError * 7) >> 4);			\
												n = limit(n, 0, 255);						\
												nRemainder += n - src[1].c;					\
												src[1].c = n;								\
																							\
												/* below and to the left: 3/16th's */		\
												n = next[-1].c + ((nError * 3) >> 4);		\
												n = limit(n, 0, 255);						\
												nRemainder += n - next[-1].c;				\
												next[-1].c = n;								\
																							\
												/* below: 5/16th's */						\
												n = next[0].c + ((nError * 5) >> 4);		\
												n = limit(n, 0, 255);						\
												nRemainder += n - next[0].c;				\
												next[0].c = n;								\
																							\
												/* below and to the right: 1/16th */		\
												n = next[1].c + (nError - nRemainder);		\
												n = limit(n, 0, 255);						\
												next[1].c = n;								\
											}												\
										}
										
										Diffuse(r)
										Diffuse(g)
										Diffuse(b)
									}
									
									// Copy the next scanline into the current one
									memcpy(Buffer[0]-1, Buffer[1]-1, (Src->x + 2) * sizeof(*Buffer[0]));
									
									// Move output ptr to the next scanline
									Ptr += Dest->Line;
								}
							}

							DeleteObj(pBuf);
						}
						break;
					}
				}
				break;
			}
		}
	}
	return true;
}

// 8 bit or sub functions
void GdcApp8Or::Set()
{
	*Ptr |= c;
}

void GdcApp8Or::VLine(int height)
{
	while (height--)
	{
		*Ptr |= c;
		Ptr += Dest->Line;
	}
}

void GdcApp8Or::Rectangle(int x, int y)
{
	while (y--)
	{
		for (int n=0; n<x; n++) Ptr[n] |= c;
		Ptr += Dest->Line;
	}
}

bool GdcApp8Or::Blt(GBmpMem *Src, GPalette *SPal, GBmpMem *SrcAlpha)
{
	if (Src)
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
				uchar *s = Src->Base;
				for (int y=0; y<Src->y; y++)
				{
					MemOr(Ptr, s, Src->x);
					s += Src->Line;
					Ptr += Dest->Line;
				}
				break;
			}
		}
	}
	return true;
}

// 8 bit AND sub functions
void GdcApp8And::Set()
{
	*Ptr &= c;
}

void GdcApp8And::VLine(int height)
{
	while (height--)
	{
		*Ptr &= c;
		Ptr += Dest->Line;
	}
}

void GdcApp8And::Rectangle(int x, int y)
{
	while (y--)
	{
		for (int n=0; n<x; n++) Ptr[n] &= c;
		Ptr += Dest->Line;
	}
}

bool GdcApp8And::Blt(GBmpMem *Src, GPalette *SPal, GBmpMem *SrcAlpha)
{
	if (Src && Src->Cs == Dest->Cs)
	{
		uchar *s = Src->Base;
		for (int y=0; y<Src->y; y++)
		{
			MemAnd(Ptr, s, Src->x);
			s += Src->Line;
			Ptr += Dest->Line;
		}
	}
	return true;
}

// 8 bit XOR sub functions
void GdcApp8Xor::Set()
{
	*Ptr ^= c;
}

void GdcApp8Xor::VLine(int height)
{
	while (height--)
	{
		*Ptr ^= c;
		Ptr += Dest->Line;
	}
}

void GdcApp8Xor::Rectangle(int x, int y)
{
	while (y--)
	{
		for (int n=0; n<x; n++) Ptr[n] ^= c;
		Ptr += Dest->Line;
	}
}

bool GdcApp8Xor::Blt(GBmpMem *Src, GPalette *SPal, GBmpMem *SrcAlpha)
{
	if (Src && Src->Cs == Dest->Cs)
	{
		uchar *s = Src->Base;
		for (int y=0; y<Src->y; y++)
		{
			MemXor(Ptr, s, Src->x);
			s += Src->Line;
			Ptr += Dest->Line;
		}
	}
	return true;
}
