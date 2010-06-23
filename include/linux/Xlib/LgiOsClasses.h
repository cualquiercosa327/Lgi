#ifndef __OS_CLASS_H
#define __OS_CLASS_H

/////////////////////////////////////////////////////////////////////////////////////
#include <xbitmapimage.h>
#include <xfont.h>
#include <xevent.h>
#include <xapplication.h>
#include <xpainter.h>
#include <xfont.h>
#include <xfontmetrics.h>

typedef pthread_t					OsThread;
typedef XFont						*OsFont;
typedef XApplication				OsApplication;
typedef XPainter					*OsPainter;
typedef XBitmapImage				*OsBitmap;
typedef char16						OsChar;

#undef Bool

class GMessage : public XlibEvent
{
	friend class XApplication;
	XEvent Local;

	XEvent *GetLocal()
	{
		// Make sure this is set BEFORE we give it
		// to XlibEvent, otherwise it'll call random
		// code.
		Local.type = ClientMessage;
		return &Local;
	}

	GMessage(XEvent *e) : XlibEvent(e)
	{
	}

public:
	GMessage(int M = 0, int A = 0, int B = 0) : XlibEvent(GetLocal())
	{
		m() = M;
		a() = A;
		b() = B;
	}

	int &m() { return (int&) (GetEvent()->xclient.message_type); }
	int &a() { return (int&) (Data()[0]); }
	int &b() { return (int&) (Data()[1]); }

	long *Data()
	{
		return GetEvent()->xclient.data.l;
	}
};

/////////////////////////////////////////////////////////////////////////////////////
#define MsgCode(Msg)				((Msg->type() == ClientMessage) ? ((GMessage*)Msg)->m() : 0)
#define MsgA(Msg)					((Msg->type() == ClientMessage) ? ((GMessage*)Msg)->a() : 0)
#define MsgB(Msg)					((Msg->type() == ClientMessage) ? ((GMessage*)Msg)->b() : 0)

extern GMessage CreateMsg(int m, int a, int b);

/////////////////////////////////////////////////////////////////////////////////////
template <class q>
class XView : public q
{
	friend class GView;

protected:
	GView *v;
	bool _Paint;

	void OnClick(XlibEvent *e, bool down, bool dbl, bool move);
	bool OnKey(XlibEvent *e, bool down);
	void setBackgroundMode(ViewBackground b) {}

public:
	XView(GView *view, bool p = true);
	XView(GView *view, Window Existing);
	~XView();

	void _SetWndPtr(void *p);
	void *_GetWndPtr();
	
	// Events
	void resizeEvent(XlibEvent *e);
	void paintEvent(XlibEvent *e);
	void customEvent(XlibEvent *e);
	void notifyEvent(int i = 0);
	
	// Mouse events
	void mousePressEvent(XlibEvent *e);
	void mouseDoubleClickEvent(XlibEvent *e);
	void mouseReleaseEvent(XlibEvent *e);
	void mouseMoveEvent(XlibEvent *e);
	void leaveEvent(XlibEvent *e);
	void enterEvent(XlibEvent *e);
	void wheelEvent(XlibEvent *e);

	// Focus events
	void focusInEvent(XlibEvent *e);
	void focusOutEvent(XlibEvent *e);

	// Keyboard events
	bool keyPressEvent(XlibEvent *e);
	bool keyReleaseEvent(XlibEvent *e);
};

typedef XView<XWidget> DefaultOsView;

class XWindow : public XMainWindow
{
	GWindow *Wnd;

public:
	XWindow(GWindow *wnd);
	~XWindow();

	void _SetWndPtr(void *p);
	void *_GetWndPtr();
	void SetModal();

	void resizeEvent(XlibEvent *re);
	void closeEvent(XlibEvent *ce);
	void paintEvent(XlibEvent *e);
	void customEvent(XlibEvent *e);
	void propertyEvent(XlibEvent *e);
};

class Xgc : public XObject
{
	GC Gc;
	Pixmap Pix;

public:
	Xgc(Pixmap p)
	{
		XGCValues n;
		Pix = p;
		Gc = XCreateGC(	XDisplay(),
						Pix,
						0,
						&n);
	}
	
	~Xgc()
	{					
		if (Gc)
			XFreeGC(XDisplay(), Gc);
	}
	
	operator GC()
	{
		return Gc;
	}
};

class Ximg : public XObject
{
	XImage *Img;
	int Line;
	char *Data;

public:
	Ximg(int x, int y, int depth)
	{
		Line = (x * depth + 7) / 8;
		Data = (char*)malloc(Line * y);
		Img = XCreateImage(	XDisplay(),
							DefaultVisual(XDisplay(), 0),
							depth,
							ZPixmap,
							0,
							Data,
							x, y,
							8,
							Line);
	}
	
	~Ximg()
	{
		XDestroyImage(Img);
	}

	operator XImage *()
	{
		return Img;
	}							

	uchar *operator [](int y)
	{
		if (y >= 0 AND y < Img->height)
		{
			return (uchar*)Img->data + (y * Img->bytes_per_line);
		}
		return 0;
	}

	void Set(int x, int y, int Col)
	{
		XPutPixel(Img, x, y, Col);
	}

	int Get(int x, int y)
	{
		return XGetPixel(Img, x, y);
	}
	
	int X()
	{
		return Img->width;
	}

	int Y()
	{
		return Img->height;
	}
	
	int Depth()
	{
		return Img->depth;
	}
};

class Xpix : public XObject
{
	Drawable v;
	Pixmap Pix;

public:	Xpix(Drawable draw, Ximg *Img)
	{
		v = draw;
		Pix = (Img) ? XCreatePixmap(XDisplay(),
									v,
									Img->X(),
									Img->Y(),
									Img->Depth()) : 0;
		if (Pix)
		{
			Xgc Gc(Pix);
			XPutImage(XDisplay(), Pix, Gc, *Img, 0, 0, 0, 0, Img->X(), Img->Y());
		}
	}
	
	~Xpix()
	{
		if (Pix)
			XFreePixmap(XDisplay(), Pix);
	}
	
	operator Pixmap()
	{
		return Pix;
	}
	
	void Detach()
	{
		Pix = 0;
	}
};

#endif
