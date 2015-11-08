/*
**	FILE:			DcTools.cpp
**	AUTHOR:			Matthew Allen
**	DATE:			3/8/98
**	DESCRIPTION:	Device context tools
**
**	Copyright (C) 1998-2001, Matthew Allen
**		fret@memecode.com
*/

#include <math.h>
#include "Gdc2.h"
#include "GdcTools.h"
#include "GPalette.h"

bool IsGreyScale(GSurface *pDC)
{
	bool Status = false;
	if (pDC)
	{
		GPalette *Pal = pDC->Palette();
		Status = true;
		if (Pal && Pal->GetSize() > 0)
		{
			for (int i=0; i<Pal->GetSize(); i++)
			{
				GdcRGB *p = (*Pal)[i];
				if (p)
				{
					int Grey = i * 255 / Pal->GetSize();
					if (p->r != Grey ||
						p->g != Grey ||
						p->b != Grey)
					{
						Status = false;
					}
				}
			}
		}
	}
	return Status;
}

#define FP_RED_TO_GREY		19595	// 0.299 * 65536
#define FP_GREEN_TO_GREY	38469	// 0.587 * 65536
#define FP_BLUE_TO_GREY		7471	// 0.114 * 65536

bool GreyScaleDC(GSurface *pDest, GSurface *pSrc)
{
	bool Status = false;

	if (pDest && pSrc)
	{
		switch (pSrc->GetBits())
		{
			case 8:
			{
				// 8 -> 8 greyscale convert
				if (pDest->Create(pSrc->X(), pSrc->Y(), CsIndex8))
				{
					GPalette *Pal = pSrc->Palette();
					if (Pal)
					{
						// convert palette
						uchar Map[256];
						ZeroObj(Map);
						for (int i=0; i<Pal->GetSize(); i++)
						{
							// Weights, r:.299, g:.587, b:.114
							GdcRGB *n = (*Pal)[i];
							if (n)
							{
								uint32 r = ((uint32) n->r << 16) * FP_RED_TO_GREY;
								uint32 g = ((uint32) n->g << 16) * FP_GREEN_TO_GREY;
								uint32 b = ((uint32) n->b << 16) * FP_BLUE_TO_GREY;
								Map[i] = (r + g + b) >> 16;

								n->r = n->g = n->b = i;
							}
						}

						Pal->Update();
						pDest->Palette(Pal, true);
						Status = true;

						// convert pixels..
						for (int y=0; y<pSrc->Y(); y++)
						{
							uchar *s = (*pSrc)[y];
							uchar *d = (*pDest)[y];
							for (int x=0; x<pSrc->X(); x++)
							{
								d[x] = Map[s[x]];
							}
						}
					}
				}
				break;
			}
			case 16:
			case 24:
			case 32:
			{
				if (pDest->Create(pSrc->X(), pSrc->Y(), CsIndex8))
				{
					uchar RMap[256];
					uchar GMap[256];
					uchar BMap[256];
					for (int i=0; i<256; i++)
					{
						RMap[i] = ((i << 16) * FP_RED_TO_GREY) >> 16;
						GMap[i] = ((i << 16) * FP_GREEN_TO_GREY) >> 16;
						BMap[i] = ((i << 16) * FP_BLUE_TO_GREY) >> 16;
					}
					
					int SBits = pSrc->GetBits();
					for (int y=0; y<pSrc->Y(); y++)
					{
						switch (SBits)
						{
							case 16:
							{
								ushort *Src = (ushort*) (*pSrc)[y];
								uchar *Dest = (*pDest)[y];

								for (int x=0; x<pSrc->X(); x++)
								{
									uchar r = R16(Src[x]) << 3;
									uchar g = G16(Src[x]) << 2;
									uchar b = B16(Src[x]) << 3;
									Dest[x] = RMap[r] + GMap[g] + BMap[b];
								}
								break;
							}
							case 24:
							{
								uchar *Src = (*pSrc)[y];
								uchar *Dest = (*pDest)[y];

								for (int x=0; x<pSrc->X(); x++)
								{
									uchar *x3 = Src + ((x<<1) + x);
									Dest[x] = RMap[ x3[2] ] + GMap[ x3[1] ] + BMap[ x3[0] ];
								}
								break;
							}
							case 32:
							{
								ulong *Src = (ulong*) (*pSrc)[y];
								uchar *Dest = (*pDest)[y];

								for (int x=0; x<pSrc->X(); x++)
								{
									uchar r = R32(Src[x]);
									uchar g = G32(Src[x]);
									uchar b = B32(Src[x]);
									Dest[x] = RMap[r] + GMap[g] + BMap[b];
								}
								break;
							}
						}
					}

					GPalette *Pal = new GPalette(0, 256);
					if (Pal)
					{
						// convert palette
						for (int i=0; i<Pal->GetSize(); i++)
						{
							GdcRGB *n = (*Pal)[i];
							if (n)
							{
								n->r = n->g = n->b = i;
							}
						}

						pDest->Palette(Pal);
						Status = true;
					}
				}
				break;
			}
		}
	}

	return Status;
}

bool InvertDC(GSurface *pDC)
{
	bool Status = true;

	if (pDC)
	{
		switch (pDC->GetBits())
		{
			case 8:
			{
				if (IsGreyScale(pDC))
				{
					for (int y=0; y<pDC->Y(); y++)
					{
						uchar *d = (*pDC)[y];
						if (d)
						{
							for (int x=0; x<pDC->X(); x++)
							{
								d[x] = 255 - d[x];
							}
						}
					}
					Status = true;
				}
				else
				{
					GPalette *Pal = pDC->Palette();
					if (Pal)
					{
						GdcRGB *p = (*Pal)[0];
						if (p)
						{
							for (int i=0; i<Pal->GetSize(); i++)
							{
								p[i].r = 255 - p[i].r;
								p[i].g = 255 - p[i].g;
								p[i].b = 255 - p[i].b;
							}
							Status = true;
						}
					}
				}
				break;
			}
			case 16:
			{
				for (int y=0; y<pDC->Y(); y++)
				{
					ushort *d = (ushort*) (*pDC)[y];
					if (d)
					{
						for (int x=0; x<pDC->X(); x++)
						{
							d[x] =	((31 - R16(d[x])) << 11) |
								((63 - G16(d[x])) << 5) |
								((31 - B16(d[x])));
						}
					}
				}
				break;
			}
			case 24:
			{
				for (int y=0; y<pDC->Y(); y++)
				{
					uchar *d = (*pDC)[y];
					if (d)
					{
						for (int x=0; x<pDC->X(); x++)
						{
							*d = 255 - *d; d++;
							*d = 255 - *d; d++;
							*d = 255 - *d; d++;
						}
					}
				}
				break;
			}
			case 32:
			{
				for (int y=0; y<pDC->Y(); y++)
				{
					uchar *d = (*pDC)[y];
					if (d)
					{
						for (int x=0; x<pDC->X(); x++)
						{
							*d = 255 - *d; d++;
							*d = 255 - *d; d++;
							*d = 255 - *d; d++;
							d++; // leave alpha channel alone
						}
					}
				}
				break;
			}
		}
	}

	return Status;
}

bool FlipDC(GSurface *pDC, int Dir)
{
	bool Status = false;

	if (pDC)
	{
		switch (Dir)
		{
			case FLIP_X:
			{
				for (int y=0; y<pDC->Y(); y++)
				{
					for (int x=0, nx=pDC->X()-1; x<nx; x++, nx--)
					{
						COLOUR c = pDC->Get(x, y);
						pDC->Colour(pDC->Get(nx, y));
						pDC->Set(x, y);
						pDC->Colour(c);
						pDC->Set(nx, y);
					}
				}

				Status = true;
				break;
			}

			case FLIP_Y:
			{
				for (int y=0, ny=pDC->Y()-1; y<ny; y++, ny--)
				{
					for (int x=0; x<pDC->X(); x++)
					{
						COLOUR c = pDC->Get(x, y);
						pDC->Colour(pDC->Get(x, ny));
						pDC->Set(x, y);
						pDC->Colour(c);
						pDC->Set(x, ny);
					}
				}

				Status = true;
				break;
			}
		}
	}

	return Status;
}

bool RotateDC(GSurface *pDC, double Angle)
{
	if (!pDC)
		return false;

	GAutoPtr<GSurface> pOld(new GMemDC(pDC));

	// do the rotation
	if (Angle == 180)
	{
		GSurface *pOldAlpha = pOld->AlphaDC();
		GSurface *pAlpha = pDC->AlphaDC();

		for (int y=0; y<pOld->Y(); y++)
		{
			for (int x=0; x<pOld->X(); x++)
			{
				pDC->Colour(pOld->Get(x, y));
				pDC->Set(pOld->X()-x-1, pOld->Y()-y-1);

				if (pOldAlpha && pAlpha)
				{
					pAlpha->Colour(pOldAlpha->Get(x, y));
					pAlpha->Set(pOld->X()-x-1, pOld->Y()-y-1);
				}
			}
		}
	}
	else if (Angle == 90 || Angle == 270)
	{
		if (!pDC->Create(pOld->Y(), pOld->X(), pOld->GetColourSpace()))
			return false;

		GSurface *pOldAlpha = pOld->AlphaDC();
		if (pOldAlpha)
		{
			pDC->HasAlpha(true);
		}

		GSurface *pAlpha = pDC->AlphaDC();
		if (Angle == 90)
		{
			for (int y=0; y<pOld->Y(); y++)
			{
				for (int x=0; x<pOld->X(); x++)
				{
					pDC->Colour(pOld->Get(x, y));
					pDC->Set(pDC->X()-y-1, x);

					if (pOldAlpha && pAlpha)
					{
						pAlpha->Colour(pOldAlpha->Get(x, y));
						pAlpha->Set(pDC->X()-y-1, x);
					}
				}
			}
		}
		else
		{
			for (int y=0; y<pOld->Y(); y++)
			{
				int Dy = pDC->Y() - 1;
				for (int x=0; x<pOld->X(); x++, Dy--)
				{
					pDC->Colour(pOld->Get(x, y));
					pDC->Set(y, Dy);

					if (pOldAlpha && pAlpha)
					{
						pAlpha->Colour(pOldAlpha->Get(x, y));
						pAlpha->Set(y, Dy);
					}
				}
			}
		}
	}
	else
	{
		// free angle
		return false;
	}

	return true;
}

bool FlipXDC(GSurface *pDC)
{
	bool Status = false;
	if (pDC)
	{
		for (int y=0; y<pDC->Y(); y++)
		{
			int Sx = 0;
			int Ex = pDC->X()-1;
			for (; Sx<Ex; Sx++, Ex--)
			{
				COLOUR c = pDC->Get(Sx, y);
				pDC->Colour(pDC->Get(Ex, y));
				pDC->Set(Sx, y);
				pDC->Colour(c);
				pDC->Set(Ex, y);
			}
		}
		Status = true;
	}
	return Status;
}

bool FlipYDC(GSurface *pDC)
{
	bool Status = false;
	if (pDC)
	{
		for (int x=0; x<pDC->X(); x++)
		{
			int Sy = 0;
			int Ey = pDC->Y()-1;
			for (; Sy<Ey; Sy++, Ey--)
			{
				COLOUR c = pDC->Get(x, Sy);
				pDC->Colour(pDC->Get(x, Ey));
				pDC->Set(x, Sy);
				pDC->Colour(c);
				pDC->Set(x, Ey);
			}
		}
		Status = true;
	}
	return Status;
}

// Remaps DC to new palette
bool RemapDC(GSurface *pDC, GPalette *DestPal)
{
	bool Status = false;
	if (pDC && pDC->GetBits() <= 8 && DestPal)
	{
		GPalette *SrcPal = pDC->Palette();
		int Colours = (SrcPal) ? SrcPal->GetSize() : 1 << pDC->GetBits();
		uchar *Remap = new uchar[Colours];
		if (Remap)
		{
			if (SrcPal)
			{
				for (int i=0; i<Colours; i++)
				{
					GdcRGB *p = (*SrcPal)[i];
					if (p)
					{
						Remap[i] = DestPal->MatchRgb(Rgb24(p->r, p->g, p->b));
					}
					else
					{
						Remap[i] = 0;
					}
				}
			}
			else
			{
				for (int i=0; i<Colours; i++)
				{
					Remap[i] = DestPal->MatchRgb(Rgb24(i, i, i));
				}
			}

			for (int y=0; y<pDC->Y(); y++)
			{
				for (int x=0; x<pDC->X(); x++)
				{
					pDC->Colour(Remap[pDC->Get(x, y)]);
					pDC->Set(x, y);
				}
			}

			Status = true;
			DeleteArray(Remap);
		}
	}

	return Status;
}

/* floating point resample code...

	double Area = 0.0;
	double R = 0.0;
	double G = 0.0;
	double B = 0.0;

	// Calc the right and bottom bounds of the original scale
	double x2 = ((double) Dx+1) * ScaleX;
	double y2 = ((double) Dy+1) * ScaleY;

	// Loop through the source pixel(s) and sum all the area
	// and RGB data
	for (double x = (double) Dx * ScaleX; x<x2; x=((int)x)+1)
	{
		for (double y = (double) Dy * ScaleY; y<y2; y=((int)y)+1)
		{
			// Get the source pixel colour
			COLOUR c = CBit(24, pSrc->Get((int)x, (int)y), pSrc->GetBits(), pPal);
			
			// Calculate bounds of source pixel
			double Fx, Fy, j;
			Fx = modf(x, &j);
			double Width = (x2 < ceil(x)) ? x2 - j : 1.0;
			Fy = modf(y, &j);
			double Height = (y2 < ceil(y)) ? y2 - j : 1.0;

			// Calculate area
			double a = (Width-Fx) * (Height-Fy);
			
			// Add the colour and area to the running total
			R += a * R24(c);
			G += a * G24(c);
			B += a * B24(c);
			Area += a;
		}
	}

	R /= Area;
	G /= Area;
	B /= Area;
	COLOUR c = Rgb24( ((int)R), ((int)G), ((int)B) );
	pDest->Colour(CBit(pDest->GetBits(), c, 24));
	pDest->Set(Dx, Dy);
*/

bool ResampleDC(GSurface *pDest, GSurface *pSrc, GRect *FromRgn, Progress *Prog)
{
	if (!pDest || !pSrc)
		return false;

	GRect Full(0, 0, pSrc->X()-1, pSrc->Y()-1), Sr;
	if (FromRgn)
	{
		Sr = *FromRgn;
		Sr.Bound(&Full);
	}
	else
	{
		Sr = Full;
	}

	double ScaleX = (double) Sr.X() / pDest->X();
	double ScaleY = (double) Sr.Y() / pDest->Y();
	int Sx = (int) (ScaleX * 256.0);
	int Sy = (int) (ScaleY * 256.0);
	int SrcBits = pSrc->GetBits();
	int OutBits = pDest->GetBits() == 32 ? 32 : 24;
	RgbLut<uint16> ToLinear(RgbLutLinear, RgbLutSRGB);
	RgbLut<uint8, 1<<16> ToSRGB(RgbLutSRGB, RgbLutLinear);

	if (Prog)
	{
		Prog->SetDescription("Resampling image...");
		Prog->SetLimits(0, pDest->Y()-1);
	}
	
	Sr.x1 <<= 8;
	Sr.y1 <<= 8;
	Sr.x2 <<= 8;
	Sr.y2 <<= 8;

	GPalette *DestPal = pDest->Palette();
	System32BitPixel pal8[256];
	if (pSrc->GetBits() <= 8)
	{
		GPalette *p = pSrc->Palette();
		for (int i=0; i<256; i++)
		{
			if (p && i < p->GetSize())
			{
				GdcRGB *rgb = (*p)[i];
				pal8[i].r = rgb->r;
				pal8[i].g = rgb->g;
				pal8[i].b = rgb->b;
			}
			else
			{
				pal8[i].r = i;
				pal8[i].g = i;
				pal8[i].b = i;
			}
			pal8[i].a = 255;
		}
	}

	// For each destination pixel
	GSurface *pAlpha = pSrc->AlphaDC();
	bool HasDestAlpha = pDest->GetBits() == 32 || pDest->GetBits() == 64;
	for (int Dy = 0; Dy<pDest->Y(); Dy++)
	{
		for (int Dx = 0; Dx<pDest->X(); Dx++)
		{
			uint64 Area = 0;
			uint64 R = 0;
			uint64 G = 0;
			uint64 B = 0;
			uint64 A = 0;
			uint64 a;

			int NextX = (Dx+1)*Sx+Sr.x1;
			int NextY = (Dy+1)*Sy+Sr.y1;
			int Nx, Ny;
			COLOUR c;

			uint16 *lin = ToLinear.Lut;
			switch (pSrc->GetBits())
			{
				case 32:
				{
					for (int y=Dy*Sy+Sr.y1; y<NextY; y=Ny)
					{
						Ny = y + (256 - (y%256));

						for (int x=Dx*Sx+Sr.x1; x<NextX; x=Nx)
						{
							Nx = x + (256 - (x%256));

							c = pSrc->Get(x >> 8, y >> 8);
							a = (Nx - x) * (Ny - y);

							// Add the colour and area to the running total
							R += a * lin[R32(c)];
							G += a * lin[G32(c)];
							B += a * lin[B32(c)];
							A += a * lin[A32(c)];
							Area += a;
						}
					}
					break;
				}
				case 8:
				{
					for (int y=Dy*Sy+Sr.y1; y<NextY; y=Ny)
					{
						Ny = y + (256 - (y%256));

						if (pAlpha)
						{
							uint8 Alpha;
							register int SrcX, SrcY;
							for (int x=Dx*Sx+Sr.x1; x<NextX; x=Nx)
							{
								Nx = x + (256 - (x%256));

								SrcX = x >> 8;
								SrcY = y >> 8;
								c = pSrc->Get(SrcX, SrcY);
								Alpha = pAlpha->Get(SrcX, SrcY);
								
								a = (Nx - x) * (Ny - y);

								// Add the colour and area to the running total
								R += a * lin[pal8[c].r];
								G += a * lin[pal8[c].g];
								B += a * lin[pal8[c].b];
								A += a * lin[Alpha];
								Area += a;
							}
						}
						else
						{
							for (int x=Dx*Sx+Sr.x1; x<NextX; x=Nx)
							{
								Nx = x + (256 - (x%256));

								c = pSrc->Get(x>>8, y>>8);
								a = (Nx - x) * (Ny - y);

								// Add the colour and area to the running total
								R += a * lin[pal8[c].r];
								G += a * lin[pal8[c].g];
								B += a * lin[pal8[c].b];
								Area += a;
							}
						}
					}
					break;
				}
				default:
				{
					for (int y=Dy*Sy+Sr.y1; y<NextY; y=Ny)
					{
						Ny = y + (256 - (y%256));

						for (int x=Dx*Sx+Sr.x1; x<NextX; x=Nx)
						{
							Nx = x + (256 - (x%256));

							c = CBit(24, pSrc->Get(x>>8, y>>8), SrcBits);
							a = (Nx - x) * (Ny - y);

							// Add the colour and area to the running total
							R += a * lin[R24(c)];
							G += a * lin[G24(c)];
							B += a * lin[B24(c)];
							Area += a;
						}
					}
					break;
				}
			}

			if (Area)
			{
				R /= Area;
				G /= Area;
				B /= Area;
				if (HasDestAlpha)
				{
					A /= Area;
					c = Rgba32( ToSRGB.Lut[R],
								ToSRGB.Lut[G],
								ToSRGB.Lut[B],
								ToSRGB.Lut[A]);
				}
				else
				{
					c = Rgb24(	ToSRGB.Lut[R],
								ToSRGB.Lut[G],
								ToSRGB.Lut[B]);
				}
			}
			else
			{
				c = 0;
			}

			if (DestPal)
			{
				c = DestPal->MatchRgb(c);
				pDest->Colour(c);
			}
			else
			{
				pDest->Colour(c, OutBits);
			}
			pDest->Set(Dx, Dy);
		}

		if (Prog)
		{
			Prog->Value(Dy);
		}
	}

	return true;
}

