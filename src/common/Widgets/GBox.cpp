#include "Lgi.h"
#include "GBox.h"
#include "GCssTools.h"
#include "LgiRes.h"
#include "GPopup.h"

#define DEFAULT_SPACER_PX			5
// #define DEFAULT_SPACER_COLOUR24		LC_MED
#define DEFAULT_MINIMUM_SIZE_PX		5
#define ACTIVE_SPACER_SIZE_PX		9

enum GBoxMessages
{
	M_CHILDREN_CHANGED = M_USER + 0x2000
};

struct GBoxPriv
{
public:
	bool Vertical;
	GArray<GBox::Spacer> Spacers;
	GBox::Spacer *Dragging;
	GdcPt2 DragOffset;
	bool Dirty;
	
	GBoxPriv()
	{
		Dirty = false;
		Vertical = false;
		Dragging = NULL;
	}
	
	int GetBox(GRect &r)
	{
		return Vertical ? r.Y() : r.X();
	}

	GBox::Spacer *HitTest(int x, int y)
	{
		for (int i=0; i<Spacers.Length(); i++)
		{
			GBox::Spacer &s = Spacers[i];
			GRect Pos = s.Pos;
			if (Vertical)
			{
				while (Pos.Y() < ACTIVE_SPACER_SIZE_PX)
					Pos.Size(0, -1);
			}
			else
			{
				while (Pos.X() < ACTIVE_SPACER_SIZE_PX)
					Pos.Size(-1, 0);
			}
				
			if (Pos.Overlap(x, y))
			{
				return &s;
			}
		}
		
		return NULL;
	}	
};

GBox::GBox(int Id, bool Vertical, const char *name)
{
	d = new GBoxPriv;
	SetId(Id);
	SetVertical(Vertical);
	if (name)
		Name(name);
	LgiResources::StyleElement(this);
}

GBox::~GBox()
{
	GWindow *Wnd = GetWindow();
	if (Wnd)
		Wnd->UnregisterHook(this);

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

GBox::Spacer *GBox::GetSpacer(int idx)
{
	if (Children.Length())
	{
		while (d->Spacers.Length() < Children.Length() - 1)
		{
			Spacer &s = d->Spacers.New();
			s.SizePx = DEFAULT_SPACER_PX;
			// s.Colour.c24(DEFAULT_SPACER_COLOUR24);
		}
	}
	
	return idx >= 0 && idx < d->Spacers.Length() ? &d->Spacers[idx] : NULL;
}

GViewI *GBox::GetViewAt(int i)
{
	return Children[i];
}

bool GBox::SetViewAt(uint32 i, GViewI *v)
{
	if (!v || i > Children.Length())
	{
		return false;
	}

	if (v->GetParent())
		v->Detach();
	
	v->Visible(true);

	bool Status;
	if (i < Children.Length())
	{
		// Remove existing view..
		GViewI *existing = Children[i];
		if (existing == v)
			return true;
		if (existing)
			existing->Detach();
		Status = AddView(v, i);
		
	}
	else
	{
		Status = AddView(v);
	}
	
	if (Status)
	{
		AttachChildren();
	}
	
	return Status;
}

void GBox::OnCreate()
{
	AttachChildren();
	OnPosChange();
	
	GWindow *Wnd = GetWindow();
	if (Wnd)
		Wnd->RegisterHook(this, GMouseEvents);
}

bool GBox::OnViewMouse(GView *v, GMouse &m)
{
	// This hook allows the GBox to catch clicks nearby the splits even if the splits are too small
	// to grab normally. Consider the case of a split that is 1px wide. The active region needs to
	// be a little larger than that, however a normal click would go through to the child windows
	// on either side of the split rather than to the GBox.
	if (!m.IsMove() && m.Down())
	{
		// Convert click to the local coordinates of this view
		GMouse Local = m;
		while (v && v != (GView*)this && v->GetParent())
		{
			if (dynamic_cast<GPopup*>(v))
				return true;

			GRect p = v->GetPos();
			Local.x += p.x1;
			Local.y += p.y1;
			GViewI *vi = v->GetParent();
			v = vi ? vi->GetGView() : NULL;
		}
		
		if (v == (GView*)this)
		{
			// Is the click over our spacers?
			Spacer *s = d->HitTest(Local.x, Local.y);
			if (s)
			{
				// Pass the click to ourselves and prevent the normal view from getting it.
				OnMouseClick(Local);
				return false;
			}
		}
	}
	
	return true;
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
	if (d->Dirty)
	{
		d->Dirty = false;
		OnPosChange();
	}

	GRect cli = GetClient();
	GCssTools tools(GetCss(), GetFont());
	cli = tools.PaintBorderAndPadding(pDC, cli);

	GColour cBack = StyleColour(GCss::PropBackgroundColor, GColour(LC_MED, 24));

	size_t ChildViews = Children.Length();
	if (ChildViews == 0)
	{
		pDC->Colour(cBack);
		pDC->Rectangle(&cli);
	}
	else
	{
		#if 0 // coverage check...
		pDC->Colour(GColour(255, 0, 255));
		pDC->Rectangle(&cli);
		#endif
		
		for (int i=0; i<d->Spacers.Length(); i++)
		{
			Spacer &s = d->Spacers[i];
			if (s.Colour.IsValid())
				pDC->Colour(s.Colour);
			else
				pDC->Colour(cBack);
			pDC->Rectangle(&s.Pos);
		}
	}
}

struct BoxRange
{
	int Min, Max;
	GCss::Len Size;
	GViewI *View;
	
	BoxRange()
	{
		Min = Max = 0;
		View = NULL;
	}
};

void GBox::OnPosChange()
{
	GCssTools tools(GetCss(), GetFont());
	GRect content = tools.ApplyMargin(GetClient());
	GetSpacer(0);

	GAutoPtr<GViewIterator> views(IterateViews());
	int Cur = 0, Idx = 0;
	int AvailablePx = d->GetBox(content);
	if (AvailablePx <= 0)
		return;

	GArray<BoxRange> Sizes;

	int SpacerPx = 0;

	int FixedPx = 0;
	int FixedChildren = 0;
	
	int PercentPx = 0;
	int PercentChildren = 0;
	float PercentCount = 0.0f;
	
	int AutoChildren = 0;

	// Do first pass over children and find their sizes
	for (GViewI *c = views->First(); c; c = views->Next(), Idx++)
	{
		GCss *css = c->GetCss();
		BoxRange &box = Sizes.New();
		
		box.View = c;
		
		// Get any available CSS size
		if (css)
		{
			if (IsVertical())
				box.Size = css->Height();
			else
				box.Size = css->Width();
		}

		// Work out some min and max values		
		if (box.Size.IsValid())
		{
			if (box.Size.Type == GCss::LenPercent)
			{
				box.Max = box.Size.ToPx(AvailablePx, GetFont());
				PercentPx += box.Max;
				PercentCount += box.Size.Value;
				PercentChildren++;
			}
			else if (box.Size.IsDynamic())
			{
				AutoChildren++;
			}
			else
			{
				// Fixed children get first crack at the space
				box.Min
					= box.Max
					= box.Size.ToPx(AvailablePx, GetFont());
				FixedPx += box.Min;
				FixedChildren++;
			}
		}
		else AutoChildren++;

		// Allocate area for spacers in the Fixed portion
		if (Idx < Children.Length() - 1)
		{
			Spacer &s = d->Spacers[Idx];
			SpacerPx += s.SizePx;
		}
	}

	// Convert all the percentage sizes to px
	int RemainingPx = AvailablePx - SpacerPx - FixedPx;
	for (int i=0; i<Sizes.Length(); i++)
	{
		BoxRange &box = Sizes[i];
		if (box.Size.Type == GCss::LenPercent)
		{
			if (PercentPx > RemainingPx)
			{
				if (AutoChildren > 0 || PercentChildren > 1)
				{
					// Well... ah... we better leave _some_ space for them.
					int AutoPx = 16 * AutoChildren;
					float Ratio = ((float)RemainingPx - AutoPx) / PercentPx;
					int Px = (int) (box.Max * Ratio);
					box.Size.Type = GCss::LenPx;
					box.Size.Value = (float) Px;
					RemainingPx -= Px;
				}
				else
				{
					// We can just take all the space...
					box.Size.Type = GCss::LenPx;
					box.Size.Value = (float) RemainingPx;
					RemainingPx = 0;
				}
			}
			else
			{
				box.Size.Type = GCss::LenPx;
				box.Size.Value = (float) box.Max;
				RemainingPx -= box.Max;
			}
		}
	}

	// Convert auto children to px
	int AutoPx = AutoChildren > 0 ? RemainingPx / AutoChildren : 0;
	for (int i=0; i<Sizes.Length(); i++)
	{
		BoxRange &box = Sizes[i];
		if (box.Size.Type != GCss::LenPx)
		{
			box.Size.Type = GCss::LenPx;
			if (AutoChildren > 1)
			{
				box.Size.Value = (float) AutoPx;
				RemainingPx -= AutoPx;
			}
			else
			{
				box.Size.Value = (float) RemainingPx;
				RemainingPx = 0;
			}
			AutoChildren--;
		}
	}
	
	for (int i=0; i<Sizes.Length(); i++)
	{
		BoxRange &box = Sizes[i];
		LgiAssert(box.Size.Type == GCss::LenPx);
		int Px = (int) (box.Size.Value + 0.01);

		// Allocate space for view
		GRect viewPos = content;
		if (d->Vertical)
		{
			viewPos.y1 = Cur;
			viewPos.y2 = Cur + Px - 1;
		}
		else
		{
			viewPos.x1 = Cur;
			viewPos.x2 = Cur + Px - 1;
		}
		box.View->SetPos(viewPos);
		#ifdef WIN32
		// This forces the update, otherwise the child's display lags till the
		// mouse is released *rolls eyes*
		box.View->Invalidate((GRect*)NULL, true);
		#endif
		Cur += Px;
		
		// Allocate area for spacer
		if (i < Sizes.Length() - 1)
		{
			Spacer &s = d->Spacers[i];
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
		d->Dragging = d->HitTest(m.x, m.y);
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

bool IsValidLen(GCss *c, GCss::PropType p)
{
	if (!c || c->GetType(p) != GCss::TypeLen) return false;
	GCss::Len *l = (GCss::Len*)c->PropAddress(p);
	if (!l) return false;
	return l->IsValid();
}

void GBox::OnMouseMove(GMouse &m)
{
	if (!d->Dragging || !IsCapturing())
		return;

	if (!m.Down())
	{
		// Something else got the up click?
		Capture(false);
		return;
	}

	int DragIndex = (int) (d->Dragging - &d->Spacers[0]);
	if (DragIndex < 0 || DragIndex >= d->Spacers.Length())
	{
		LgiAssert(0);
		return;
	}

	GViewI *Prev = Children[DragIndex];
	if (!Prev)
	{
		LgiAssert(0);
		return;
	}

	GViewI *Next = DragIndex < Children.Length() ? Children[DragIndex+1] : NULL;

	GCssTools tools(GetCss(), GetFont());
	GRect Content = tools.ApplyMargin(GetClient());
	int ContentPx = d->GetBox(Content);

	GRect SplitPos = d->Dragging->Pos;

	GCss *PrevStyle = Prev->GetCss();
	GCss::PropType Style = d->Vertical ? GCss::PropHeight : GCss::PropWidth;
	bool EditPrev = !Next || IsValidLen(PrevStyle, Style);
	GViewI *Edit = EditPrev ? Prev : Next;
	LgiAssert(Edit != NULL);
	GRect ViewPos = Edit->GetPos();
	auto *EditCss = Edit->GetCss(true);

	if (d->Vertical)
	{
		// Work out the minimum height of the view
		GCss::Len MinHeight = EditCss->MinHeight();
		int MinPx = MinHeight.IsValid() ? MinHeight.ToPx(ViewPos.Y(), Edit->GetFont()) : DEFAULT_MINIMUM_SIZE_PX;

		int Offset = m.y - d->DragOffset.y - SplitPos.y1;
		if (Offset)
		{
			// Slide up and down the Y axis

			// Limit to the min size
			GRect r = ViewPos;
			if (EditPrev)
			{
				r.y2 += Offset;
				if (r.Y() < MinPx)
				{
					int Diff = MinPx - r.Y();
					Offset += Diff;
					r.y2 += Diff;
				}
			}
			else
			{
				r.y1 += Offset;
				if (r.Y() < MinPx)
				{
					int Diff = MinPx - r.Y();
					Offset -= Diff;
					r.y1 -= Diff;
				}
			}

			if (Offset)
			{
				SplitPos.Offset(0, Offset);
		
				// Save the new height of the view
				GCss::Len Ht = EditCss->Height();
				if (Ht.Type == GCss::LenPercent && ContentPx > 0)
				{
					Ht.Value = (float)r.Y() * 100 / ContentPx;
				}
				else
				{
					Ht.Type = GCss::LenPx;
					Ht.Value = (float)r.Y();
				}

				EditCss->Height(Ht);
			}
		}
	}
	else
	{
		// Work out the minimum width of the view
		GCss::Len MinWidth = EditCss->MinWidth();
		int MinPx = MinWidth.IsValid() ? MinWidth.ToPx(ViewPos.X(), Edit->GetFont()) : DEFAULT_MINIMUM_SIZE_PX;

		int Offset = m.x - d->DragOffset.x - SplitPos.x1;
		if (Offset)
		{
			// Slide along the X axis

			// Limit to the min size
			GRect r = ViewPos;
			if (EditPrev)
			{
				r.x2 += Offset;
				int rx = r.X();
				if (r.X() < MinPx)
				{
					int Diff = MinPx - rx;
					Offset += Diff;
					r.x2 += Diff;
				}
			}
			else
			{
				r.x1 += Offset;
				int rx = r.X();
				if (r.X() < MinPx)
				{
					int Diff = MinPx - rx;
					Offset -= Diff;
					r.x1 -= Diff;
				}
			}

			if (Offset)
			{
				SplitPos.Offset(Offset, 0);
		
				// Save the new height of the view
				GCss::Len Wid = EditCss->Width();
				if (Wid.Type == GCss::LenPercent && ContentPx > 0)
				{
					Wid.Value = (float)r.X() * 100 / ContentPx;
				}
				else
				{
					Wid.Type = GCss::LenPx;
					Wid.Value = (float)r.X();
				}

				EditCss->Width(Wid);
			}
		}
	}
	
	OnPosChange();
	Invalidate((GRect*)NULL, true);
}

void GBox::OnChildrenChanged(GViewI *Wnd, bool Attaching)
{
	#if 0
	LgiTrace("GBox(%s)::OnChildrenChanged(%s, %i)\n", Name(), Wnd ? Wnd->GetClass() : NULL, Attaching);
	for (int i=0; i<Children.Length(); i++)
		LgiTrace("	[%i]=%s hnd=%p vis=%i\n", i, Children[i]->GetClass(), Children[i]->Handle(), Children[i]->Visible());
	#endif
	
	d->Dirty = true;
	if (Handle())
		PostEvent(M_CHILDREN_CHANGED);
}

int64 GBox::Value()
{
	GViewI *v = Children.First();
	if (!v) return 0;
	GCss *css = v->GetCss();
	if (!css) return 0;
	GCss::Len l = d->Vertical ? css->Height() : css->Width();
	if (l.Type != GCss::LenPx)
		return 0;
	return (int64)l.Value;
}

void GBox::Value(int64 i)
{
	GViewI *v = Children.First();
	if (!v) return;
	GCss *css = v->GetCss(true);
	if (!css) return;
	if (d->Vertical)
		css->Height(GCss::Len(GCss::LenPx, (float)i));
	else
		css->Width(GCss::Len(GCss::LenPx, (float)i));
	OnPosChange();
}

LgiCursor GBox::GetCursor(int x, int y)
{
	Spacer *Over = d->HitTest(x, y);
	if (Over)
		return (d->Vertical) ? LCUR_SizeVer : LCUR_SizeHor;
	else
		return LCUR_Normal;
}

bool GBox::OnLayout(GViewLayoutInfo &Inf)
{
	Inf.Width.Min = -1;
	Inf.Width.Max = -1;
	Inf.Height.Min = -1;
	Inf.Height.Max = -1;
	return true;
}

bool GBox::Serialize(GDom *Dom, const char *OptName, bool Write)
{
	if (Write)
	{
	}
	else
	{
	}

	LgiAssert(0);
	
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

GMessage::Result GBox::OnEvent(GMessage *Msg)
{
	if (Msg->Msg() == M_CHILDREN_CHANGED)
	{
		if (d->Dirty)
		{
			d->Dirty = false;
			OnPosChange();
		}
	}
	
	return GView::OnEvent(Msg);
}
