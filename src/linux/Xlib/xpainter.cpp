#include "Gdc2.h"
#include "xpainter.h"
#include "xfont.h"
#include "LgiCommon.h"
#include "GString.h"

#define TheVisual	DefaultVisual(XDisplay(), 0)
#define TheCmap		DefaultColormap(XDisplay(), 0)

class XClip
{
public:
	Region		Rgn;
	XRectangle	Rect;
	bool		System;
	
	XClip()
	{
		Rgn = 0;
		System = false;
	}
	
	~XClip()
	{
		XDestroyRegion(Rgn);
	}
};

class GPainterPrivate : public XObject
{
	// Pixmap buffer
	Pixmap Buf;
	GC BufGc;
	XftDraw *BufDraw;
	int BufX, BufY;

public:
	XWidget *Widget;
	GC Gc;
	int Ox, Oy;
	int x, y;
	
	Colormap Cmap;
	XVisualInfo Info;
	int Fore, Back;
	XftColor *ForeCol, *BackCol;
	XftDraw *Draw;
	XftFont *Font;
	int Ascent;
	bool Underline;
	GRect Client;
	
	// Clip stack
	List<XClip> Clip; 
	
	GPainterPrivate()
	{
		Widget = 0;
		x = y = Ox = Oy = 0;
		Gc = 0;
		Draw = 0;
		Font = 0;
		Fore = Back = -1;
		ForeCol = BackCol = 0;
		Buf = 0;
		BufGc = 0;
		BufX = BufY = 0;
		BufDraw = 0;
	}

	~GPainterPrivate()
	{
		FreeBuf();
		
		Clip.DeleteObjects();
		if (Draw)
		{
			XftDrawDestroy(Draw);
		}
	}
	
	void SetClip(XClip *c)
	{
		if (c)
		{
			// Set area
			XftDrawSetClip(Draw, c->Rgn);
			XSetRegion(Widget->XDisplay(), Gc, c->Rgn);
		}
	}

	void FreeBuf()
	{
		if (BufDraw)
		{
			XftDrawDestroy(BufDraw);
			BufDraw = 0;
		}
		if (BufGc)
		{
			XFreeGC(XDisplay(), BufGc);
			BufGc = 0;
		}
		if (Buf)
		{
			XFreePixmap(XDisplay(), Buf);
			Buf = 0;
		}
	}
	
	Pixmap GetBuf(int x, int y, int Colour, GC *pGc, XftDraw **pDraw)
	{
		int Depth = DefaultDepth(XDisplay(), 0);
		if (NOT Buf OR x > BufX OR y > BufY)
		{
			// Recreate
			FreeBuf();
			
			BufX = max(x + 2, BufX);
			BufY = max(y + 2, BufY);			
			
			Buf = XCreatePixmap(XDisplay(), Widget->handle(), BufX, BufY, Depth);
			if (Buf)
			{
				if (pGc)
				{
					XGCValues v;
					v.foreground = CBit(Depth, Colour, 24);
					*pGc = BufGc = XCreateGC(XDisplay(), Buf, GCForeground, &v);
				}
				
				if (pDraw)
				{
					*pDraw = BufDraw = XftDrawCreate(	XDisplay(),
														Buf,
														TheVisual,
														TheCmap);
				}
			}
		}
		else
		{
			if (pGc)
			{
				XGCValues v;
				v.foreground = CBit(Depth, Colour, 24);
				XChangeGC(XDisplay(), BufGc, GCForeground, &v);
				
				*pGc = BufGc;
			}
				
			if (pDraw)
			{
				*pDraw = BufDraw;
			}
		}
		
		return Buf;
	}
};

XPainter::XPainter()
{
	d = NEW(GPainterPrivate);
}

XPainter::~XPainter()
{
	DeleteObj(d);
}

XWidget *XPainter::Handle()
{
	return d->Widget;
}

GRect *XPainter::GetClient()
{
	return d->Client.Valid() ? &d->Client : 0;
}

int XPainter::X()
{
	return d->Client.Valid() ? d->Client.X() : d->x;
}

int XPainter::Y()
{
	return d->Client.Valid() ? d->Client.Y() : d->y;
}

bool XPainter::IsOk()
{
	bool Status =	this != 0 AND
					d != 0 AND
					d->Widget != 0;
	if (NOT Status)
	{
		#ifdef _DEBUG
		LgiTrace("XPainter::IsOk() failed. this=%p d=%p d->W=%p\n",
			this,
			d,
			d ? d->Widget : 0);
			
		exit(-1);
		#endif
		
		LgiAssert(Status);
	}
	return Status;
}

bool XPainter::Begin(XWidget *w)
{
	d->Widget = w;
	if (IsOk())
	{
		d->Client.ZOff(-1, -1);
		d->x = w->width();
		d->y = w->height();
		
		d->Draw = XftDrawCreate(XDisplay(),
								d->Widget->handle(),
								TheVisual,
								TheCmap);
		if (NOT d->Draw)
		{
			printf("XPainter::begin, XftDrawCreate failed..\n");
		}

		XGCValues v;
		v.fill_style = FillSolid;
		d->Gc = XCreateGC(XDisplay(), d->Widget->handle(), GCFillStyle, &v);
		if (d->Gc)
		{
			GRegion *Clip = d->Widget->_GetClipRgn();
			if (Clip)
			{
				XClip *c = NEW(XClip);
				if (c)
				{
					c->System = true;
					c->Rgn = XCreateRegion();
					for (int i=0; i<Clip->Length(); i++)
					{
						// printf("\t[i]=%i,%i,%i,%i\n", Rect[i].x, Rect[i].y, Rect[i].width, Rect[i].height);
						GRect *g = (*Clip)[i];
						XRectangle x;
						x.x = g->x1;
						x.y = g->y1;
						x.width = g->X();
						x.height = g->Y();						
						XUnionRectWithRegion(&x, c->Rgn, c->Rgn);
					}
					XClipBox(c->Rgn, &c->Rect);
					
					d->SetClip(c);
					d->Clip.Insert(c);
				}
			}
		}

		return true;
	}

	return false;
}

void XPainter::End()
{
	if (IsOk())
	{
		// XftDrawDestroy(d->Draw);
		XFreeColormap(XDisplay(), TheCmap);		
		XFreeGC(XDisplay(), d->Gc);
		d->Widget = 0;
		d->Gc = 0;
	}
}

void XPainter::translate(int x, int y)
{
	if (IsOk())
	{
		d->Ox += x;
		d->Oy += y;
	}
}

void XPainter::PushClip(int x1, int y1, int x2, int y2)
{
	if (IsOk() AND d->Gc)
	{
		XClip *l = d->Clip.Last();
		if (l)
		{
			XClip *c = NEW(XClip);
			if (c)
			{
				c->System = false;
				
				// Create new area
				c->Rect.x = x1 + d->Ox - d->Client.x1;
				c->Rect.y = y1 + d->Oy - d->Client.y1;
				if (d->Client.Valid())
				{
					c->Rect.x += d->Client.x1;
					c->Rect.y += d->Client.y1;
				}
				c->Rect.width = x2 - x1 + 1;
				c->Rect.height = y2 - y1 + 1;
				c->Rgn = XCreateRegion();
				XUnionRectWithRegion(&c->Rect, c->Rgn, c->Rgn);
				XIntersectRegion(c->Rgn, l->Rgn, c->Rgn);
		
				d->Clip.Insert(c);
				d->SetClip(c);
			}
		}
	}
}

void XPainter::PopClip()
{
	if (IsOk() AND d->Gc)
	{
		XClip *l = d->Clip.Last();
		if (l AND NOT l->System)
		{
			d->Clip.Delete(l);
			DeleteObj(l);

			l = d->Clip.Last();
			if (l)
			{
				d->SetClip(l);
			}			
		}
	}
}

void XPainter::EmptyClip()
{
	if (IsOk() AND d->Gc)
	{
		XClip *l = d->Clip.Last();
		while (l AND NOT l->System)
		{
			d->Clip.Delete(l);
			DeleteObj(l);
		}

		l = d->Clip.Last();
		if (l)
		{
			d->SetClip(l);
		}			
	}
}

void XPainter::SetClient(GRect *Client)
{
	if (IsOk() AND d->Gc)
	{
		if (Client)
		{
			// Clip output to the client area
			PushClip(Client->x1, Client->y1, Client->x2, Client->y2);
			XClip *c = d->Clip.Last();
			if (c) c->System = true;			

			d->Client.Set(Client->x1, Client->y1, Client->x2, Client->y2);
			translate(Client->x1, Client->y1);
		}
		else
		{
			translate(-d->Client.x1, -d->Client.y1);
			d->Client.ZOff(-1, -1);
			
			// Reset clip to empty
			XClip *l = d->Clip.Last();
			while (l AND d->Clip.Length() > 1)
			{
				d->Clip.Delete(l);
				DeleteObj(l);
			}
	
			l = d->Clip.Last();
			if (l)
			{
				d->SetClip(l);
			}			
		}
	}
}

void XPainter::setFore(int c)
{
	if (IsOk() AND d->Gc)
	{
		if (d->ForeCol)
		{
			XftColorFree(XDisplay(), TheVisual, TheCmap, d->ForeCol);
			DeleteObj(d->ForeCol);
		}

		XGCValues v;
		v.foreground = d->Fore = c;
		XChangeGC(XDisplay(), d->Gc, GCForeground, &v);
	}
}

void XPainter::setBack(int c)
{
	if (IsOk() AND d->Gc)
	{
		XGCValues v;
		v.background = d->Back = c;
		XChangeGC(XDisplay(), d->Gc, GCBackground, &v);
	}
}

void XPainter::setRasterOp(RowOperation i)
{
}

XPainter::RowOperation XPainter::rasterOp()
{
	return CopyROP;
}

void XPainter::setFont(XFont &f)
{
	if (IsOk() AND d->Gc)
	{
		d->Font = f.GetTtf();
		d->Underline = f.GetUnderline();
		if (NOT d->Font AND f.GetFont())
		{
			XGCValues v;
			v.font = f.GetFont();
			XChangeGC(XDisplay(), d->Gc, GCFont, &v);
		}
	
		XFontMetrics m(&f);
		d->Ascent = m.ascent();
	}
}

void XPainter::drawPoint(int x, int y)
{
	if (IsOk() AND d->Gc)
	{
		XDrawPoint(XDisplay(), d->Widget->handle(), d->Gc, d->Ox + x, d->Oy + y);
	}
}

void XPainter::drawLine(int x1, int y1, int x2, int y2)
{
	if (IsOk() AND d->Gc)
	{
		XDrawLine(	XDisplay(),
					d->Widget->handle(),
					d->Gc,
					d->Ox + x1,
					d->Oy + y1,
					d->Ox + x2,
					d->Oy + y2);
	}
}

void XPainter::drawRect(int x, int y, int wid, int height)
{
	if (IsOk() AND d->Gc)
	{
		XFillRectangle(	XDisplay(),
						d->Widget->handle(),
						d->Gc,
						d->Ox + x,
						d->Oy + y,
						wid,
						height);
	}
}

void XPainter::drawArc(double cx, double cy, double radius)
{
	if (IsOk() AND d->Gc)
	{
		XDrawArc(	XDisplay(),
					d->Widget->handle(),
					d->Gc,
					cx - radius,
					cy - radius,
					radius * 2,
					radius * 2,
					0,
					360 * 64);
	}
}

void XPainter::drawArc(double cx, double cy, double radius, double start, double end)
{
	if (IsOk() AND d->Gc)
	{
		XDrawArc(	XDisplay(),
					d->Widget->handle(),
					d->Gc,
					cx - radius,
					cy - radius,
					radius * 2,
					radius * 2,
					start * 64,
					end * 64);
	}
}

void XPainter::fillArc(double cx, double cy, double radius)
{
	if (IsOk() AND d->Gc)
	{
		XFillArc(	XDisplay(),
					d->Widget->handle(),
					d->Gc,
					cx - radius,
					cy - radius,
					radius * 2,
					radius * 2,
					0,
					360 * 64);
	}
}

void XPainter::fillArc(double cx, double cy, double radius, double start, double end)
{
	if (IsOk() AND d->Gc)
	{
		XFillArc(	XDisplay(),
					d->Widget->handle(),
					d->Gc,
					cx - radius,
					cy - radius,
					radius * 2,
					radius * 2,
					start * 64,
					end * 64);
	}
}

void XPainter::drawImage(int x, int y, XBitmapImage &image, int sx, int sy, int sw, int sh, XBitmapImage::BlitOp op)
{
	if (d->Widget AND d->Gc AND image.Img)
	{
		if (image.Img->depth == GdcD->GetBits())
		{
			int e = XPutImage(	XDisplay(),
								d->Widget->handle(),
								d->Gc,
								image.Img,
								sx, sy,
								d->Ox + x, d->Oy + y,
								sw, sh);
			if (e)
			{
				printf("XPainter::drawImage, XPutImage failed (%s)\n", XErr(e));
			}
		}
		else
		{
			printf("Error: Can't put XImage (depth: %i) onto Screen (depth: %i)\n", image.Img->depth, GdcD->GetBits());
		}
	}
	else
	{
		printf("XPainter::drawImage, bad params\n");
	}
}

void XPainter::drawText(int x, int y, char16 *text, int len, int *backColour, GRect *clip)
{
	if (d->Widget AND d->Gc AND text)
	{
		GRegion *Exposed = d->Widget->_GetClipRgn();
		if (clip AND Exposed)
		{
			GRect Clip = *clip;
			Clip.Offset(d->Ox, d->Oy);
			if (NOT Exposed->Overlap(&Clip))
			{
				return;
			}
		}		
		
		if (d->Draw AND d->Font)
		{
			if (NOT d->ForeCol)
			{
				d->ForeCol = NEW(XftColor);
				if (d->ForeCol)
				{
					XRenderColor c;
					int DisplayDepth = DefaultDepth(XDisplay(), 0);

					COLOUR c24 = CBit(24, d->Fore, DisplayDepth);
					c.red = R24(c24) * 257;
					c.green = G24(c24) * 257;
					c.blue = B24(c24) * 257;
					c.alpha = 65535;
					
					if (NOT XftColorAllocValue(XDisplay(), TheVisual, TheCmap, &c, d->ForeCol))
					{
						printf("Allocing colour failed: %i,%i,%i...\n", c.red, c.green, c.blue);
						DeleteObj(d->ForeCol);
					}
				}
			}

			if (d->ForeCol)
			{
				if (clip)
				{
					PushClip(clip->x1, clip->y1, clip->x2, clip->y2);
				}

				// Pixmap buffer code
				bool Done = false;
				
				if (clip AND backColour AND d->ForeCol)
				{
					GC Gc = 0;
					XftDraw *Draw = 0;
					
					Pixmap Buf = d->GetBuf(clip->X(), clip->Y(), *backColour, &Gc, &Draw);
					if (Buf AND Gc AND Draw)
					{
						XFillRectangle(XDisplay(), Buf, Gc, 0, 0, clip->X(), clip->Y());

						if (len < 0) len = StrlenW(text);
						if (len)
						{
							XftDrawString32(	Draw,
												d->ForeCol,
												d->Font,
												x - clip->x1,
												y - clip->y1,
												(XftChar32*)text,
												len);
												
							if (d->Underline)
							{
								XGlyphInfo Info;
								XftTextExtents32(XDisplay(), d->Font, (XftChar32*)text, len, &Info);
								
								XGCValues v;
								v.foreground = d->Fore;
								XChangeGC(XDisplay(), Gc, GCForeground, &v);
								
								XDrawLine(	XDisplay(),
											Buf,
											Gc,
											0,
											y - clip->y1 + 1,
											Info.width + Info.xOff,
											y - clip->y1 + 1);
							}

							XCopyArea(	XDisplay(),
										Buf,
										d->Widget->handle(),
										d->Gc,
										0, 0,
										clip->X(), clip->Y(),
										d->Ox + clip->x1, d->Oy + clip->y1);

							Done = true;
						}
					}
				}
				
				if (NOT Done)
				{
					// Old straight to screen code
					
					// Draw the background fill
					if (backColour AND clip)
					{
						XGCValues v;
						v.foreground = *backColour;
						XChangeGC(XDisplay(), d->Gc, GCForeground, &v);
		
						XFillRectangle(	XDisplay(),
										d->Widget->handle(),
										d->Gc,
										d->Ox + clip->x1,
										d->Oy + clip->y1,
										clip->X(),
										clip->Y());
		
						v.foreground = d->Fore;
						XChangeGC(XDisplay(), d->Gc, GCForeground, &v);
					}
					
					// Draw the text over the top
					if (len < 0) len = StrlenW(text);
					if (len)
					{
						XftDrawString32(	d->Draw,
											d->ForeCol,
											d->Font,
											d->Ox + x,
											d->Oy + y,
											(XftChar32*)text,
											len);
						if (d->Underline)
						{
							XGlyphInfo Info;
							XftTextExtents32(XDisplay(), d->Font, (XftChar32*)text, len, &Info);
							drawLine(x, y + 1, x + Info.width + Info.xOff, y + 1);
						}
					}
				}
			
				if (clip)
				{
					PopClip();
				}
			}
		}
	}
}
