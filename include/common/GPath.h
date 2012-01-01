#ifndef __GPath_h__
#define __GPath_h__

#include <math.h>

class GSeg;
class GVector;

extern bool _Disable_ActiveList;
extern bool _Disable_XSort;
extern bool _Disable_Alpha;
extern bool _Disable_Rops;

class LgiClass GMatrix
{
	friend class GPath;

	double m[6];

public:
	GMatrix()
	{
		ZeroObj(m);
	}

	void Translate(double x, double y)
	{
		m[0] = x;
		m[1] = y;
	}
};

enum GPathFillRule
{
	FILLRULE_ODDEVEN,
	FILLRULE_NONZERO
};

class LgiClass GPointF
{
public:
	static double Threshold;
	double x, y;

	GPointF()
	{
		x = y = 0;
	}

	GPointF(double X, double Y)
	{
		x = X;
		y = Y;
	}

	GPointF(const GPointF &p)
	{
		x = p.x;
		y = p.y;
	}

	GPointF &operator =(GPointF &p)
	{
		x = p.x;
		y = p.y;
		return *this;
	}

	GPointF &operator -(GPointF &p)
	{
		static GPointF Result;
		Result.x = x - p.x;
		Result.y = y - p.y;
		return Result;
	}

	GPointF &operator +(GPointF &p)
	{
		static GPointF Result;
		Result.x = x + p.x;
		Result.y = y + p.y;
		return Result;
	}

	bool operator ==(GPointF &p)
	{
		double dx = x - p.x;
		if (dx < 0) dx = -dx;
		double dy = y - p.y;
		if (dy < 0) dy = -dy;
		return dx<Threshold AND dy<Threshold;
	}

	bool operator !=(GPointF &p)
	{
		return !(*this == p);
	}

	void Set(double X, double Y)
	{
		x = X;
		y = Y;
	}
	
	GPointF &Translate(double tx, double ty)
	{
		x += tx;
		y += ty;
		return *this;
	}

	GPointF &Rotate(double a)
	{
		double ix = x;
		double iy = y;
		x = (cos(a)*ix) - (sin(a)*iy);
		y = (sin(a)*ix) + (cos(a)*iy);
		return *this;
	}
};

class LgiClass GRectF
{
public:
	double x1, y1, x2, y2;
	bool Set;

	GRectF()
	{
		Set = false;
	}

	GRectF(GRect &r)
	{
		*this = r;
	}

	GRectF(double X1, double Y1, double X2, double Y2)
	{
		x1 = X1; y1 = Y1; x2 = X2; y2 = Y2;
		Set = true;
	}

	GRectF(GPointF &a, GPointF &b)
	{
		x1 = a.x; y1 = a.y; x2 = b.x; y2 = b.y;
		Set = true;
	}

	double X() { return x2 - x1; }
	double Y() { return y2 - y1; }
	bool IsNormal() { return x2 >= x1 AND y2 >= y1; }
	
	void Normalize();
	void Union(GPointF &p);
	void Union(GRectF &p);
	void Intersect(GRectF &p);
	bool Overlap(GPointF &p);
	bool Overlap(GRectF &p);
	void Offset(double x, double y);
	void Size(double dx, double dy);
	char *Describe();

	GRectF &operator =(GRect &f);
	GRectF &operator =(GRectF &f);
	GRectF &operator =(GPointF &p);
};

class LgiClass GBrush
{
	friend class GPath;

protected:
	uchar AlphaLut[65];

	class GRopArgs
	{
	public:
		uchar *Pixels;
		uchar *EndOfMem;
		uchar *Alpha;
		int Bits;
		int Len;
		int x, y;

		GSurface *pDC;
	};

	virtual bool Start(GRopArgs &a) { return true; }
	virtual void Rop(GRopArgs &a) = 0;
	virtual int Alpha() { return 255; }

	void MakeAlphaLut();

public:
	GBrush() {}
	virtual ~GBrush() {}
};

class LgiClass GEraseBrush : public GBrush
{
	void Rop(GRopArgs &a);

public:
	GEraseBrush()
	{
		MakeAlphaLut();
	}
};

class LgiClass GSolidBrush : public GBrush
{
	COLOUR c32;

	void Rop(GRopArgs &a);
	int Alpha() { return A32(c32); }

public:
	GSolidBrush(COLOUR c)
	{
		c32 = c;
		MakeAlphaLut();
	}

	GSolidBrush(GColour c)
	{
		c32 = c.c32();
		MakeAlphaLut();
	}

	GSolidBrush(int r, int g, int b, int a = 255)
	{
		c32 = Rgba32(r, g, b, a);
		MakeAlphaLut();
	}
};

struct GBlendStop
{
	double Pos;
	COLOUR c32;
};

class LgiClass GBlendBrush : public GBrush
{
protected:
	int Stops;
	GBlendStop *Stop;

	COLOUR Lut[256];
	double Base, IncX, IncY;

	GPointF p[2];

	bool Start(GRopArgs &a);

public:
	GBlendBrush(int stops, GBlendStop *stop)
	{
		MakeAlphaLut();
		Stops = 0;
		Stop = 0;
		if (stop)
		{
			SetStops(stops, stop);
		}
	}

	~GBlendBrush()
	{
		DeleteArray(Stop);
	}

	int GetStops(GBlendStop **stop = 0)
	{
		if (stop)
			*stop = Stop;
		return Stops;
	}

	void SetStops(int stops, GBlendStop *stop)
	{
		Stops = stops;
		DeleteArray(Stop);
		Stop = new GBlendStop[Stops];
		if (Stop) memcpy(Stop, stop, sizeof(*Stop) * Stops);
	}
};

class LgiClass GLinearBlendBrush : public GBlendBrush
{
	bool Start(GRopArgs &Args);
	void Rop(GRopArgs &Args);

public:
	GLinearBlendBrush(GPointF a, GPointF b, int stops = 0, GBlendStop *stop = 0) :
		GBlendBrush(stops, stop)
	{
		p[0] = a;
		p[1] = b;
	}
};

class LgiClass GRadialBlendBrush : public GBlendBrush
{
	void Rop(GRopArgs &Args);

public:
	GRadialBlendBrush(GPointF center, GPointF rim, int stops = 0, GBlendStop *stop = 0) :
		GBlendBrush(stops, stop)
	{
		p[0] = center;
		p[1] = rim;
	}
};

class LgiClass GPath
{
	// Data
	List<GSeg> Segs;
	List<GVector> Vecs;
	GRectF Bounds;
	bool Aa;
	GPathFillRule FillRule;
	GMatrix Mat;
	
	// Flattened representation
	int Points;
	GArray<int> Outline;
	GPointF *Point;

	// Methods
	void Unflatten();
	void Append(GPath &p);

public:
	GPath(bool aa = true);
	virtual ~GPath();

	// Primitives
	void MoveTo(		double x,
						double y);
	void MoveTo(		GPointF &pt);
	void LineTo(		double x,
						double y);
	void LineTo(		GPointF &pt);
	void QuadBezierTo(	double cx, double cy,
						double px, double py);
	void QuadBezierTo(	GPointF &c,
						GPointF &p);
	void CubicBezierTo(	double c1x, double c1y,
						double c2x, double c2y,
						double px, double py);
	void CubicBezierTo(	GPointF &c1,
						GPointF &c2,
						GPointF &p);
	void Rectangle(		double x1, double y1,
						double x2, double y2);
	void Rectangle(		GPointF &tl,
						GPointF &rb);
	void Rectangle(		GRectF &r);
	void RoundRect(		GRectF &b, double r);
	void Circle(		double cx,
						double cy,
						double radius);
	void Circle(		GPointF &c,
						double radius);
	void Ellipse(		double cx,
						double cy,
						double x,
						double y);
	void Ellipse(		GPointF &c,
						double x,
						double y);
	bool Text(			GFont *Font,
						double x,
						double y,
						char *Utf8,
						int Bytes = -1);

	// Properties
	int Segments();
	void GetBounds(GRectF *b);
	GPathFillRule GetFillRule() { return FillRule; }
	void SetFillRule(GPathFillRule r) { FillRule = r; }

	// Methods
	bool IsClosed();
	void Close();

	bool IsFlattened();
	bool Flatten();

	bool IsEmpty();
	void Empty();

	void Transform(GMatrix &m);
	void DeleteSeg(int i);

	// Colouring
	void Fill(GSurface *pDC, GBrush &Brush);
	void Stroke(GSurface *pDC, GBrush &Brush, double Width);
};

void FlattenQuadratic(GPointF *&Out, GPointF &p1, GPointF &p2, GPointF &p3, int Steps);
void FlattenCubic(GPointF *&Out, GPointF &p1, GPointF &p2, GPointF &p3, GPointF &p4, int Steps);

#endif
