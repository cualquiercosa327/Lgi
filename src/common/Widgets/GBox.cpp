#include "Lgi.h"
#include "GBox.h"
#include "GCssTools.h"

#define DEFAULT_SPACER_PX			5
#define DEFAULT_SPACER_COLOUR24		LC_MED
#define DEFAULT_MINIMUM_SIZE_PX		5

struct GBoxPriv
{
public:
	bool Vertical;
	GArray<GBox::Spacer> Spacers;
	GBox::Spacer *Dragging;
	GdcPt2 DragOffset;
	
	GBoxPriv()
	{
		Vertical = false;
		Dragging = NULL;
	}
	
	int GetBox(GRect &r)
	{
		return Vertical ? r.Y() : r.X();
	}

	GBox::Spacer *HitTest(GMouse &m)
	{
		for (int i=0; i<Spacers.Length(); i++)
		{
			GBox::Spacer &s = Spacers[i];
			if (s.Pos.Overlap(m.x, m.y))
			{
				return &s;
			}
		}
		
		return NULL;
	}	
};

GBox::GBox(int Id)
{
	d = new GBoxPriv;
	SetId(Id);
}

GBox::~GBox()
{
	DeleteObj(d);
}

bool GBox::IsVertical()
{
	return d->Vertical;
}

void GBox::SetVertical(bool v)
{
	if (d->Vertical != v)
	{
		d->Vertical = v;
		OnPosChange();
	}
}

void GBox::OnCreate()
{
	AttachChildren();
	OnPosChange();
}

bool GBox::Pour(GRegion &r)
{
	GRect *p = FindLargest(r);
	if (!p)
		return false;

	SetPos(*p);
	return true;
}

void GBox::OnPaint(GSurface *pDC)
{
	GRect cli = GetClient();
	GCssTools tools(GetCss(), GetFont());
	cli = tools.PaintBorderAndPadding(pDC, cli);

	for (int i=0; i<d->Spacers.Length(); i++)
	{
		Spacer &s = d->Spacers[i];
		pDC->Colour(s.Colour);
		pDC->Rectangle(&s.Pos);
	}
}

void GBox::OnPosChange()
{
	GCssTools tools(GetCss(), GetFont());
	GRect content = tools.ApplyMargin(GetClient());
	while (d->Spacers.Length() < Children.Length() - 1)
	{
		Spacer &s = d->Spacers.New();
		s.SizePx = DEFAULT_SPACER_PX;
		s.Colour.c24(DEFAULT_SPACER_COLOUR24);
	}
	
	GAutoPtr<GViewIterator> views(IterateViews());
	int Cur = 0, Idx = 0;
	for (GViewI *c = views->First(); c; c = views->Next(), Idx++)
	{
		// Get the size of the control according to CSS or failing that it's existing size
		GCss::Len size;
		GCss *css = c->GetCss();
		if (css)
			size = css->Width();
		if (!size.IsValid())
		{
			size.Type = GCss::LenPx;
			size.Value = (float) d->GetBox(c->GetPos());
		}
		
		// Convert to px
		int size_px;
		if (Idx < Children.Length() - 1)
			size_px = size.ToPx(d->GetBox(content), GetFont());
		else
			size_px = d->GetBox(content);
		
		// Allocate space for view
		GRect viewPos = content;
		if (d->Vertical)
		{
			viewPos.y1 = Cur;
			viewPos.y2 = Cur + size_px;
		}
		else
		{
			viewPos.x1 = Cur;
			viewPos.x2 = Cur + size_px;
		}
		c->SetPos(viewPos);
		Cur += size_px;
		
		// Allocate area for spacer
		if (Idx < Children.Length() - 1)
		{
			Spacer &s = d->Spacers[Idx];
			s.Pos = content;
			if (d->Vertical)
			{
				s.Pos.y1 = Cur;
				s.Pos.y2 = Cur + s.SizePx - 1;
			}
			else
			{
				s.Pos.x1 = Cur;
				s.Pos.x2 = Cur + s.SizePx - 1;
			}
			Cur += s.SizePx;
		}
	}
}

void GBox::OnMouseClick(GMouse &m)
{
	if (m.Down())
	{
		d->Dragging = d->HitTest(m);
		if (d->Dragging)
		{
			d->DragOffset.x = m.x - d->Dragging->Pos.x1;
			d->DragOffset.y = m.y - d->Dragging->Pos.y1;
			Capture(d->Dragging != NULL);
		}
	}
	else if (IsCapturing())
	{
		Capture(false);
		d->Dragging = NULL;
	}
}

void GBox::OnMouseMove(GMouse &m)
{
	if (d->Dragging && IsCapturing())
	{
		int DragIndex = d->Dragging - &d->Spacers[0];
		if (DragIndex < 0 || DragIndex >= d->Spacers.Length())
		{
			LgiAssert(0);
		}
		else
		{
			GViewI *c = Children[DragIndex];
			if (!c)
			{
				LgiAssert(0);
			}
			else
			{
				GCssTools tools(GetCss(), GetFont());
				GRect Content = tools.ApplyMargin(GetClient());
				int ContentPx = d->GetBox(Content);

				GCss *css = c->GetCss(true);
				GRect SplitPos = d->Dragging->Pos;
				GRect ViewPos = c->GetPos();
				if (d->Vertical)
				{
					// Work out the minimum height of the view
					GCss::Len MinHeight = css->MinHeight();
					int MinPx = MinHeight.IsValid() ? MinHeight.ToPx(ViewPos.Y(), c->GetFont()) : DEFAULT_MINIMUM_SIZE_PX;

					// Slide up and down the Y axis
					SplitPos.Offset(0, m.y - d->DragOffset.y - SplitPos.y1);

					// Limit to the min size
					int MinY = ViewPos.y1 + MinPx;
					if (SplitPos.y1 < ViewPos.y1 + MinY)
						SplitPos.Offset(ViewPos.y1 + MinY - SplitPos.y1, 0);
					
					// Save the new height of the view
					GCss::Len Ht = css->Height();
					if (Ht.Type == GCss::LenPercent)
					{
						Ht.Value = (float) ((SplitPos.y1 - ViewPos.y1) * 100 / ContentPx);
					}
					else
					{
						Ht.Type = GCss::LenPx;
						Ht.Value = (float) (SplitPos.y1 - ViewPos.y1);
					}
					css->Height(Ht);
				}
				else
				{
					// Work out the minimum width of the view
					GCss::Len MinWidth = css->MinWidth();
					int MinPx = MinWidth.IsValid() ? MinWidth.ToPx(ViewPos.X(), c->GetFont()) : DEFAULT_MINIMUM_SIZE_PX;

					// Slide along the X axis
					SplitPos.Offset(m.x - d->DragOffset.x - SplitPos.x1, 0);

					// Limit to the min size
					int MinX = ViewPos.x1 + MinPx;
					if (SplitPos.x1 < ViewPos.x1 + MinX)
						SplitPos.Offset(ViewPos.x1 + MinX - SplitPos.x1, 0);
					
					// Save the new height of the view
					GCss::Len Wd = css->Width();
					if (Wd.Type == GCss::LenPercent)
					{
						Wd.Value = (float) ((SplitPos.x1 - ViewPos.x1) * 100 / ContentPx);
					}
					else
					{
						Wd.Type = GCss::LenPx;
						Wd.Value = (float) (SplitPos.x1 - ViewPos.x1);
					}
					css->Width(Wd);
				}
				
				OnPosChange();
				Invalidate();
			}
		}
	}

	Spacer *Over = d->HitTest(m);
	if (Over)
		SetCursor((d->Vertical) ? LCUR_SizeVer : LCUR_SizeHor);
	else
		SetCursor(LCUR_Normal);
}

bool GBox::Serialize(GDom *Dom, const char *OptName, bool Write)
{
	if (Write)
	{
	}
	else
	{
	}
	
	return false;
}

bool GBox::SetSize(int ViewIndex, GCss::Len Size)
{
	GViewI *v = Children[ViewIndex];
	if (!v)
		return false;

	GCss *c = v->GetCss(true);
	if (!c)
		return false;
	
	c->Width(Size);
	return true;
}
