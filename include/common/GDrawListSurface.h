#ifndef _GDRAW_LIST_SURFACE_H_
#define _GDRAW_LIST_SURFACE_H_

class GDrawListSurface : public GSurface
{
	struct GDrawListSurfacePriv *d;

public:
	GDrawListSurface(int Width, int Height, GColourSpace Cs = CsRgba32);
	GDrawListSurface(GSurface *FromSurface);
	~GDrawListSurface();

	// Calls specific to this class:
	int Length();
	bool OnPaint(GSurface *Dest);
	GFont *GetFont();
	void SetFont(GFont *Font);
	GColour Background();
	GColour Background(GColour c);
	GDisplayString *Text(int x, int y, const char *Str, int Len = -1);
	
	// Calls that are stored and played back:
	GRect ClipRgn();
	GRect ClipRgn(GRect *Rgn);
	COLOUR Colour();
	COLOUR Colour(COLOUR c, int Bits = 0);
	GColour Colour(GColour c);
	int Op() { return GDC_SET; }
	int Op(int Op, NativeInt Param = -1) { return GDC_SET; }
	int X();
	int Y();
	int GetRowStep();
	int DpiX();
	int DpiY();
	int GetBits();
	uchar *operator[](int y) { return NULL; }
	void GetOrigin(int &x, int &y) { x = OriginX; y = OriginY; }
	void SetOrigin(int x, int y);
	void Set(int x, int y);
	COLOUR Get(int x, int y) { return 0; }

	// Primitives
	void HLine(int x1, int x2, int y);
	void VLine(int x, int y1, int y2);
	void Line(int x1, int y1, int x2, int y2);
	uint LineStyle(uint32 Bits, uint32 Reset = 0x80000000);
	void Circle(double cx, double cy, double radius);
	void FilledCircle(double cx, double cy, double radius);
	void Arc(double cx, double cy, double radius, double start, double end);
	void FilledArc(double cx, double cy, double radius, double start, double end);
	void Ellipse(double cx, double cy, double x, double y);
	void FilledEllipse(double cx, double cy, double x, double y);
	void Box(int x1, int y1, int x2, int y2);
	void Box(GRect *a = NULL);
	void Rectangle(int x1, int y1, int x2, int y2);
	void Rectangle(GRect *a = NULL);
	void Blt(int x, int y, GSurface *Src, GRect *a = NULL);
	void StretchBlt(GRect *d, GSurface *Src, GRect *s);
	void Polygon(int Points, GdcPt2 *Data);
	void Bezier(int Threshold, GdcPt2 *Pt);
	void FloodFill(int x, int y, int Mode, COLOUR Border = 0, GRect *Bounds = NULL);	


	// Stubs that don't work here..
	bool HasAlpha() { return false; }
	bool HasAlpha(bool b) { return false; }
	bool Applicator(GApplicator *pApp) { return false; }
	GApplicator *Applicator() { return NULL; }
	GPalette *Palette() { return NULL; }
	void Palette(GPalette *pPal, bool bOwnIt = true) { }
};

#endif