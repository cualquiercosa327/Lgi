
#ifndef __CPainter_h
#define __CPainter_h

#include "Xft.h"
#include "Xrender.h"

#include "xwidget.h"
#include "xbitmapimage.h"

class XRgb : public XObject
{
	int R, G, B;

public:
	XRgb(int r, int g, int b)
	{
		set(r, g, b);
	}

	int r() { return R; }
	int g() { return G; }
	int b() { return B; }
	void set(int r, int g, int b)
	{	
		R = r; G = g; B = b;
	}
};

class XPainter : public XObject
{
protected:
	class GPainterPrivate *d;

public:
	enum RowOperation
	{
		CopyROP,
		AndROP,
		OrROP,
		XorROP
	};

	XPainter();
	~XPainter();

	int X();
	int Y();
	XWidget *Handle();
	class GRect *GetClient();

	virtual void GetScale(double &x, double &y) { x = 1.0; y = 1.0; }
	
	virtual bool Begin(XWidget *w);
	virtual void End();
	virtual bool IsOk();

	virtual void SetClient(class GRect *r);
	virtual void PushClip(int x1, int y1, int x2, int y2);
	virtual void PopClip();
	virtual void EmptyClip();
	
	virtual void translate(int x, int y);
	virtual void setFore(int c);
	virtual void setBack(int c);
	virtual void setRasterOp(RowOperation i);
	virtual RowOperation rasterOp();
	virtual void setFont(class XFont &f);

	virtual void drawPoint(int x, int y);
	virtual void drawLine(int x1, int y1, int x2, int y2);
	virtual void drawRect(int x, int y, int wid, int height);
	virtual void drawArc(double cx, double cy, double radius);
	virtual void drawArc(double cx, double cy, double radius, double start, double end);
	virtual void fillArc(double cx, double cy, double radius);
	virtual void fillArc(double cx, double cy, double radius, double start, double end);
	virtual void drawImage(int x, int y, XBitmapImage &image, int sx, int sy, int sw, int sh, XBitmapImage::BlitOp op);
	virtual void drawText(int x, int y, char16 *text, int len, int *backColour, GRect *clip);
};

#endif
