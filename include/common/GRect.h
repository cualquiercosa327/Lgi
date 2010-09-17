/*
	\file
	\author Matthew Allen
	\date 22/8/1997
*/

#ifndef __GDCREGION_H
#define __GDCREGION_H

#if defined WIN32

	#define CornerOffset 1
	typedef RECT OsRect;

#elif defined BEOS

	#define CornerOffset 0
	typedef BRect OsRect;

#elif defined ATHEOS

	#include <gui/rect.h>
	#define CornerOffset 0
	typedef os::Rect OsRect;

#elif defined MAC

	#define CornerOffset 1
	typedef Rect OsRect;

#else

	// Implement as required
	#define CornerOffset 0
	struct OsRect
	{
		int left, right, top, bottom;
	};

#endif

/// Rectangle class
class LgiClass GRect
{
	friend LgiClass bool operator ==(GRect &a, GRect &b);
	friend LgiClass bool operator !=(GRect &a, GRect &b);

public:
	int x1, y1, x2, y2;

	GRect() {}
	GRect(int X1, int Y1, int X2, int Y2)
	{
		x1 = X1;
		x2 = X2;
		y1 = Y1;
		y2 = Y2;
	}

	GRect(GRect *r)
	{
		x1 = r->x1;
		x2 = r->x2;
		y1 = r->y1;
		y2 = r->y2;
	}

	/// Returns the width
	int X() { return x2 - x1 + 1; }
	
	/// Returns the height
	int Y() { return y2 - y1 + 1; }
	
	/// Sets the rectangle
	void Set(int X1, int Y1, int X2, int Y2)
	{
		x1 = X1;
		x2 = X2;
		y1 = Y1;
		y2 = Y2;
	}

	/// Zero offset, sets the top left to 0,0 and the bottom right to x,y
	void ZOff(int x, int y);
	
	/// Normalizes the rectangle so that left is less than the right and so on
	void Normal();
	
	/// Returns true if the rectangle is valid
	bool Valid();
	
	/// Moves the rectangle by an offset
	void Offset(int x, int y);
	
	/// Moves the edges by an offset
	void Offset(GRect *a);
	
	/// Zooms the rectangle
	void Size(int x, int y);

	/// Zooms the rectangle
	void Size(GRect *a);

	/// Sets the width and height
	void Dimension(int x, int y);

	/// Sets the width and height
	void Dimension(GRect *a);

	/// Sets the rectangle to the intersection of this object and 'b'
	void Bound(GRect *b);

	/// Returns true if the point 'x,y' is in this rectangle
	bool Overlap(int x, int y);

	/// Returns true if the rectangle 'b' overlaps this rectangle
	bool Overlap(GRect *b);
	
	/// Enlarges this rectangle to include the point 'x,y'
	void Union(int x, int y);

	/// Enlarges this rectangle to include all points in 'a'
	void Union(GRect *a);

	/// Makes this rectangle include all points in 'a' and 'b'
	void Union(GRect *a, GRect *b);

	/// Makes this rectangle the intersection of 'this' and 'a'
	void Intersection(GRect *a);

	/// Makes this rectangle the intersection of 'a' and 'b'
	void Intersection(GRect *a, GRect *b);

	/// Returns a static string formated to include the points in the order: x1,y1,x2,y2
	char *GetStr();
	char *Describe() { return GetStr(); }

	/// Sets the rect from a string containing: x1,y1,x2,y2
	bool SetStr(char *s);

	/// Returns how near a point is to a rectangle
	int Near(int x, int y);
	/// Returns how near a point is to a rectangle
	int Near(GRect &r);

	/// Returns an operating system specific rectangle
	operator OsRect()
	{
		OsRect r;

		r.left = x1;
		r.top = y1;
		r.right = x2+CornerOffset;
		r.bottom = y2+CornerOffset;

		return r;
	}
	
	bool operator ==(const GRect &r)
	{
		return	x1 == r.x1 &&
				y1 == r.y1 &&
				x2 == r.x2 &&
				y2 == r.y2;
	}
	
	GRect &operator =(const GRect &r);	
	
	GRect &operator =(OsRect &r)
	{
		x1 = (int) r.left;
		y1 = (int) r.top;
		x2 = (int) r.right - CornerOffset;
		y2 = (int) r.bottom - CornerOffset;
		return *this;
	}

	GRect(OsRect r)
	{
		x1 = (int) r.left;
		y1 = (int) r.top;
		x2 = (int) r.right - CornerOffset;
		y2 = (int) r.bottom - CornerOffset;
	}
	
	#ifdef MAC
	GRect &operator =(HIRect &r)
	{
		x1 = (int)r.origin.x;
		y1 = (int)r.origin.y;
		x2 = x1 + (int)r.size.width - 1;
		y2 = y1 + (int)r.size.height - 1;
		return *this;
	}
	
	operator CGRect()
	{
		CGRect r;
		r.origin.x = x1;
		r.origin.y = y2;
		r.size.width = x2 - x1;
		r.size.height = y2 - y1;
		return r;
	}
	#endif
	
	#ifdef __GTK_H__
	operator Gtk::GdkRectangle()
	{
	    Gtk::GdkRectangle r;
	    r.x = x1;
	    r.y = y1;
	    r.width = X();
	    r.height = Y();
	    return r;
	}
	#endif
};

LgiClass bool operator ==(GRect &a, GRect &b);
LgiClass bool operator !=(GRect &a, GRect &b);

/// A region is a list of non-overlapping rectangles that can describe any shape
class LgiClass GRegion : public GRect
{
	int Size;
	int Alloc;
	int Current;
	GRect *a;

	bool SetSize(int s);
	GRect *NewOne() { return (SetSize(Size+1)) ? a+(Size-1) : 0; }
	bool Delete(int i);

public:
	GRegion();
	GRegion(int X1, int Y1, int X2, int Y2);
	GRegion(GRect &r);
	GRegion(OsRect &r);
	GRegion(GRegion &c);
	~GRegion();

	int X() { return x2 - x1 + 1; }
	int Y() { return y2 - y1 + 1; }
	int Length() { return Size; }
	GRect *operator [](int i) { return (i >= 0 AND i < Size) ? a+i : 0; }
	GRegion &operator =(const GRect &r);	
	GRect *First();
	GRect *Last();
	GRect *Next();
	GRect *Prev();

	void Empty();
	void ZOff(int x, int y);
	void Normal();
	bool Valid();
	void Offset(int x, int y);
	void Bound(GRect *b);
	GRect Bound();
	bool Overlap(GRect *b);
	bool Overlap(int x, int y);
	
	void Union(GRect *a);
	void Intersect(GRect *a);
	void Subtract(GRect *a);

	friend bool operator ==(GRegion &a, GRegion &b);
	friend bool operator !=(GRegion &a, GRegion &b);
};

#endif


