#include "Lgi.h"
#include "GGraph.h"
#include "GDocView.h"
#include <math.h>

struct GGraphPair
{
	GVariant x, y;
};

struct GGraphPriv
{
	int XAxis, YAxis;
	GVariantType XType, YType;
	GVariant MaxX, MinX;
	GVariant MaxY, MinY;
	GArray<GGraphPair> Val;

	GVariantType GuessType(char *s)
	{
		bool Dot = false;
		bool Num = false;
		bool Alpha = false;
		bool Slash = false;

		while (s && *s)
		{
			if (IsAlpha(*s))
				Alpha = true;
			else if (IsDigit(*s))
				Num = true;
			else if (*s == '/')
				Slash = true;
			else if (*s == '.')
				Dot = true;
			s++;
		}

		if (Num)
		{
			if (Dot)
				return GV_DOUBLE;
			else if (Slash)
				return GV_DATETIME;
			
			return GV_INT64;
		}
		else
		{
			return GV_STRING;
		}
	}

	bool Convert(GVariant &v, GVariantType type, char *in)
	{
		if (!in)
			return false;
		switch (type)
		{
			case GV_DOUBLE:
				v = atof(in);
				break;
			case GV_DATETIME:
			{
				GDateTime dt;
				dt.SetFormat(0);
				dt.Set(in);
				v = &dt;
				break;
			}
			case GV_INT64:
				v = atoi64(in);
				break;
			case GV_STRING:
				v = in;
				break;
			default:
				LgiAssert(!"Not impl.");
				break;
		}
		return true;
	}

	int Compare(GVariant &a, GVariant &b)
	{
		// a - b
		if (a.Type != b.Type)
		{
			LgiAssert(!"Only defined for comparing values of the same type.");
			return 0;
		}

		switch (a.Type)
		{
			case GV_DOUBLE:
			{
				double d = a.Value.Dbl - b.Value.Dbl;
				if (d < 0) return -1;
				else if (d > 0) return 1;
				break;
			}
			case GV_DATETIME:
			{
				return a.Value.Date->Compare(b.Value.Date);
				break;
			}
			case GV_INT64:
			{
				double i = a.Value.Int64 - b.Value.Int64;
				if (i < 0) return -1;
				else if (i > 0) return 1;
				break;
			}
			case GV_STRING:
			{
				return stricmp(a.Str(), b.Str());
				break;
			}
			default:
			{
				LgiAssert(!"Not impl.");
				break;
			}
		}

		return 0;
	}

	int Map(GVariant &v, int pixels, GVariant &min, GVariant &max)
	{
		if (v.Type != min.Type ||
			v.Type != max.Type)
		{
			LgiAssert(!"Incompatible types.");
			return 0;
		}

		switch (v.Type)
		{
			case GV_DATETIME:
			{
				uint64 Min, Max, Val;
				min.Value.Date->Get(Min);
				max.Value.Date->Get(Max);
				v.Value.Date->Get(Val);
				int64 Range = Max - Min;
				LgiAssert(Range > 0);

				return (Val - Min) * (pixels - 1) / Range;
				break;
			}
			case GV_INT64:
			{
				int64 Min, Max, Val;
				Min = min.CastInt64();
				Max = max.CastInt64();
				Val = v.CastInt64();

				int64 Range = Max - Min;
				LgiAssert(Range > 0);

				return (Val - Min) * (pixels - 1) / Range;
				break;
			}
			case GV_DOUBLE:
			{
				double Min, Max, Val;
				Min = min.CastDouble();
				Max = max.CastDouble();
				Val = v.CastDouble();

				double Range = Max - Min;
				LgiAssert(Range > 0);

				return (Val - Min) * (pixels - 1) / Range;
				break;
			}
			default:
				LgiAssert(0);
				break;
		}

		return 0;
	}

	void DrawAxis(GSurface *pDC, GRect &r, int xaxis, GVariant &min, GVariant &max)
	{
		GVariant v = min;

		int x = xaxis ? r.x1 : r.x2;
		int y = xaxis ? r.y1 : r.y2;
		int pixels = xaxis ? r.X() : r.Y();
		int64 int_range = 0;
		double dbl_inc = 0.0;

		SysFont->Colour(LC_TEXT, LC_WORKSPACE);

		for (int i=0; i<=pixels; )
		{
			int dx = x + (xaxis ? i : 0);
			int dy = y - (xaxis ? 0 : i);

			char s[256];
			switch (v.Type)
			{
				case GV_DATETIME:
				{
					if (v.Value.Date->Hours() ||
						v.Value.Date->Minutes())
						v.Value.Date->Get(s);
					else
						v.Value.Date->GetDate(s);
					break;
				}
				case GV_INT64:
				{
					sprintf(s, LGI_PrintfInt64, v.CastInt64());
					break;
				}
				case GV_DOUBLE:
				{
					sprintf(s, "%g", v.CastDouble());
					break;
				}
			}

			GDisplayString d(SysFont, s);
			if (xaxis)
				d.Draw(pDC, dx - (d.X()/2), dy + 10);
			else
				d.Draw(pDC, dx - d.X() - 10, dy - (d.Y() / 2));

			if (xaxis)
				pDC->Line(dx, dy, dx, dy + 5);
			else
				pDC->Line(dx, dy, dx - 5, dy);

			switch (v.Type)
			{
				case GV_DATETIME:
				{
					if (!i)
						v.Value.Date->SetTime("0:0:0");
					v.Value.Date->AddDays(1);
					int oldi = i;
					i = Map(v, pixels, min, max);
					x += i - oldi;
					break;
				}
				case GV_INT64:
				{
					if (!i)
					{
						int_range = max.CastInt64() - min.CastInt64();
						int asd=0;
					}

					LgiAssert(!"Finish this.");
					break;
				}
				case GV_DOUBLE:
				{
					if (!i)
					{
						double dbl_range = max.CastDouble() - min.CastDouble();
						int p = 0;
						while (dbl_range > 10)
						{
							p++;
							dbl_range /= 10;
						}
						while (dbl_range < 1)
						{
							p--;
							dbl_range *= 10;
						}
						dbl_inc = pow(10, p) / 10;
					}

					v = v.CastDouble() + dbl_inc;
					i = Map(v, pixels, min, max);
					break;
				}
			}
		}
	}
};

GGraph::GGraph(int Id, int XAxis, int YAxis)
{
	d = new GGraphPriv;
	d->XAxis = XAxis;
	d->YAxis = YAxis;
	SetPourLargest(true);
}

GGraph::~GGraph()
{
	DeleteObj(d);
}

bool GGraph::SetDataSource(GDbRecordset *Rs)
{
	if (!Rs)
		return false;

	if (d->XAxis < 0)
		d->XAxis = 0;
	if (d->YAxis < 0)
		d->YAxis = 1;

	d->XType = GV_NULL;
	d->YType = GV_NULL;

	for (bool b = Rs->MoveFirst(); b; b = Rs->MoveNext())
	{
		char *x = (*Rs)[d->XAxis];
		char *y = (*Rs)[d->YAxis];
		if (x && y)
		{
			if (d->XType == GV_NULL)
				d->XType = d->GuessType(x);
			if (d->YType == GV_NULL)
				d->YType = d->GuessType(y);

			GGraphPair &p = d->Val.New();
			if (d->Convert(p.x, d->XType, x))
			{
				if (d->MaxX.IsNull() || d->Compare(p.x, d->MaxX) > 0)
					d->MaxX = p.x;
				if (d->MinX.IsNull() || d->Compare(p.x, d->MinX) < 0)
					d->MinX = p.x;
			}

			if (d->Convert(p.y, d->YType, y))
			{
				if (d->MaxY.IsNull() || d->Compare(p.y, d->MaxY) > 0)
					d->MaxY = p.y;
				if (d->MinY.IsNull() || d->Compare(p.y, d->MinY) < 0)
					d->MinY = p.y;
			}
		}
	}

	return true;
}

void GGraph::OnPaint(GSurface *pDC)
{
	pDC->Colour(LC_WORKSPACE, 24);
	pDC->Rectangle();

	GRect c = GetClient();
	GRect data = c;
	data.Size(20, 20);
	GRect y = data;
	y.x2 = y.x1 + 60;
	data.x1 = y.x2 + 1;
	GRect x = data;
	x.y1 = x.y2 - 60;
	y.y2 = data.y2 = x.y1 - 1;	
	
	pDC->Colour(GColour(222, 222, 222));
	pDC->Box(&data);

	// Draw axis
	d->DrawAxis(pDC, x, true, d->MinX, d->MaxX);
	d->DrawAxis(pDC, y, false, d->MinY, d->MaxY);
	
	// Draw data
	int cx, cy, px, py;
	pDC->Colour(GColour(0, 0, 222));
	for (int i=0; i<d->Val.Length(); i++)
	{
		GGraphPair &p = d->Val[i];
		cx = x.x1 + d->Map(p.x, x.X(), d->MinX, d->MaxX);
		cy = y.y2 - d->Map(p.y, y.Y(), d->MinY, d->MaxY);
		if (i)
		{
			pDC->Line(cx, cy, px, py);
		}
		px = cx;
		py = cy;
	}
}

