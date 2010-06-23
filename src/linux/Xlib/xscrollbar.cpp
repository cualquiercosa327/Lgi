
#include "Gdc2.h"
#include "LgiCommon.h"
#include "xscrollbar.h"
#include "GRect.h"

LgiFunc COLOUR LgiColour(int Colour);
LgiFunc void LgiThinBorder(GSurface *pDC, GRect &r, int Type);

//#define DrawBorder(dc, r, edge) LgiWideBorder(dc, r, edge)
#define DrawBorder(dc, r, edge) LgiThinBorder(dc, r, edge)

#define BTN_NONE			0
#define BTN_SUB				1
#define BTN_SLIDE			2
#define BTN_ADD				3
#define BTN_PAGE_SUB		4
#define BTN_PAGE_ADD		5

class XScrollBarPrivate : public XObject
{
public:
	XWidget *Widget;
	XScrollBar::Orientation Dir;
	int Value, Min, Max, Page;
	GRect Pos, Sub, Add, Slide, PageSub, PageAdd;
	int Clicked;
	bool Over;
	int SlideOffset;

	XScrollBarPrivate(XWidget *This)
	{
		Widget = This;
		Dir = XScrollBar::Vertical;
		Value = Min = Max = 0;
		Page = 1;
		Clicked = BTN_NONE;
		Over = false;
	}

	bool IsVertical()
	{
		return Dir == XScrollBar::Vertical;
	}

	int IsOver()
	{
		return Over ? Clicked : BTN_NONE;
	}

	void DrawIcon(GSurface *pDC, GRect &r, bool Add, COLOUR c)
	{
		pDC->Colour(c, 24);
		int IconSize = max(r.X(), r.Y()) / 3;
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
		COLOUR Dark = Rgb24(32,32,32);

		// Border
		GRect r = Pos;
		pDC->Colour(Dark, 24);

		// control outline
		pDC->Box(&r);

		// between buttons and slider area
		if (IsVertical())
		{
			pDC->Line(Sub.x1, Sub.y2+1, Sub.x2, Sub.y2+1);
			pDC->Line(Add.x1, Add.y1-1, Add.x2, Add.y1-1);
		}
		else
		{
			pDC->Line(Sub.x2+1, Sub.y1, Sub.x2+1, Sub.y2);
			pDC->Line(Add.x1-1, Add.y1, Add.x1-1, Add.y2);
		}

		// left/up button
		r = Sub;
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

		if (IsValid())
		{
			// slide space
			pDC->Colour(Rgb24(0xb8, 0xb8, 0xb8), 24);
			pDC->Rectangle(&PageSub);
			pDC->Rectangle(&PageAdd);

			// slide button
			r = Slide;
			pDC->Colour(Dark, 24);
			pDC->Box(&r);
			r.Size(1, 1);
			DrawBorder(pDC, r, IsOver() == BTN_SLIDE ? SUNKEN : RAISED);
			pDC->Colour(LC_MED, 24);
			if (r.Valid()) pDC->Rectangle(&r);
		}
		else
		{
			pDC->Colour(LC_MED, 24);
			pDC->Rectangle(&Slide);
		}
	}

	void OnPos(GRect &p)
	{
		Pos.Set(0, 0, p.X()-1, p.Y()-1);
		CalcRegions();
	}

	int GetWidth()
	{
		return IsVertical() ? Widget->width() : Widget->height();
	}

	int GetLength()
	{
		return (IsVertical() ? Pos.Y() : Pos.X()) - (GetWidth() * 2) + 2;

		/*
		if (IsVertical())
		{
			return (Add.y1 - 1) - (Sub.y2 + 1);
		}
		else
		{
			return (Add.x1 - 1) - (Sub.x2 + 1);
		}
		*/
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
		Sub.ZOff(w-3, w-3);
		Add.ZOff(w-3, w-3);

		// Button positions
		Sub.Offset(1, 1);
		if (IsVertical())
		{
			Add.Offset(1, Pos.y2-(w-2));
		}
		else
		{
			Add.Offset(Pos.x2-(w-2), 1);
		}

		// Slider
		int Start, End;
		if (IsValid())
		{
			int Range = GetRange();
			int Size = Range ? Page * GetLength() / Range : GetLength();
			if (Size < 8) Size = 8;
			Start = Range > Page ? Value * (GetLength() - Size) / (Range - Page) : 0;
			End = Start + Size;

/*
printf("::CalcRgn Vert=%i Value=%i Min=%i Max=%i Page=%i Pos=%s Len=%i Range=%i (f=%i w=%i)\n", IsVertical(), Value, Min, Max, Page, Pos.Describe(), GetLength(), GetRange(), IsVertical() ? Pos.Y() : Pos.X(), GetWidth());
printf("\tSize=%i Start=%i End=%i Add=%s\n", Size, Start, End, Add.Describe());
*/

			if (IsVertical())
			{
				Slide.ZOff(w-1, End-Start-1);
				Slide.Offset(0, Sub.y2+1+Start);

				if (Start > 1)
				{
					PageSub.x1 = Sub.x1;
					PageSub.y1 = Sub.y2 + 2;
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
					PageAdd.y2 = Add.y1 - 2;
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
					PageSub.y2 = Sub.y2;
					PageSub.x1 = Sub.x2 + 2;
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
					PageAdd.x2 = Add.x1 - 2;
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

			Slide = Pos;
			if (IsVertical())
			{
				Slide.Size(1, Sub.y2 + 2);
			}
			else
			{
				Slide.Size(Sub.x2 + 2, 1);
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

			// printf("SetValue v=%i min=%i max=%i page=%i\n", Value, Min, Max, Page);

			CalcRegions();
			Widget->update();
			Widget->notifyEvent(Value);
		}
	}
};

/////////////////////////////////////////////////////////////////////////////////////
XScrollBar::XScrollBar(XWidget *p, char *name) : XWidget(p->handle(), name)
{
	d = NEW(XScrollBarPrivate(this));
}

XScrollBar::~XScrollBar()
{
	DeleteObj(d);
}

void XScrollBar::paintEvent(XlibEvent *e)
{
	GScreenDC Dc(this);
	d->OnPaint(&Dc);
}

void XScrollBar::resizeEvent(XlibEvent *e)
{
	GRect p = geometry();
	d->OnPos(p);
}

void XScrollBar::mousePressEvent(XlibEvent *e)
{
	int Hit = d->OnHit(e->x(), e->y());
	if (Hit != d->Clicked)
	{
		d->Clicked = Hit;
		d->Over = true;
		update();
		grabMouse();

		d->OnClick(Hit, e->x(), e->y());
	}
}

void XScrollBar::mouseReleaseEvent(XlibEvent *e)
{
	int Hit = d->OnHit(e->x(), e->y());
	if (d->Clicked)
	{
		d->Clicked = BTN_NONE;
		d->Over = false;
		update();
		ungrabMouse();
	}
}

void XScrollBar::mouseMoveEvent(XlibEvent *e)
{
	if (e->button() & (XWidget::LeftButton | XWidget::RightButton))
	{
		if (d->Clicked == BTN_SLIDE)
		{
			if (d->GetLength())
			{
				int Px = (d->IsVertical() ? e->y() : e->x()) - d->GetWidth();
				int Off = Px * d->GetRange() / d->GetLength();
				int Thumb = d->SlideOffset * d->GetRange() / d->GetLength();
				d->SetValue(Off - Thumb);
			}
		}
		else
		{
			int Hit = d->OnHit(e->x(), e->y());
			bool Over = Hit == d->Clicked;

			// printf("XScrollBar::mouseMoveEvent x,y=%i,%i Hit=%i d->Clicked=%i Over=%i d->Over=%i PageAdd=%s\n", e->x(), e->y(), Hit, d->Clicked, Over, d->Over, d->PageAdd.Describe());
			if (Over != d->Over)
			{
				d->Over = Over;
				update();
			}
		}
	}
}

bool XScrollBar::keyPressEvent(XlibEvent *e)
{
}

void XScrollBar::wheelEvent(XlibEvent *e)
{
}

//////////////////////////////////////////////////
XScrollBar::Orientation XScrollBar::orientation()
{
	return d->Dir;
}

void XScrollBar::setOrientation(Orientation o)
{
	if (d->Dir != o)
	{
		d->Dir = o;
		d->CalcRegions();
	}
}

int XScrollBar::value()
{
	return d->Value;
}

void XScrollBar::setValue(int v)
{
	d->SetValue(v);
}

int XScrollBar::minValue()
{
	return d->Min;
}

int XScrollBar::maxValue()
{
	return d->Max;
}

void XScrollBar::setRange(int min, int max)
{
	if (d->Min != min OR
		d->Max != max)
	{
		d->Min = min;
		d->Max = max;
		d->CalcRegions();
		update();
	}
}

int XScrollBar::pageStep()
{
	return d->Page;
}

void XScrollBar::setPageStep(int i)
{
	if (d->Page != i)
	{
		d->Page = max(i, 1);
		d->CalcRegions();
		update();
	}
}


