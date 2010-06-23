/*hdr
**	FILE:			GMemDC.cpp
**	AUTHOR:			Matthew Allen
**	DATE:			14/10/2000
**	DESCRIPTION:	GDC v2.xx header
**
**	Copyright (C) 2000, Matthew Allen
**		fret@memecode.com
*/

#include <stdio.h>
#include <math.h>

#include "Gdc2.h"
#include "GString.h"

#define UnTranslate()		if (d->p) d->p->translate(OriginX, OriginY);
#define Translate()			if (d->p) d->p->translate(-OriginX, -OriginY);

/////////////////////////////////////////////////////////////////////////////////////////////////////
// This object redirects painter commands to a XImage...
// Because of XWindows brokeness... it's prolly gonna be dog slow.
class ImagePainter : public XPainter
{
	GMemDC *pDC;
	XFont *Font;
	int Fore, Back;
	List<GRect> Clip;
	int Ox, Oy;

public:
	ImagePainter(GMemDC *dc)
	{
		pDC = dc;
		Font = 0;
		Fore = Back = 0;
		Ox = Oy = 0;
	}
	
	~ImagePainter()
	{
		EmptyClip();
	}

	// Don't need these
	bool Begin(XWidget *w) { return pDC != 0; }
	void End() {}
	int X() { return pDC->X(); }
	int Y() { return pDC->Y(); }
	
	void setRasterOp(RowOperation i) {}
	RowOperation rasterOp() { return CopyROP; }
	void drawPoint(int x, int y) {}
	void drawLine(int x1, int y1, int x2, int y2) {}
	void drawRect(int x, int y, int wid, int height) {}
	void drawImage(int x, int y, XBitmapImage &image, int sx, int sy, int sw, int sh, XBitmapImage::BlitOp op) {}
	
	// The good Text -> XPixmap -> XImage functions
	void setFore(int c)
	{
		Fore = c;
	}
	
	void setBack(int c)
	{
		Back = c;
	}
	
	void setFont(XFont &f)
	{
		Font = &f;
	}
	
	void translate(int x, int y)
	{
		Ox += x;
		Oy += y;
	}
	
	void PushClip(int x1, int y1, int x2, int y2)
	{
		GRect *Next = NEW(GRect(Ox + x1, Oy + y1, Ox + x2, Ox + y2));
		if (Next)
		{
			GRect *Last = Clip.Last();
			if (Last)
			{
				Next->Bound(Last);
			}
			else
			{
				GRect r(0, 0, X()-1, Y()-1);
				Next->Bound(&r);
			}
			
			Clip.Insert(Last);
		}
	}
	
	void PopClip()
	{
		GRect *Last = Clip.Last();
		if (Last)
		{
			Clip.Delete(Last);
			DeleteObj(Last);
		}
	}
	
	void EmptyClip()
	{
		Clip.DeleteObjects();
	}
	
	void SetClient(GRect *Client)
	{
	}
	
	void drawText(int x, int y, char16 *text, int len, int *backColour, GRect *clip)
	{
		try
		{
			XWidget *w = XWidget::FirstWidget();
			
			x += Ox;
			y += Oy;

			if (text AND
				pDC AND
				pDC->GetBitmap() AND
				pDC->GetBitmap()->GetImage() AND
				Font AND
				w)
			{
				if (len < 0)
				{
					len = StrlenW(text);
				}

				/*

				// The general idea...
				XImage *ximage = XCreateImage();
				Pixmap pixmap = XCreatePixmap();

				XDrawString(pixmap, x, y, string, strlen(string));
				XGetImage(pixmap, x, y, width, height);

				XDestroyPixmap(pixmap);

				*/

				XFontMetrics Metrics(Font);
				int Ascent = Metrics.ascent();
				int Width = Metrics.width(text, len);
				int Height = Metrics.height();
				
				y -= Ascent;

				if (Width > 0 AND Height > 0)
				{
					// Draw any background
					if (backColour AND clip)
					{
						int Old = pDC->Colour(*backColour, 24);
						pDC->Rectangle(clip);
						pDC->Colour(Old);
					}
					
					// Create buffer for text
					Pixmap Pm = XCreatePixmap(XDisplay(), w->handle(), Width, Height, pDC->GetBits());
					if (Pm)
					{
						XftDraw *Draw = 0;

						XGCValues v;
						v.fill_style = FillSolid;
						v.foreground = CBit(GdcD->GetBits(), pDC->Colour(), pDC->GetBits());
						v.background = Back;
						GC Gc = XCreateGC(	XDisplay(),
											Pm,
											GCFillStyle | GCForeground | GCBackground,
											&v);

						XImage *Img = pDC->GetBitmap()->GetImage();
						if (Img)
						{
							// Setup img -> buf clipping
							// (XPutImage is so broken it doesn't clip itself)
							GRect Full(0, 0, pDC->X()-1, pDC->Y()-1); // The src bitmap's dimensions
							GRect *Last = Clip.Last();
							if (Last)
							{
								Full = *Last;
							}
							if (clip)
							{
								GRect c = *clip;
								c.Offset(Ox, Oy);
								Full.Bound(&c);
							}
							
							if (Full.Valid())
							{							
								GRect Src(0, 0, Width-1, Height-1);
								Src.Offset(x, y); // The area of the source bitmap we're going to use as background
								Src.Bound(&Full); // Bound the src area to the bitmaps dimensions
								if (Src.Valid()) // Check we're still overlapping the bitmap somewhere
								{
									GRect Dst = Src;
									Dst.Offset(-x, -y); // offset Dst back into Pixmap coordinates

									// Copy the valid peice of background into our temp buffer
									XPutImage(		XDisplay(),
													Pm,
													Gc,
													Img,
													Src.x1, Src.y1,
													Dst.x1, Dst.y1,
													Dst.X(), Dst.Y());

									// Draw the text into the temp buffer
									if (Font->GetTtf())
									{
										// XRender text
										Draw = XftDrawCreate(XDisplay(),
															Pm,
															GdcD->GetBits() == pDC->GetBits() ? DefaultVisual(XDisplay(), 0) : 0,
															DefaultColormap(XDisplay(), 0));
										if (Draw)
										{
											COLOUR c24 = CBit(24, pDC->Colour(), pDC->GetBits());

											XftColor Colour;
											Colour.pixel = 0;
											Colour.color.red = R24(c24) * 257;
											Colour.color.green = G24(c24) * 257;
											Colour.color.blue = B24(c24) * 257;
											Colour.color.alpha = 0xffff;

											XftDrawString32(Draw,
															&Colour,
															Font->GetTtf(),
															0, Ascent,
															(XftChar32*)text,
															len);

											if (Font->GetUnderline())
											{
												XGlyphInfo Info;
												XftTextExtentsUtf8(	XDisplay(),
																	Font->GetTtf(),
																	(XftChar8*)text,
																	len,
																	&Info);
												XDrawLine(			XDisplay(),
																	Pm,
																	Gc,
																	0,
																	Ascent + 1,
																	Info.width + Info.xOff,
																	Ascent + 1);
											}

											XftDrawDestroy(Draw);
										}
										else
										{
											printf("%s:%i - XftDrawCreate failed.\n", __FILE__, __LINE__);
										}
									}
									else
									{
										printf("%s:%i - No font.\n", __FILE__, __LINE__);
									}
									

									// Copy the temp buffer into the original XImage
									// printf("XGetSubImage buf(%i,%i) dest(%i,%i)@(%i,%i)(%i,%i)\n", Width, Height, pDC->X(), pDC->Y(), Ax, Ay, Aw, Ah);
									if (Dst.Valid())
									{
										XGetSubImage(	XDisplay(),
														Pm,
														Dst.x1, Dst.y1, Dst.X(), Dst.Y(),
														AllPlanes,
														ZPixmap,
														pDC->GetBitmap()->GetImage(),
														Src.x1, Src.y1);
										
									}
									else
									{
										printf("%s:%i - Invalid dest image.\n", __FILE__, __LINE__);
									}
								}
								else
								{
									// printf("%s:%i - Invalid src region.\n", __FILE__, __LINE__);
								}
							}
							else
							{
								// printf("%s:%i - No region.\n", __FILE__, __LINE__);
							}
						}
						else
						{
							printf("%s:%i - No XImage.\n", __FILE__, __LINE__);
						}

						XFreeGC(XDisplay(), Gc);
						XFreePixmap(XDisplay(), Pm);
					}
					else
					{
						printf("%s:%i - XCreatePixmap failed.\n", __FILE__, __LINE__);
					}
				}
			}
		}
		catch (...)
		{
			printf("drawText crashed\n");
		}
	}
};

/////////////////////////////////////////////////////////////////////////////////////////////////////
class GMemDCPrivate
{
public:
	OsBitmap	Bmp;
	Pixmap		Pix;
	Pixmap		Mask;
	ImagePainter *p;
    
    GMemDCPrivate()
    {
        Bmp = 0;
        p = 0;
        Pix = 0;
        Mask = 0;
    }

    ~GMemDCPrivate()
    {
		DeleteObj(Bmp);
		if (Pix) XFreePixmap(p->XDisplay(), Pix);
		if (Mask) XFreePixmap(p->XDisplay(), Mask);
		DeleteObj(p);
	}
};

GMemDC::GMemDC(int x, int y, int bits)
{
	d = NEW(GMemDCPrivate);
	
	if (bits > 0)
	{
		Create(x, y, bits);
	}
}

GMemDC::GMemDC(GSurface *pDC)
{
	d = NEW(GMemDCPrivate);
	
	if (pDC AND
		Create(pDC->X(), pDC->Y(), pDC->GetBits()))
	{
		Blt(0, 0, pDC);
	}
}

GMemDC::~GMemDC()
{
	DeleteObj(d);
}

XPainter *GMemDC::Handle()
{
	if (NOT d->p)
	{
		d->p = NEW(ImagePainter(this));
	}
	return d->p;
}

OsBitmap GMemDC::GetBitmap()
{
	return d->Bmp;
}

Pixmap GMemDC::GetPixmap()
{
	return d->Pix;
}

Pixmap GMemDC::GetPixmapMask()
{
	return d->Mask;
}

bool GMemDC::Lock()
{
	bool Status = false;

	// Converts a pixmap to a XImage
	if (d->Pix)
	{
		if (Create(X(), Y(), GetBits()))
		{
			XObject o;
			XGetSubImage(o.XDisplay(), d->Pix, 0, 0, X(), Y(), -1, ZPixmap, d->Bmp->GetImage(), 0, 0);
			XFreePixmap(o.XDisplay(), d->Pix);
			d->Pix = 0;

			if (d->Mask)
			{
				// Create the alpha DC from the mask Pixmap
				Ximg Mono(X(), Y(), 1);
				XGetSubImage(o.XDisplay(), d->Mask, 0, 0, X(), Y(), -1, ZPixmap, Mono, 0, 0);

				if (IsAlpha(true))
				{
					for (int y=0; y<Y(); y++)
					{
						uchar *a = (*AlphaDC())[y];
						for (int x=0; x<X(); x++)
						{
							a[x] = Mono.Get(x, y) ? 0xff : 0;
						}
					}
				}
				
				XFreePixmap(o.XDisplay(), d->Mask);
				d->Mask = 0;
			}
			
			Status = true;
		}
	}
	
	return Status;
}

bool GMemDC::Unlock()
{
	bool Status = false;

	// Converts the XImage to a pixmap
	XWidget *w = XWidget::FirstWidget();
	if (w AND d->Bmp)
	{
		Display *Dsp = d->Bmp->XDisplay();
		XImage *Img = d->Bmp->GetImage();
		
		GSurface *Alpha = AlphaDC();
		if (Alpha)
		{
			Ximg Msk(Alpha->X(), Alpha->Y(), 1);
			for (int y=0; y<Alpha->Y(); y++)
			{
				uchar *a = (*Alpha)[y];
				for (int x=0; x<Alpha->X(); x++)
				{
					Msk.Set(x, y, a[x] ? 1 : 0);
				}
			}

			Xpix Pix(w->handle(), &Msk);
			if ((Pixmap)Pix)
			{
				d->Mask = Pix;
				Pix.Detach();
			}
			else
			{
				printf("%s:%i - No pixmap.\n", __FILE__, __LINE__);
			}
		}
		
		d->Pix = XCreatePixmap(Dsp, w->handle(), X(), Y(), GetBits());
		if (d->Pix)
		{
			XGCValues v;
			GC Gc = XCreateGC(Dsp, d->Pix, 0, &v);
			if (Gc)
			{
				XPutImage(Dsp, d->Pix, Gc, Img, 0, 0, 0, 0, X(), Y());
				XFreeGC(Dsp, Gc);				
			}
		}
		
		if (d->Pix)
		{
			DeleteObj(d->Bmp);
			
			for (int i=0; i<GDC_CACHE_SIZE; i++)
			{
				if (pAppCache[i] AND pAppCache[i] == pApp) pApp = 0;
				DeleteObj(pAppCache[i]);
			}
			DeleteObj(pApp);
			
			pMem->Base = 0;
			
			Status = true;
		}
	}

	return Status;
}

uchar *GMemDC::operator[](int y)
{
	if (d->Bmp AND
		pMem AND
		pMem->Base AND
		y >= 0 AND
		y < pMem->y)
	{
		return pMem->Base + (pMem->Line * y);
	}
	
	return 0;
}

void GMemDC::SetOrigin(int x, int y)
{
	Handle();
	UnTranslate();
	GSurface::SetOrigin(x, y);
	Translate();
}

bool GMemDC::Create(int x, int y, int Bits, int LineLen, bool KeepData)
{
	DeleteObj(d->Bmp);

	if (x < 1 OR y < 1 OR Bits < 1) return false;
	if (Bits < 8) Bits = 8;

	d->Bmp = NEW(XBitmapImage);
	if (d->Bmp AND
		d->Bmp->create(x, y, Bits))
	{
		if (NOT pMem) pMem = NEW(GBmpMem);
		if (pMem)
		{
			pMem->Base = d->Bmp->scanLine(0);
			pMem->x = x;
			pMem->y = y;
			pMem->Bits = d->Bmp->getBits();
			pMem->Line = d->Bmp->bytesPerLine();
			pMem->Flags = 0;

			int NewOp = (pApp) ? Op() : GDC_SET;

			if (	(Flags & GDC_OWN_APPLICATOR) AND
				NOT (Flags & GDC_CACHED_APPLICATOR))
			{
				DeleteObj(pApp);
			}

			for (int i=0; i<GDC_CACHE_SIZE; i++)
			{
				DeleteObj(pAppCache[i]);
			}

			if (NewOp < GDC_CACHE_SIZE AND NOT DrawOnAlpha())
			{
				pApp = (pAppCache[NewOp]) ? pAppCache[NewOp] : pAppCache[NewOp] = CreateApplicator(NewOp);
				Flags &= ~GDC_OWN_APPLICATOR;
				Flags |= GDC_CACHED_APPLICATOR;
			}
			else
			{
				pApp = CreateApplicator(NewOp, pMem->Bits);
				Flags &= ~GDC_CACHED_APPLICATOR;
				Flags |= GDC_OWN_APPLICATOR;
			}

			if (NOT pApp)
			{
				printf("GMemDC::Create(%i,%i,%i,%i,%i) No Applicator.\n", x, y, Bits, LineLen, KeepData);
				LgiAssert(0);
			}

			Clip.ZOff(X()-1, Y()-1);

			return true;
		}
	}
	else
	{
		printf("Error: GMemDC::Create(%i,%i,%i) failed\n", x, y, Bits);
	}

	return false;
}


void GMemDC::Blt(int x, int y, GSurface *Src, GRect *a)
{
	if (Src)
	{
		if (Src->IsScreen())
		{
			// Screen -> Memory
			bool Status = false;
			GScreenDC *Screen = dynamic_cast<GScreenDC*>(Src);
			if (Screen)
			{
				XWidget *Wnd = Screen->Handle()->Handle();
				GRect *Client = Screen->Handle()->GetClient();

				GRect SrcRect(0, 0, Src->X()-1, Src->Y()-1);
				if (Client)
				{
					SrcRect = *Client;
				}
				
				GRect s;
				if (a)
				{
					s = *a;
					s.Offset(SrcRect.x1, SrcRect.y1);
				}
				else
				{
					s = SrcRect;
				}
				
				GRect b;
				if (a)
				{
					b = *a;
					b.Offset(SrcRect.x1, SrcRect.y1);
					b.Bound(&SrcRect);
				}
				else				
				{
					b = SrcRect;
				}
				
				if (b.Valid())
				{
					GRect Dst, e(0, 0, X()-1, Y()-1);
					Dst.ZOff(b.X()-1, b.Y()-1);
					Dst.Offset(x, y);
					Dst.Bound(&e);
					if (Dst.Valid())
					{
						// Clip the input rectangle against the size of the dest
						if (b.X() > Dst.X())
						{
							b.x2 = b.x1 + Dst.X() - 1;
						}
						if (b.Y() > Dst.Y())
						{
							b.y2 = b.y1 + Dst.Y() - 1;
						}
						
						Display *Dsp = Handle()->XDisplay();
						if (d->Bmp)
						{
							// Screen -> Memory
							XImage *Img = d->Bmp->GetImage();
							if (Img)
							{
								XGetSubImage(	Dsp,
												Wnd->handle(),
												b.x1, b.y1,
												b.X(), b.Y(),
												-1,
												ZPixmap,
												GetBitmap()->GetImage(),
												b.x1-s.x1, b.y1-s.y1);
								
								Status = true;
							}
						}
						else if (d->Pix)
						{
							// Screen -> Pixmap
						}
					}
				}
			}
			
			if (NOT Status)
			{
				Colour(Rgb24(255, 0, 255), 24);
				Rectangle();
			}
		}
		else
		{
			GMemDC *SrcMem = dynamic_cast<GMemDC*>(Src);

			if (d->Bmp)
			{
				if ((*Src)[0])
				{
					// Memory -> Memory (Source alpha used)
					GSurface::Blt(x, y, Src, a);
				}
				else if (SrcMem)
				{
					if (SrcMem->d->Pix)
					{
						// Pixmap -> Memory
						GRect All;
						if (NOT a)
						{
							All.ZOff(Src->X()-1, Src->Y()-1);
							a = &All;
						}
						
						Display *Dsp = Handle()->XDisplay();
						
						if (SrcMem->d->Mask)
						{
							// Has an alpha channel... this is unbelievably slow
							// but it works. There isn't really any way around this
							// because XGetSubImage can't take a mask pixmap as an
							// argument. However as a nice side effect of doing it
							// this way we get the full bit depth conversion capability
							// of GDC working for us.
							GMemDC Temp(a->X(), a->Y(), GdcD->GetBits());

							// Copy the pixels into a temporary buffer
							XGetSubImage(	Dsp,
											SrcMem->d->Pix,
											a->x1, a->y1,
											a->X(), a->Y(),
											-1,
											ZPixmap,
											Temp.d->Bmp->GetImage(),
											0, 0);

							if (Temp.IsAlpha(true))
							{
								// Copy the alpha channel into the temp buffer
								Ximg Mask(a->X(), a->Y(), 1);
								XGetSubImage(	Dsp,
												SrcMem->d->Mask,
												a->x1, a->y1,
												a->X(), a->Y(),
												-1,
												ZPixmap,
												Mask,
												0, 0);
								for (int y=0; y<a->Y(); y++)
								{
									uchar *d = (*Temp.AlphaDC())[y];
									for (int x=0; x<a->X(); x++)
									{
										d[x] = Mask.Get(x, y) ? 0xff : 0;
									}
								}
							}
							else
							{
								printf("%s:%i - couldn't alloc alpha channel.\n", __FILE__, __LINE__);
							}

							// Do the blt
							int Old = Op(GDC_ALPHA);
							GSurface::Blt(x, y, &Temp);
							Op(Old);
						}
						else
						{
							XGetSubImage(	Dsp,
											SrcMem->d->Pix,
											a->x1, a->y1,
											a->X(), a->Y(),
											-1,
											ZPixmap,
											d->Bmp->GetImage(),
											x, y);
						}											
					}
					else
					{
						printf("%s:%i - Error.\n", __FILE__, __LINE__);
					}
				}
				else
				{
					printf("%s:%i - Error.\n", __FILE__, __LINE__);
				}
			}
			else if (d->Pix)
			{
				if (SrcMem)
				{
					if (SrcMem->d->Bmp)
					{
						// Memory -> Pixmap
						XBitmapImage *SImg = SrcMem->d->Bmp;
						if (SImg)
						{
							printf("%s:%i - Not implemented.\n",
								__FILE__, __LINE__);
						}
						else
						{
							printf("%s:%i - Error.\n", __FILE__, __LINE__);
						}
					}
					else if (SrcMem->d->Pix)
					{
						// Pixmap -> Pixmap
						Pixmap SPix = SrcMem->d->Pix;
						if (SPix)
						{
							printf("%s:%i - Not implemented.\n",
								__FILE__, __LINE__);
						}
						else
						{
							printf("%s:%i - Error.\n", __FILE__, __LINE__);
						}
					}
					else
					{
						printf("%s:%i - Error.\n", __FILE__, __LINE__);
					}
				}
				else
				{
					printf("%s:%i - Error.\n", __FILE__, __LINE__);
				}
			}
			else
			{
				printf("%s:%i - Error.\n", __FILE__, __LINE__);
			}
		}
	}
}

void GMemDC::StretchBlt(GRect *d, GSurface *Src, GRect *s)
{

}

void GMemDC::HLine(int x1, int x2, int y, COLOUR a, COLOUR b)
{
	if (x1 > x2) LgiSwap(x1, x2);

	if (x1 < Clip.x1) x1 = Clip.x1;
	if (x2 > Clip.x2) x2 = Clip.x2;
	if (x1 <= x2 AND
		y >= Clip.y1 AND
		y <= Clip.y2 AND
		pApp)
	{
		COLOUR Prev = pApp->c;

		pApp->SetPtr(x1, y);
		for (; x1 <= x2; x1++)
		{
			if (x1 & 1)
			{
				pApp->c = a;
			}
			else
			{
				pApp->c = b;
			}

			pApp->Set();
			pApp->IncX();
		}

		pApp->c = Prev;
	}
}

void GMemDC::VLine(int x, int y1, int y2, COLOUR a, COLOUR b)
{
	if (y1 > y2) LgiSwap(y1, y2);
	
	if (y1 < Clip.y1) y1 = Clip.y1;
	if (y2 > Clip.y2) y2 = Clip.y2;
	if (y1 <= y2 AND
		x >= Clip.x1 AND
		x <= Clip.x2 AND
		pApp)
	{
		COLOUR Prev = pApp->c;

		pApp->SetPtr(x, y1);
		for (; y1 <= y2; y1++)
		{
			if (y1 & 1)
			{
				pApp->c = a;
			}
			else
			{
				pApp->c = b;
			}

			pApp->Set();
			pApp->IncY();
		}

		pApp->c = Prev;
	}
}
