#include "Lgi.h"
#include "GScrollBar.h"

// #define DrawBorder(dc, r, edge) LgiWideBorder(dc, r, edge)
#define DrawBorder(dc, r, edge) LgiThinBorder(dc, r, edge)

#if defined(MAC) && !defined(LGI_SDL)
#define MAC_SKIN		1
#else
#define MAC_SKIN		0
#endif

enum ScrollZone
{
	BTN_NONE,
	BTN_SUB,
	BTN_SLIDE,
	BTN_ADD,
	BTN_PAGE_SUB,
	BTN_PAGE_ADD,
};

class GScrollBarPrivate
{
public:
	GScrollBar *Widget;
	bool Vertical;
	int64 Value, Min, Max, Page;
	GRect Sub, Add, Slide, PageSub, PageAdd;
	int Clicked;
	bool Over;
	int SlideOffset;
	int Ignore;

	GScrollBarPrivate(GScrollBar *w)
	{
		Ignore = 0;
		Widget = w;
		Vertical = true;
		Value = Min = 0;
		Max = -1;
		Page = 1;
		Clicked = BTN_NONE;
		Over = false;
	}

	bool IsVertical()
	{
		return Vertical;
	}

	int IsOver()
	{
		return Over ? Clicked : BTN_NONE;
	}

	void DrawIcon(GSurface *pDC, GRect &r, bool Add, COLOUR c)
	{
		pDC->Colour(c, 24);
		int IconSize = max(r.X(), r.Y()) * 2 / 6;
		int Cx = r.x1 + (r.X() >> 1);
		int Cy = r.y1 + (r.Y() >> 1);
		int Off = (IconSize >> 1) * (Add ? 1 : -1);
		int x = Cx + (IsVertical() ? 0 : Off);
		int y = Cy + (IsVertical() ? Off : 0);

		if (Add)
		{
			if (IsOver() == BTN_ADD)
			{
				x++;
				y++;
			}
			if (IsVertical())
			{
				// down
				for (int i=0; i<IconSize; i++, y--)
				{
					pDC->Line(x-i, y, x+i, y);
				}
			}
			else
			{
				// right
				for (int i=0; i<IconSize; i++, x--)
				{
					pDC->Line(x, y-i, x, y+i);
				}
			}
		}
		else
		{
			if (IsOver() == BTN_SUB)
			{
				x++;
				y++;
			}
			if (IsVertical())
			{
				// up
				for (int i=0; i<IconSize; i++, y++)
				{
					pDC->Line(x-i, y, x+i, y);
				}
			}
			else
			{
				// left
				for (int i=0; i<IconSize; i++, x++)
				{
					pDC->Line(x, y-i, x, y+i);
				}
			}
		}
	}

	void OnPaint(GSurface *pDC)
	{
		// left/up button
		GRect r = Sub;
		DrawBorder(pDC, r, IsOver() == BTN_SUB ? DefaultSunkenEdge : DefaultRaisedEdge);
		pDC->Colour(LC_MED, 24);
		pDC->Rectangle(&r);
		DrawIcon(pDC, r, false, IsValid() ? LC_BLACK : LC_LOW);

		// right/down
		r = Add;
		DrawBorder(pDC, r, IsOver() == BTN_ADD ? DefaultSunkenEdge : DefaultRaisedEdge);
		pDC->Colour(LC_MED, 24);
		pDC->Rectangle(&r);
		DrawIcon(pDC, r, true, IsValid() ? LC_BLACK : LC_LOW);

		COLOUR SlideCol = LC_MED;
		SlideCol = Rgb24(	(255 + R24(SlideCol)) >> 1,
							(255 + G24(SlideCol)) >> 1,
							(255 + B24(SlideCol)) >> 1);

		if (IsValid())
		{
			// slide space
			pDC->Colour(SlideCol, 24);
			pDC->Rectangle(&PageSub);
			pDC->Rectangle(&PageAdd);

			// slide button
			r = Slide;
			DrawBorder(pDC, r, DefaultRaisedEdge); // IsOver() == BTN_SLIDE ? SUNKEN : RAISED);
			pDC->Colour(LC_MED, 24);
			if (r.Valid()) pDC->Rectangle(&r);
		}
		else
		{
			pDC->Colour(SlideCol, 24);
			pDC->Rectangle(&Slide);
		}
	}

	int GetWidth()
	{
		return IsVertical() ? Widget->X() : Widget->Y();
	}

	int GetLength()
	{
		return (IsVertical() ? Widget->Y() : Widget->X()) - (GetWidth() * 2);
	}

	int GetRange()
	{
		return Max >= Min ? Max - Min + 1 : 0;
	}

	bool IsValid()
	{
		return Max >= Min;
	}

	void CalcRegions()
	{
		GRect r = Widget->GetPos();
		Vertical = r.Y() > r.X();

		int w = GetWidth();
		int len = GetLength();
		
		// Button sizes
		Sub.ZOff(w-1, w-1);
		Add.ZOff(w-1, w-1);

		// Button positions
		if (IsVertical())
		{
			Add.Offset(0, Widget->GetPos().Y()-w);
		}
		else
		{
			Add.Offset(Widget->GetPos().X()-w, 0);
		}

		// Slider
		int Start, End;
		#if LGI_SDL
		int MinSize = w; // Touch UI needs large slide....
		#else
		int MinSize = 8;
		#endif

		if (IsValid())
		{
			int Range = GetRange();
			int Size = Range ? min(Page, Range) * len / Range : len;
			if (Size < MinSize) Size = MinSize;
			Start = Range > Page ? Value * (len - Size) / (Range - Page) : 0;
			End = Start + Size;

			if (IsVertical())
			{
				Slide.ZOff(w-1, End-Start-1);
				Slide.Offset(0, Sub.y2+1+Start);

				if (Start > 1)
				{
					PageSub.x1 = Sub.x1;
					PageSub.y1 = Sub.y2 + 1;
					PageSub.x2 = Sub.x2;
					PageSub.y2 = Slide.y1 - 1;
				}
				else
				{
					PageSub.ZOff(-1, -1);
				}

				if (End < Add.y1 - 2)
				{
					PageAdd.x1 = Add.x1;
					PageAdd.x2 = Add.x2;
					PageAdd.y1 = Slide.y2 + 1;
					PageAdd.y2 = Add.y1 - 1;
				}
				else
				{
					PageAdd.ZOff(-1, -1);
				}
			}
			else
			{
				Slide.ZOff(End-Start-1, w-1);
				Slide.Offset(Sub.x2+1+Start, 0);
				
				if (Start > 1)
				{
					PageSub.y1 = Sub.y1;
					PageSub.x1 = Sub.x2 + 1;
					PageSub.y2 = Sub.y2;
					PageSub.x2 = Slide.x1 - 1;
				}
				else
				{
					PageSub.ZOff(-1, -1);
				}

				if (End < Add.x1 - 2)
				{
					PageAdd.y1 = Add.y1;
					PageAdd.y2 = Add.y2;
					PageAdd.x1 = Slide.x2 + 1;
					PageAdd.x2 = Add.x1 - 1;
				}
				else
				{
					PageAdd.ZOff(-1, -1);
				}
			}
		}
		else
		{
			PageAdd.ZOff(-1, -1);
			PageSub.ZOff(-1, -1);

			Slide = Widget->GetClient();
			if (IsVertical())
			{
				Slide.Size(0, Sub.y2 + 1);
			}
			else
			{
				Slide.Size(Sub.x2 + 1, 0);
			}
		}
	}

	int OnHit(int x, int y)
	{
		#if MAC_SKIN
		
		HIThemeTrackDrawInfo Info;
		GRect Client = Widget->GetClient();
		HIRect Rc = Client;
		Info.version = 0;
		Info.kind = kThemeScrollBarMedium;
		Info.bounds = Rc;
		Info.min = Min;
		Info.max = Max - Page;
		Info.value = Value;
		Info.reserved = 0;
		Info.attributes =	(Widget->Vertical() ? 0 : kThemeTrackHorizontal) |
							(Widget->Focus() ? kThemeTrackHasFocus : 0) |
							kThemeTrackShowThumb;
		Info.enableState = Widget->Enabled() ? kThemeTrackActive : kThemeTrackDisabled;
		Info.filler1 = 0;
		Info.trackInfo.scrollbar.viewsize = Page;
		Info.trackInfo.scrollbar.pressState = false;

		HIPoint pt = {x, y};

		ControlPartCode part;

		Boolean b = HIThemeHitTestTrack(&Info, &pt, &part);
		if (b)
		{
			switch (part)
			{
				case kAppearancePartUpButton:	return BTN_SUB;
				case kAppearancePartDownButton:	return BTN_ADD;
				case 129:						return BTN_SLIDE;
				case kControlPageUpPart:		return BTN_PAGE_SUB;
				case kControlPageDownPart:		return BTN_PAGE_ADD;
				default:
					printf("%s:%i - Unknown scroll bar hittest value: %i\n", _FL, part);
					break;
			}
		}
		
		#else
		
		if (Sub.Overlap(x, y)) return BTN_SUB;
		if (Slide.Overlap(x, y)) return BTN_SLIDE;
		if (Add.Overlap(x, y)) return BTN_ADD;
		if (PageSub.Overlap(x, y)) return BTN_PAGE_SUB;
		if (PageAdd.Overlap(x, y)) return BTN_PAGE_ADD;
		
		#endif
		
		return BTN_NONE;
	}

	int OnClick(int Btn, int x, int y)
	{
		if (IsValid())
		{
			switch (Btn)
			{
				case BTN_SUB:
				{
					SetValue(Value-1);
					break;
				}
				case BTN_ADD:
				{
					SetValue(Value+1);
					break;
				}
				case BTN_PAGE_SUB:
				{
					SetValue(Value-Page);
					break;
				}
				case BTN_PAGE_ADD:
				{
					SetValue(Value+Page);
					break;
				}
				case BTN_SLIDE:
				{
					SlideOffset = IsVertical() ? y - Slide.y1 : x - Slide.x1;
					break;
				}
			}
		}
		
		return false;
	}

	void SetValue(int i)
	{
		if (i < Min)
		{
			i = Min;
		}

		if (IsValid() && i > Max - Page + 1)
		{
			i = max(Min, Max - Page + 1);
		}
		
		if (Value != i)
		{
			Value = i;

			CalcRegions();
			Widget->Invalidate();
			
			GViewI *n = Widget->GetNotify() ? Widget->GetNotify() : Widget->GetParent();
			if (n) n->OnNotify(Widget, Value);
		}
	}
};

/////////////////////////////////////////////////////////////////////////////////////
GScrollBar::GScrollBar()
	: ResObject(Res_ScrollBar)
{
	d = new GScrollBarPrivate(this);
}

GScrollBar::GScrollBar(int id, int x, int y, int cx, int cy, const char *name)
	:	ResObject(Res_ScrollBar)	
{
	d = new GScrollBarPrivate(this);
	SetId(id);

	if (name) Name(name);
	if (cx > cy)
	{
		SetVertical(false);
	}
	LgiResources::StyleElement(this);
}

GScrollBar::~GScrollBar()
{
	DeleteObj(d);
}

bool GScrollBar::Valid()
{
	return d->Max > d->Min;
}

int GScrollBar::GetScrollSize()
{
	return SCROLL_BAR_SIZE;
}

bool GScrollBar::Attach(GViewI *p)
{
	bool Status = GControl::Attach(p);
	#if 0
	printf("%p::Attach scroll bar to %s, Status=%i, _View=%p, Vis=%i\n",
		this, p->GetClass(),
		Status,
		_View,
		Visible());
	#endif
	
	return Status;
}

void GScrollBar::OnPaint(GSurface *pDC)
{
	#if MAC_SKIN

	#if 0
	pDC->Colour(GColour(255, 0, 255));
	pDC->Rectangle();
	#endif
	
	HIThemeTrackDrawInfo Info;
	GRect Client = GetClient();
	HIRect Rc = Client;
	Info.version = 0;
	Info.kind = kThemeScrollBarMedium;
	Info.bounds = Rc;
	Info.min = d->Min;
	Info.max = d->Max - d->Page + 1;
	Info.value = d->Value;
	Info.reserved = 0;
	Info.attributes =	(Vertical() ? 0 : kThemeTrackHorizontal) |
						(Focus() ? kThemeTrackHasFocus : 0) |
						kThemeTrackShowThumb;
	Info.enableState = Enabled() ? kThemeTrackActive : kThemeTrackDisabled;
	Info.filler1 = 0;
	Info.trackInfo.scrollbar.viewsize = d->Page;
	Info.trackInfo.scrollbar.pressState = false;
	CGContextRef Cr = pDC->Handle();
	OSStatus e = HIThemeDrawTrack(&Info, NULL, Cr, kHIThemeOrientationNormal);
	if (e)
		printf("%s:%i - HIThemeDrawTrack failed with %li\n", _FL, e);
	
	#else
	d->OnPaint(pDC);
	#endif
}

void GScrollBar::OnPosChange()
{
	d->CalcRegions();
}

void GScrollBar::OnMouseClick(GMouse &m)
{
	if (d->Max >= d->Min)
	{
		int Hit = d->OnHit(m.x, m.y);
		Capture(m.Down());
		if (m.Down())
		{
			if (Hit != d->Clicked)
			{
				d->Clicked = Hit;
				d->Over = true;
				Invalidate();
				d->OnClick(Hit, m.x, m.y);
	
				if (Hit != BTN_SLIDE)
				{
					d->Ignore = 2;
					SetPulse(50);
				}
			}
		}
		else
		{
			if (d->Clicked)
			{
				d->Clicked = BTN_NONE;
				d->Over = false;
				Invalidate();
			}
		}
	}
}

void GScrollBar::OnMouseMove(GMouse &m)
{
	if (IsCapturing())
	{
		if (d->Clicked == BTN_SLIDE)
		{
			if (d->GetLength())
			{
				int Range = d->GetRange();
				int Len = d->GetLength();
				int Size = d->IsVertical() ? d->Slide.Y() : d->Slide.X();
				int Px = (d->IsVertical() ? m.y : m.x) - d->GetWidth() - d->SlideOffset;
				int Value = Px * (Range - d->Page) / (Len - Size);
				d->SetValue(Value);
			}
		}
		else
		{
			int Hit = d->OnHit(m.x, m.y);
			bool Over = Hit == d->Clicked;

			if (Over != d->Over)
			{
				d->Over = Over;
				Invalidate();
			}
		}
	}
}

bool GScrollBar::OnKey(GKey &k)
{
	return false;
}

bool GScrollBar::OnMouseWheel(double Lines)
{
	return false;
}

bool GScrollBar::Vertical()
{
	return d->Vertical;
}

void GScrollBar::SetVertical(bool v)
{
	d->Vertical = v;
	d->CalcRegions();
	Invalidate();
}

int64 GScrollBar::Value()
{
	return d->Value;
}

void GScrollBar::Value(int64 v)
{
	d->SetValue(v);
}

void GScrollBar::Limits(int64 &Low, int64 &High)
{
	Low = d->Min;
	High = d->Max;
}

void GScrollBar::SetLimits(int64 Low, int64 High)
{
	if (d->Min != Low ||
		d->Max != High)
	{
		d->Min = Low;
		d->Max = High;
		d->Page = min(d->Page, d->GetRange());
		d->CalcRegions();

		Invalidate();
		OnConfigure();
	}
}

int GScrollBar::Page()
{
	return d->Page;
}

void GScrollBar::SetPage(int i)
{
	if (d->Page != i)
	{
		d->Page = max(i, 1);
		d->CalcRegions();
		Invalidate();
		OnConfigure();
	}
}

GMessage::Result GScrollBar::OnEvent(GMessage *Msg)
{
	return GView::OnEvent(Msg);
}

void GScrollBar::OnPulse()
{
	if (d->Ignore > 0)
	{
		d->Ignore--;
	}
	else
	{
		GMouse m;
		if (GetMouse(m))
		{
			int Hit = d->OnHit(m.x, m.y);
			if (Hit == d->Clicked)
			{
				d->OnClick(d->Clicked, m.x, m.y);
			}
		}
	}
}
