#include "Lgi.h"
#include "GScrollBar.h"

LgiFunc COLOUR LgiColour(int Colour);
LgiFunc void LgiThinBorder(GSurface *pDC, GRect &r, int Type);
LgiFunc void LgiWideBorder(GSurface *pDC, GRect &r, int Type);

#define DrawBorder(dc, r, edge) LgiWideBorder(dc, r, edge)
// #define DrawBorder(dc, r, edge) LgiThinBorder(dc, r, edge)

#define BTN_NONE			0
#define BTN_SUB				1
#define BTN_SLIDE			2
#define BTN_ADD				3
#define BTN_PAGE_SUB		4
#define BTN_PAGE_ADD		5

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
		int IconSize = max(r.X(), r.Y()) * 2 / 5;
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
		DrawBorder(pDC, r, IsOver() == BTN_SUB ? SUNKEN : RAISED);
		pDC->Colour(LC_MED, 24);
		pDC->Rectangle(&r);
		DrawIcon(pDC, r, false, IsValid() ? LC_BLACK : LC_LOW);

		// right/down
		r = Add;
		DrawBorder(pDC, r, IsOver() == BTN_ADD ? SUNKEN : RAISED);
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
			DrawBorder(pDC, r, RAISED); // IsOver() == BTN_SLIDE ? SUNKEN : RAISED);
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
		int w = GetWidth();
		
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
		if (IsValid())
		{
			int Range = GetRange();
			int Size = Range ? min(Page, Range) * GetLength() / Range : GetLength();
			if (Size < 8) Size = 8;
			Start = Range > Page ? Value * (GetLength() - Size) / (Range - Page) : 0;
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
				/*
				printf("::CalcRgn Vert=%i Value=%i Min=%i Max=%i Page=%i Pos=%s Len=%i Range=%i (f=%i w=%i)\n", IsVertical(), Value, Min, Max, Page, Widget->GetPos().Describe(), GetLength(), GetRange(), IsVertical() ? Widget->Y() : Widget->X(), GetWidth());
				printf("\tSize=%i Start=%i End=%i Add=%s\n", Size, Start, End, Add.Describe());
				*/

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

				/*
				printf("H slide=%s ", Slide.Describe());
				printf("sub=%s ", Sub.Describe());
				printf("add=%s ", Add.Describe());
				printf("GetLength()=%i\n", GetLength());
				*/
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
		if (Sub.Overlap(x, y)) return BTN_SUB;
		if (Slide.Overlap(x, y)) return BTN_SLIDE;
		if (Add.Overlap(x, y)) return BTN_ADD;
		if (PageSub.Overlap(x, y)) return BTN_PAGE_SUB;
		if (PageAdd.Overlap(x, y)) return BTN_PAGE_ADD;
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

		if (IsValid() AND i > Max - Page + 1)
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
	if (X() > Y())
	{
		SetVertical(false);
	}

	return GControl::Attach(p);
}

void GScrollBar::OnPaint(GSurface *pDC)
{
	d->OnPaint(pDC);
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
				int SlideSize = d->IsVertical() ? d->Slide.Y() : d->Slide.X();
				int Px = (d->IsVertical() ? m.y : m.x) - d->GetWidth() - d->SlideOffset;
				int Off = Px * d->GetRange() / d->GetLength();
				d->SetValue(Off);
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

GMessage::Param GScrollBar::OnEvent(GMessage *Msg)
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
