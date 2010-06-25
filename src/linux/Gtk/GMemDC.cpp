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
using namespace Gtk;

#define UnTranslate()		// if (d->p) d->p->translate(OriginX, OriginY);
#define Translate()			// if (d->p) d->p->translate(-OriginX, -OriginY);

/////////////////////////////////////////////////////////////////////////////////////////////////////
// This object redirects painter commands to a XImage...
// Because of XWindows brokeness... it's prolly gonna be dog slow.
class ImagePainter
{
	GMemDC *pDC;
	GFont *Font;
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
	//bool Begin(XWidget *w) { return pDC != 0; }
	void End() {}
	int X() { return pDC->X(); }
	int Y() { return pDC->Y(); }
	
	// void setRasterOp(RowOperation i) {}
	// RowOperation rasterOp() { return CopyROP; }
	void drawPoint(int x, int y) {}
	void drawLine(int x1, int y1, int x2, int y2) {}
	void drawRect(int x, int y, int wid, int height) {}
	// void drawImage(int x, int y, XBitmapImage &image, int sx, int sy, int sw, int sh, XBitmapImage::BlitOp op) {}
	
	// The good Text -> XPixmap -> XImage functions
	void setFore(int c)
	{
		Fore = c;
	}
	
	void setBack(int c)
	{
		Back = c;
	}
	
	void setFont(GFont *f)
	{
		Font = f;
	}
	
	void translate(int x, int y)
	{
		Ox += x;
		Oy += y;
	}
	
	void PushClip(int x1, int y1, int x2, int y2)
	{
		GRect *Next = new GRect(Ox + x1, Oy + y1, Ox + x2, Ox + y2);
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
				/*

				// The general idea...
				XImage *ximage = XCreateImage();
				Pixmap pixmap = XCreatePixmap();

				XDrawString(pixmap, x, y, string, strlen(string));
				XGetImage(pixmap, x, y, width, height);

				XDestroyPixmap(pixmap);

				*/
		
		/*
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
		*/
	}
};

/////////////////////////////////////////////////////////////////////////////////////////////////////
#define ROUND_UP(bits) (((bits) + 7) / 8)

class GMemDCPrivate
{
public:
	GRect Client;
	GdkImage *Img;

    GMemDCPrivate()
    {
		Client.ZOff(-1, -1);
		Img = 0;
    }

    ~GMemDCPrivate()
    {
		if (Img)
		{
			g_object_unref(Img);
		}
	}

	bool ConvertToPix()
	{
		/*
		if (!Bmp)
			return false;
		
		DeleteObj(Pix);
		if (Pix = new GPixmap(Bmp->X(), Bmp->Y(), Bmp->GetBits()))
		{
			xcb_gcontext_t gc = xcb_generate_id(XcbConn());
			xcb_create_gc (XcbConn(), gc, Pix->Handle(), 0, 0);
			xcb_void_cookie_t c =
				xcb_put_image_checked(	XcbConn(),
										XCB_IMAGE_FORMAT_Z_PIXMAP,
										Pix->Handle(),
										gc,
										Bmp->X(),
										Bmp->Y(),
										0,
										0,
										0, //left_pad?
										Bmp->GetBits(), //depth?
										Bmp->Sizeof(),
										Bmp->Addr(0));
			if (!XcbCheck(c))
			{
				if (XcbScreen()->root_depth != Bmp->GetBits())
				{
					printf("\tBit depths different: screen=%i\n", XcbScreen()->root_depth);
				}
				
				printf("\tParams=%i,%i,%i %i,%i,%p\n",
					Bmp->X(), Bmp->Y(), Bmp->GetBits(),
					Bmp->GetLine(), Bmp->Sizeof(), Bmp->Addr(0));
			}
			
			DeleteObj(Bmp);
			xcb_free_gc(XcbConn(), gc);
		}
		else return 0;
		*/
		
		return true;
	}

	bool ConvertToBmp()
	{
		/*
		if (!Pix)
			return false;
		
		DeleteObj(Bmp);
		if (Bmp = new GXBitmap(Pix->X(), Pix->Y(), Pix->GetBits()))
		{
			xcb_get_image(XcbConn(), XCB_IMAGE_FORMAT_Z_PIXMAP, Pix->Handle(), 0, 0, Pix->X(), Pix->Y(), 0);
			DeleteObj(Pix);
		}
		else return false;
		
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
		*/
		
		return true;
	}
};

GMemDC::GMemDC(int x, int y, int bits)
{
	d = new GMemDCPrivate;
	if (bits > 0)
		Create(x, y, bits);
}

GMemDC::GMemDC(GSurface *pDC)
{
	d = new GMemDCPrivate;
	
	if (pDC &&
		Create(pDC->X(), pDC->Y(), pDC->GetBits()))
	{
		Blt(0, 0, pDC);
	}
}

GMemDC::~GMemDC()
{
	Empty();
	DeleteObj(d);
}

GdkImage *GMemDC::GetImage()
{
    return d->Img;
}

GdcPt2 GMemDC::GetSize()
{
	return GdcPt2(pMem->x, pMem->y);
}

void GMemDC::SetClient(GRect *c)
{
	if (c)
	{
		d->Client = *c;
		OriginX = -c->x1;
		OriginY = -c->y1;
	}
	else
	{
		d->Client.ZOff(-1, -1);
		OriginX = 0;
		OriginY = 0;
	}
}

void GMemDC::Empty()
{
	if (d->Img)
	{
		g_object_unref(d->Img);
		d->Img = 0;
	}
}

bool GMemDC::Lock()
{
	bool Status = false;

	/*
	// Converts a server pixmap to a local bitmap
	if (d->Pix)
	{
		Status = d->ConvertToBmp();
		pMem->Base = d->Bmp->Addr(0);

		int NewOp = (pApp) ? Op() : GDC_SET;

		if ( (Flags & GDC_OWN_APPLICATOR) &&
			!(Flags & GDC_CACHED_APPLICATOR))
		{
			DeleteObj(pApp);
		}

		for (int i=0; i<GDC_CACHE_SIZE; i++)
		{
			DeleteObj(pAppCache[i]);
		}

		if (NewOp < GDC_CACHE_SIZE && !DrawOnAlpha())
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

		LgiAssert(pApp);
	}
	*/
	
	return Status;
}

bool GMemDC::Unlock()
{
	bool Status = false;

	/*
	// Converts the local image to a server pixmap
	if (d->Bmp)
	{
		Status = d->ConvertToPix();
		
		if (Status)
		{
			for (int i=0; i<GDC_CACHE_SIZE; i++)
			{
				if (pAppCache[i] && pAppCache[i] == pApp) pApp = 0;
				DeleteObj(pAppCache[i]);
			}
			DeleteObj(pApp);
			
			pMem->Base = 0;
			
			Status = true;
		}
	}
	*/

	return Status;
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
	if (x < 1 OR y < 1 OR Bits < 1) return false;
	if (Bits < 8) Bits = 8;

	Empty();
	
	GdkVisual Vis = *gdk_visual_get_system();
	if (Bits == 8)
	{
	    Vis.type = GDK_VISUAL_DIRECT_COLOR;
	    Vis.depth = 8;
	}
	
	d->Img = gdk_image_new(	GDK_IMAGE_FASTEST,
							&Vis,
							x,
							y);
	if (!d->Img)
	{
		LgiTrace("%s:%i - Error: failed to create image (%i,%i,%i)\n", _FL, x, y, Bits);
		return false;
	}
	
	if (!pMem)
		pMem = new GBmpMem;
	if (!pMem)
		return false;

	pMem->x = x;
	pMem->y = y;
	pMem->Bits = d->Img->bits_per_pixel;
	pMem->Line = d->Img->bpl;
	pMem->Flags = 0;
	pMem->Base = (uchar*)d->Img->mem;

	printf("Created gdk image %ix%i @ %i bpp line=%i (%i) ptr=%p Vis=%p\n",
		pMem->x,
		pMem->y,
		pMem->Bits,
		pMem->Line,
		pMem->Bits*pMem->x/8,
		pMem->Base,
		Vis);

	int NewOp = (pApp) ? Op() : GDC_SET;

	if ( (Flags & GDC_OWN_APPLICATOR) &&
		!(Flags & GDC_CACHED_APPLICATOR))
	{
		DeleteObj(pApp);
	}

	for (int i=0; i<GDC_CACHE_SIZE; i++)
	{
		DeleteObj(pAppCache[i]);
	}

	if (NewOp < GDC_CACHE_SIZE && !DrawOnAlpha())
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

	if (!pApp)
	{
		printf("GMemDC::Create(%i,%i,%i,%i,%i) No Applicator.\n", x, y, Bits, LineLen, KeepData);
		LgiAssert(0);
	}

	Clip.ZOff(X()-1, Y()-1);

	return true;
}

void GMemDC::Blt(int x, int y, GSurface *Src, GRect *a)
{
	if (!Src)
		return;

	bool Status = false;
	GBlitRegions br(this, x, y, Src, a);
	if (!br.Valid())
		return;

	GScreenDC *Screen;
	if (Screen = Src->IsScreen())
	{
		if (pMem->Base)
		{
			// Screen -> Memory
			Status = true;
		}
		
		if (!Status)
		{
			Colour(Rgb24(255, 0, 255), 24);
			Rectangle();
		}
	}
	else if ((*Src)[0])
	{
		// Memory -> Memory (Source alpha used)
		GSurface::Blt(x, y, Src, a);
	}
}

void GMemDC::StretchBlt(GRect *d, GSurface *Src, GRect *s)
{
    LgiAssert(!"Not implemented");
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
