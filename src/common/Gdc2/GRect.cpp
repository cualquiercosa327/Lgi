/*hdr
**	FILE:			GRect.cpp
**	AUTHOR:			Matthew Allen
**	DATE:			22/8/97
**	DESCRIPTION:	Rectangle class
**
**	Copyright (C) 1997-2015, Matthew Allen
**		fret@memecode.com
*/

#include <stdio.h>
#include <math.h>

#include "Gdc2.h"
#include "GToken.h"

bool GdcPt2::Inside(GRect &r)
{
	return	(x >= r.x1) &&
			(x <= r.x2) &&
			(y >= r.y1) &&
			(y <= r.y2);
}

GRect &GRect::operator =(const GRect &r)
{
	x1 = r.x1;
	x2 = r.x2;
	y1 = r.y1;
	y2 = r.y2;
	
	return *this;
}

void GRect::ZOff(int x, int y)
{
	x1 = y1 = 0;
	x2 = x;
	y2 = y;
}

void GRect::Normal()
{
	if (x1 > x2)
	{
		LgiSwap(x1, x2);
	}
	
	if (y1 > y2)
	{
		LgiSwap(y1, y2);
	}
}

int GRect::Near(int x, int y)
{
	if (Overlap(x, y))
	{
		return 0;
	}
	else if (x >= x1 && x <= x2)
	{
		if (y < y1)
			return y1 - y;
		else
			return y - y2;
	}
	else if (y >= y1 && y <= y2)
	{
		if (x < x1)
			return x1 - x;
		else
			return x - x2;
	}

	int dx = 0;
	int dy = 0;	
	if (x < x1)
	{
		if (y < y1)
		{
			// top left
			dx = x1 - x;
			dy = y1 - y;
		}
		else
		{
			// bottom left
			dx = x1 - x;
			dy = y - y2;
		}
	}
	else
	{
		if (y < y1)
		{
			// top right
			dx = x - x2;
			dy = y1 - y;
		}
		else
		{
			// bottom right
			dx = x - x2;
			dy = y - y2;
		}
	}

	return (int)ceil(sqrt( (double) ((dx * dx) + (dy * dy)) ));
}

int GRect::Near(GRect &r)
{
	if (Overlap(&r))
		return 0;

	if (r.x1 > x2)
	{
		// Off the right edge
		return r.x1 - x2;
	}
	else if (r.x2 < x1)
	{
		// Off the left edge
		return x1 - r.x2;
	}
	else if (r.y2 < y1)
	{
		// Off the top edge
		return y1 - r.y2;
	}
	else
	{
		// Off the bottom edge
		return r.y1 - y2;
	}
}

bool GRect::Valid()
{
	return (x1 <= x2 && y1 <= y2);
}

void GRect::Offset(int x, int y)
{
	x1 += x;
	y1 += y;
	x2 += x;
	y2 += y;
}

void GRect::Offset(GRect *a)
{
	x1 += a->x1;
	y1 += a->y1;
	x2 += a->x2;
	y2 += a->y2;
}

void GRect::Size(int x, int y)
{
	x1 += x;
	y1 += y;
	x2 -= x;
	y2 -= y;
}

void GRect::Size(GRect *a)
{
	x1 += a->x1;
	y1 += a->y1;
	x2 -= a->x2;
	y2 -= a->y2;
}

void GRect::Dimension(int x, int y)
{
	x2 = x1 + x - 1;
	y2 = y1 + y - 1;
}

void GRect::Dimension(GRect *a)
{
	x2 = x1 + a->X() - 1;
	y2 = y1 + a->Y() - 1;
}

void GRect::Bound(GRect *b)
{
	x1 = max(x1,b->x1);		
	y1 = max(y1,b->y1);
	x2 = min(x2,b->x2);
	y2 = min(y2,b->y2);
}

bool GRect::Overlap(int x, int y)
{
	return (x >= x1) && (x <= x2) && (y >= y1) && (y <= y2);
}

bool GRect::Overlap(GRect *b)
{
	if (Valid() &&
		b->Valid() &&
		(x1 > b->x2 ||
		y1 > b->y2 ||
		x2 < b->x1 ||
		y2 < b->y1))
	{
		return false;
	}

	return true;
}

void GRect::Union(int x, int y)
{
	x1 = min(x1, x);
	y1 = min(y1, y);
	x2 = max(x2, x);
	y2 = max(y2, y);
}

void GRect::Union(GRect *a)
{
	x1 = min(a->x1, x1);
	y1 = min(a->y1, y1);
	x2 = max(a->x2, x2);
	y2 = max(a->y2, y2);
}

void GRect::Union(GRect *a, GRect *b)
{
	x1 = min(a->x1,b->x1);
	y1 = min(a->y1,b->y1);
	x2 = max(a->x2,b->x2);
	y2 = max(a->y2,b->y2);
}

void GRect::Intersection(GRect *a)
{
	if (Overlap(a))
	{
		x1 = max(a->x1, x1);
		y1 = max(a->y1, y1);
		x2 = min(a->x2, x2);
		y2 = min(a->y2, y2);
	}
	else
	{
		x1 = y1 = 0;
		x2 = y2 = -1;
	}
}

void GRect::Intersection(GRect *a, GRect *b)
{
	if (a->Overlap(b))
	{
		x1 = max(a->x1,b->x1);
		y1 = max(a->y1,b->y1);
		x2 = min(a->x2,b->x2);
		y2 = min(a->y2,b->y2);
	}
	else
	{
		x1 = y1 = 0;
		x2 = y2 = -1;
	}
}

char *GRect::GetStr()
{
	#define BUFFERS	4
	static char Str[BUFFERS][48];
	static int Cur = 0;
	
	char *b = Str[Cur];
	sprintf_s(b, 48, "%i,%i,%i,%i", x1, y1, x2, y2);	
	Cur++;
	if (Cur >= BUFFERS)
		Cur = 0;

	return b;
}

bool GRect::SetStr(char *s)
{
	bool Status = false;
	if (s)
	{
		GToken t(s, ",");
		if (t.Length() == 4)
		{
			x1 = atoi(t[0]);
			y1 = atoi(t[1]);
			x2 = atoi(t[2]);
			y2 = atoi(t[3]);
			Status = true;
		}
	}
	return Status;
}

bool operator ==(GRect &a, GRect &b)
{
	return (a.x1 == b.x1 &&
		a.y1 == b.y1 &&
		a.x2 == b.x2 &&
		a.y2 == b.y2);
}

bool operator !=(GRect &a, GRect &b)
{
	return !((a.x1 == b.x1 &&
		a.y1 == b.y1 &&
		a.x2 == b.x2 &&
		a.y2 == b.y2));
}

////////////////////////////////////////////////////////////////////////////////////////////
GRegion::GRegion() : GRect(0, 0, 0, 0)
{
	Size = Alloc = 0;
	a = 0;
	Current = 0;
}

GRegion::GRegion(GRegion &c) : GRect(c.x1, c.y1, c.x2, c.y2)
{
	Size = Alloc = 0;	
	Current = 0;
	a = 0;

	SetSize(c.Length());
	for (int i=0; i<Size; i++)
	{
		a[i] = c.a[i];
	}
}

GRegion::GRegion(int X1, int Y1, int X2, int Y2) : GRect(X1, Y1, X2, Y2)
{
	Size = Alloc = 0;	
	Current = 0;
	a = 0;

	SetSize(1);
	if (a)
	{
		a[0].x1 = X1;
		a[0].y1 = Y1;
		a[0].x2 = X2;
		a[0].y2 = Y2;
	}
}

GRegion::GRegion(GRect &r) : GRect(r)
{
	Size = Alloc = 0;	
	Current = 0;
	a = 0;

	SetSize(1);
	if (a)
	{
		a[0].x1 = r.x1;
		a[0].y1 = r.y1;
		a[0].x2 = r.x2;
		a[0].y2 = r.y2;
	}
}

#if !defined COCOA
GRegion::GRegion(OsRect &r) : GRect(0, 0, 0, 0)
{
	Size = Alloc = 0;	
	Current = 0;
	a = 0;

	SetSize(1);
	if (a)
	{
		x1 = a[0].x1 = (int) r.left;
		y1 = a[0].y1 = (int) r.top;
		x2 = a[0].x2 = (int) r.right - CornerOffset;
		y2 = a[0].y2 = (int) r.bottom - CornerOffset;
	}
}
#endif

GRegion::~GRegion()
{
	Empty();
}

GRegion &GRegion::operator =(const GRect &r)
{
	SetSize(1);
	if (a)
	{
		*a = r;
	}

	return *this;
}

GRect *GRegion::First()
{
	Current = 0;
	return (Current >= 0 && Current < Size) ? a+Current : 0;
}

GRect *GRegion::Last()
{
	Current = Size-1;
	return (Current >= 0 && Current < Size) ? a+Current : 0;
}

GRect *GRegion::Next()
{
	Current++;
	return (Current >= 0 && Current < Size) ? a+Current : 0;
}

GRect *GRegion::Prev()
{
	Current--;
	return (Current >= 0 && Current < Size) ? a+Current : 0;
}

bool GRegion::SetSize(int s)
{
	bool Status = true;

	if (s > Alloc)
	{
		int NewAlloc = (s + 15) & (~0xF);
		GRect *Temp = new GRect[NewAlloc];
		if (Temp)
		{
			int Common = min(Size, s);
			memcpy(Temp, a, sizeof(GRect)*Common);
			DeleteArray(a);
			a = Temp;
			Size = s;
			Alloc = NewAlloc;
		}
		else
		{
			Status = false;
		}
	}
	else
	{
		Size = s;
		if (!Size)
		{
			DeleteArray(a);
			Alloc = 0;
		}
	}

	return Status;
}

bool GRegion::Delete(int i)
{
	if (i >= 0 && i < Size)
	{
		if (i != Size - 1)
		{
			memmove(a + i, a + (i + 1), sizeof(GRect) * (Size - i - 1));
		}
		Size--;
		return true;
	}
	return false;
}

void GRegion::Empty()
{
	Size = Alloc = 0;
	DeleteArray(a);
}

void GRegion::ZOff(int x, int y)
{
	if (SetSize(1))
	{
		a[0].ZOff(x, y);
	}
}

void GRegion::Normal()
{
	for (int i=0; i<Size; i++)
	{
		a[i].Normal();
	}
}

bool GRegion::Valid()
{
	bool Status = true;

	for (int i=0; i<Size && Status; i++)
	{
		Status &= a[i].Valid();
	}

	return Status && Size > 0;
}

void GRegion::Offset(int x, int y)
{
	for (int i=0; i<Size; i++)
	{
		a[i].Offset(x, y);
	}
}

GRect GRegion::Bound()
{
	static GRect r;
	
	if (a && Size > 0)
	{
		r = a[0];
		for (int i=1; i<Size; i++)
		{
			r.Union(a+i);
		}
	}
	else
	{
		r.ZOff(-1, -1);
	}

	return r;
}

void GRegion::Bound(GRect *b)
{
	if (b)
	{
		for (int i=0; i<Size; )
		{
			if (a[i].Overlap(b))
			{
				a[i++].Bound(b);
			}
			else
			{
				Delete(i);
			}
		}
	}
}

bool GRegion::Overlap(int x, int y)
{
	for (int i=0; i<Size; i++)
	{
		if (a[i].Overlap(x, y)) return true;
	}

	return false;
}

bool GRegion::Overlap(GRect *b)
{
	if (b)
	{
		for (int i=0; i<Size; i++)
		{
			if (a[i].Overlap(b))
			{
				return true;
			}
		}
	}

	return false;
}

void GRegion::Union(GRect *b)
{
	if (b && b->Valid())
	{
		Subtract(b);
		GRect *n = NewOne();
		if (n)
		{
			*n = *b;
		}
	}
}

void GRegion::Intersect(GRect *b)
{
	if (b)
	{
		for (int i=0; i<Size; )
		{
			if (a[i].Overlap(b))
			{
				a[i++].Bound(b);
			}
			else
			{
				Delete(i);
			}
		}
	}
}

void GRegion::Subtract(GRect *b)
{
	if (b && b->Valid())
	{
		GRect Sub = *b;
		int StartSize = Size;

		Normal();
		Sub.Normal();

		for (int i=0; i<StartSize; )
		{
			GRect c = *(a + i);
			if (c.Overlap(&Sub))
			{
				if (Sub.y1 > c.y1)
				{
					GRect *n = NewOne();
					n->x1 = c.x1;
					n->y1 = c.y1;
					n->x2 = c.x2;
					n->y2 = Sub.y1 - 1;
				}

				if (Sub.y2 < c.y2)
				{
					GRect *n = NewOne();
					n->x1 = c.x1;
					n->y1 = Sub.y2 + 1;
					n->x2 = c.x2;
					n->y2 = c.y2;
				}

				if (Sub.x1 > c.x1)
				{
					GRect *n = NewOne();
					n->x1 = c.x1;
					n->y1 = max(Sub.y1, c.y1);
					n->x2 = Sub.x1 - 1;
					n->y2 = min(Sub.y2, c.y2);
				}

				if (Sub.x2 < c.x2)
				{
					GRect *n = NewOne();
					n->x1 = Sub.x2 + 1;
					n->y1 = max(Sub.y1, c.y1);
					n->x2 = c.x2;
					n->y2 = min(Sub.y2, c.y2);
				}

				Delete(i);
				StartSize--;
			}
			else
			{
				i++;
			}
		}
	}	
}

bool operator ==(GRegion &a, GRegion &b)
{
	return false;
}

bool operator !=(GRegion &a, GRegion &b)
{
	return false;
}
