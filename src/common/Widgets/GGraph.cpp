#include "Lgi.h"
#include "GGraph.h"
#include "GDocView.h"
#include <math.h>

#define SELECTION_SIZE          2

struct GraphAv
{
    uint64 Sum;
    uint64 Count;
};

struct GGraphPriv
{
	int XAxis, YAxis;
	GVariantType XType, YType;
	GVariant MaxX, MinX;
	GVariant MaxY, MinY;
    GArray<GGraph::GGraphPair> Val;
	GGraph::Style Style;
	
	// Averages
	bool Average;
    GArray<GraphAv> Ave;
    int BucketSize;
    
    // Selection
    GAutoPtr<GdcPt2> Select;
    GArray<GGraph::GGraphPair*> Selection;
	
	GGraphPriv()
	{
	    #if 1
        Style = GGraph::PointGraph;
        #else
        Style = GGraph::LineGraph;
        #endif
	    Average = false;
	    BucketSize = 500;

	    XType = GV_NULL;
	    YType = GV_NULL;
	}

	GVariantType GuessType(char *s)
	{
		bool Dot = false;
		bool Num = false;
		bool Alpha = false;
		bool Delims = false;

		while (s && *s)
		{
			if (IsAlpha(*s))
				Alpha = true;
			else if (IsDigit(*s))
				Num = true;
			else if (strchr("/\\-_:", *s))
				Delims = true;
			else if (*s == '.')
				Dot = true;
			s++;
		}

		if (Num)
		{
			if (Dot)
				return GV_DOUBLE;
			else if (Delims)
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
				int64 i = a.Value.Int64 - b.Value.Int64;
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

				return (int) ((Val - Min) * (pixels - 1) / Range);
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
		bool First = true;
		bool Loop = true;

		if (min.Type == GV_NULL ||
			max.Type == GV_NULL)
			return;

		int x = xaxis ? r.x1 : r.x2;
		int y = xaxis ? r.y1 : r.y2;
		int pixels = xaxis ? r.X() : r.Y();
		int64 int_range = 0;
		double dbl_inc = 0.0;
		int64 int64_inc = 0;
		int date_inc = 1;

		SysFont->Colour(LC_TEXT, LC_WORKSPACE);

		GArray<GVariant> Values;
		while (Loop)
		{
			Values.Add(v);
			switch (v.Type)
			{
				default:
				{
					Loop = false;
					return;
					break;
				}
				case GV_DATETIME:
				{
					if (First)
					{
					    uint64 s, e;
					    min.Value.Date->Get(s);
					    max.Value.Date->Get(e);
					    int64 period = e - s;
                        double days = (double)period / GDateTime::Second64Bit / 24 / 60 / 60;
					    if (days > 7)
					        date_inc = (int) (days / 5);
					    else
					        date_inc = 1;
						v.Value.Date->SetTime("0:0:0");
					}
					v.Value.Date->AddDays(date_inc);
					Loop = *v.Value.Date < *max.Value.Date;
					break;
				}
				case GV_INT64:
				{
					if (First)
					{
						int64 int64_range = max.CastInt64() - min.CastInt64();
						int64 rng = int64_range;
						int p = 0;
						while (rng > 10)
						{
							p++;
							rng /= 10;
						}
						while (rng < 1)
						{
							p--;
							rng *= 10;
						}
						int64_inc = (int64) pow(10.0, p);
						int64 d = (int64)((v.CastInt64() + int64_inc) / int64_inc);
						v = d * int64_inc;
					}
					else
					{
						v = v.CastInt64() + int64_inc;
					}
					Loop = v.CastInt64() < max.CastInt64();
					break;
				}
				case GV_DOUBLE:
				{
					if (First)
					{
						double dbl_range = max.CastDouble() - min.CastDouble();
						double rng = dbl_range;
						int p = 0;
						while (rng > 10)
						{
							p++;
							rng /= 10;
						}
						while (rng < 1)
						{
							p--;
							rng *= 10;
						}
						dbl_inc = pow(10.0, p);
						int d = (int)((v.CastDouble() + dbl_inc) / dbl_inc);
						v = (double)d * dbl_inc;
					}
					else
					{
						v = v.CastDouble() + dbl_inc;
					}
					Loop = v.CastDouble() < max.CastDouble();
					break;
				}
			}

			First = false;
		}
		Values.Add(max);

		for (int i=0; i<Values.Length(); i++)
		{
			v = Values[i];
			int Offset = Map(v, pixels, min, max);
			int dx = x + (xaxis ? Offset : 0);
			int dy = y - (xaxis ? 0 : Offset);

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
				default:
					return;

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

bool GGraph::AddPair(char *x, char *y, void *UserData)
{
    if (!x || !y)
        return false;

    if (d->XType == GV_NULL)
        d->XType = d->GuessType(x);
    if (d->YType == GV_NULL)
        d->YType = d->GuessType(y);

    GGraphPair &p = d->Val.New();
    p.UserData = UserData;
    
    if (d->Convert(p.x, d->XType, x))
    {
	    if (d->MaxX.IsNull() || d->Compare(p.x, d->MaxX) > 0)
		    d->MaxX = p.x;
	    if (d->MinX.IsNull() || d->Compare(p.x, d->MinX) < 0)
		    d->MinX = p.x;
    }
    else
    {
        d->Val.Length(d->Val.Length()-1);
        return false;
    }

    if (d->Convert(p.y, d->YType, y))
    {
	    if (d->MaxY.IsNull() || d->Compare(p.y, d->MaxY) > 0)
		    d->MaxY = p.y;
	    if (d->MinY.IsNull() || d->Compare(p.y, d->MinY) < 0)
		    d->MinY = p.y;
    }
    else
    {
        d->Val.Length(d->Val.Length()-1);
        return false;
    }
    
    return true;
}

bool GGraph::SetDataSource(GDbRecordset *Rs, int XAxis, int YAxis)
{
	if (!Rs)
		return false;

	d->XType = GV_NULL;
	d->YType = GV_NULL;

    if (XAxis >= 0)
        d->XAxis = XAxis;
    if (YAxis >= 0)
        d->YAxis = YAxis;

	if (Rs->Fields() >= 2)
	{
	    int Idx = 0;
		for (bool b = Rs->MoveFirst(); b; b = Rs->MoveNext(), Idx++)
		{
		    if (d->XAxis < 0 || d->YAxis < 0)
		    {
                for (int i=0; i<Rs->Fields(); i++)
                {
                    char *s = (*Rs)[i];
                    GVariantType t = d->GuessType(s);
                    if (t != GV_NULL && t != GV_STRING)
                    {
                        if (d->XAxis < 0)
                        {
                            d->XAxis = i;
                            d->XType = t;
                        }
                        else if (d->YAxis < 0)
                        {
                            d->YAxis = i;
                            d->YType = t;
                        }
                        else break;
                    }
                }
            }			
			
			if (d->XAxis >= 0 && d->YAxis >= 0)
			    AddPair((*Rs)[d->XAxis], (*Rs)[d->YAxis]);
		}
	}

	return true;
}

void GGraph::SetStyle(Style s)
{
    d->Style = s;
    Invalidate();
}

GGraph::Style GGraph::GetStyle()
{
    return d->Style;
}

static enum Msg
{
    IDM_LINE = 100,
    IDM_POINT,
    IDM_AVERAGE,
    IDC_AVERAGE_SAVE,
};

void GGraph::OnMouseClick(GMouse &m)
{
    if (m.IsContextMenu())
    {
        GSubMenu s;
        m.ToScreen();

        GSubMenu *style = s.AppendSub("Style");
        style->AppendItem("Line", IDM_LINE);
        style->AppendItem("Point", IDM_POINT);
        
        GSubMenu *a = s.AppendSub("Average");
        GMenuItem *i = a->AppendItem("Show", IDM_AVERAGE);
        i->Checked(d->Average);
        a->AppendItem("Save", IDC_AVERAGE_SAVE);

        switch (s.Float(this, m.x, m.y))
        {
            case IDM_LINE:
                SetStyle(LineGraph);
                break;
            case IDM_POINT:
                SetStyle(PointGraph);
                break;
            case IDM_AVERAGE:
                d->Average = !d->Average;
                Invalidate();
                break;
            case IDC_AVERAGE_SAVE:
            {
                if (!d->Ave.Length())
                {
                    LgiMsg(this, "No average calculated.", "GGraph");
                    break;
                }

                GFileSelect s;
                s.Parent(this);
                s.Name("average.csv");
                char Desktop[MAX_PATH];
                LgiGetSystemPath(LSP_DESKTOP, Desktop, sizeof(Desktop));
                s.InitialDir(Desktop);
                if (!s.Save())
                    break;

                GFile o;
                if (!o.Open(s.Name(), O_WRITE))
                {
                    LgiMsg(this, "Failed to open file for writing.", "GGraph");
                    break;
                }

                o.SetSize(0);
                switch (d->MinX.Type)
                {
                    case GV_INT64:
                    {
                        int XRange = d->MaxX.CastInt64() - d->MinX.CastInt64() + 1;
                        for (int b=0; b<d->BucketSize; b++)
                        {
                            GraphAv &g = d->Ave[b];
                            int64 x = d->MinX.CastInt64() + (((b * d->BucketSize) + (d->BucketSize >> 1)) / XRange);
                            int64 y = (g.Count) ? g.Sum / g.Count : 0;
                            o.Print(LGI_PrintfInt64 "," LGI_PrintfInt64 "\n", x, y);
                        }
                        break;
                    }
                    case GV_DOUBLE:
                    {
                        double XRange = d->MaxX.CastDouble() - d->MinX.CastDouble();
                        for (int b=0; b<d->BucketSize; b++)
                        {
                            GraphAv &g = d->Ave[b];
                            double x = d->MinX.CastDouble() + ( ((double)b+0.5) * XRange / d->BucketSize);
                            int64 y = (g.Count) ? g.Sum / g.Count : 0;
                            o.Print("%f," LGI_PrintfInt64 "\n", x, y);
                        }
                        break;
                    }
                    default:
                        LgiAssert(0);
                        break;
                }
                break;
            }
        }
    }
    else if (m.Left() && m.Down())
    {
        d->Select.Reset(new GdcPt2);
        d->Select->x = m.x;
        d->Select->y = m.y;
        Invalidate();        
    }
}

GArray<GGraph::GGraphPair*> *GGraph::GetSelection()
{
    return &d->Selection;
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

    if (d->Average && !d->Ave.Length())
    {
        for (int i=0; i<d->Val.Length(); i++)
        {
	        GGraphPair &p = d->Val[i];
	        int Bucket = d->Map(p.x, d->BucketSize, d->MinX, d->MaxX);
            d->Ave[Bucket].Sum += p.y.CastInt64();
            d->Ave[Bucket].Count++;
        }
    }
	
	switch (d->Style)
	{
	    case LineGraph:
	    {
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
	        break;
	    }
	    case PointGraph:
	    {
	        for (int i=0; i<d->Val.Length(); i++)
	        {
		        GGraphPair &p = d->Val[i];
		        cx = x.x1 + d->Map(p.x, x.X(), d->MinX, d->MaxX);
		        cy = y.y2 - d->Map(p.y, y.Y(), d->MinY, d->MaxY);
		        pDC->Set(cx, cy);
		        
		        if (d->Select &&
		            abs(d->Select->x - cx) < SELECTION_SIZE &&
		            abs(d->Select->y - cy) < SELECTION_SIZE)
		        {
		            d->Selection.Add(&p);
		        }
	        }
	        
	        if (d->Average)
	        {
	            int px = -1, py = -1;
	            pDC->Colour(GColour(255, 0, 0));
	            for (int b=0; b<d->BucketSize; b++)
	            {
	                if (d->Ave[b].Count)
	                {
	                    int cx = x.x1 + (((b * x.X()) + (x.X() >> 1)) / d->BucketSize);
	                    GVariant v = d->Ave[b].Sum / d->Ave[b].Count;
	                    int cy = y.y2 - d->Map(v, y.Y(), d->MinY, d->MaxY);
	                    
	                    if (py >= 0)
	                    {
	                        pDC->Line(cx, cy, px, py);
	                    }	                    
	                    
	                    px = cx;
	                    py = cy;
	                }
	            }
	        }
	        
	        if (d->Select)
	        {
	            d->Select.Reset();
	            SendNotify();
	        }
	        break;
	    }
	}
}

