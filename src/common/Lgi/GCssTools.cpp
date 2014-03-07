#include "Lgi.h"
#include "GCssTools.h"

GRect GCssTools::ApplyMargin(GRect &in)
{
	if (!Css)
		return in;

	GRect r = in;
	
	// Insert by the margin
	GCss::Len margin = Css->Margin();
	#define DoMargin(name, edge, box, sign) \
		{ GCss::Len m = Css->Margin##name(); \
		r.edge sign (m.IsValid() ? m : margin).ToPx(in.box(), Font); }
	DoMargin(Left, x1, X, +=)
	DoMargin(Top, y1, Y, +=)
	DoMargin(Right, x2, X, -=)
	DoMargin(Bottom, y2, Y, -=)
	
	return r;
}

GRect GCssTools::ApplyPadding(GRect &in)
{
	if (!Css)
		return in;

	// Insert by the padding
	GRect r = in;
	GCss::Len padding = Css->Padding();
	#define DoPadding(name, edge, box, sign) \
		{ GCss::Len p = Css->Padding##name(); \
		r.edge sign (p.IsValid() ? p : padding).ToPx(in.box(), Font); }
	DoPadding(Left, x1, X, +=)
	DoPadding(Top, y1, Y, +=)
	DoPadding(Right, x2, X, -=)
	DoPadding(Bottom, y2, Y, -=)

	return r;
}

GRect GCssTools::PaintBorderAndPadding(GSurface *pDC, GRect &in)
{
	GRect Content = in;
	if (!Css)
		return Content;

	// Draw the border
	GCss::BorderDef b = Css->Border();
	switch (b.Style)
	{
		case GCss::BorderNone:
		case GCss::BorderHidden:
			// Do nothing
			break;
		case GCss::BorderInset: // Sunken
		{
			int Px = b.ToPx(Content.X(), Font);
			if (Px == 1)
				LgiThinBorder(pDC, Content, SUNKEN);
			else if (Px == 2)
				LgiWideBorder(pDC, Content, SUNKEN);
			else
				LgiAssert(!"Unsupported sunken border width");
			break;
		}
		case GCss::BorderOutset: // Raised
		{
			int Px = b.ToPx(Content.X(), Font);
			if (Px == 1)
				LgiThinBorder(pDC, Content, RAISED);
			else if (Px == 2)
				LgiWideBorder(pDC, Content, RAISED);
			else
				LgiAssert(!"Unsupported raised border width");
			break;
		}
		case GCss::BorderSolid:
		{
			int Px = b.ToPx(Content.X(), Font);
			if (b.Color.Type == GCss::ColorRgb)
			{
				pDC->Colour(b.Color.Rgb32, 32);
				for (int i=0; i<Px; i++)
				{
					pDC->Box(&Content);
					Content.Size(1, 1);
				}
			}
			break;
		}
		/*
		case BorderDotted:
			break;
		case BorderDashed:
			break;
		case BorderDouble:
			break;
		case BorderGroove:
			break;
		case BorderRidge:
			break;
		*/
		default:
		{
			LgiAssert(!"Impl me.");
			break;
		}
	}

	// Insert by the padding
	GRect r = ApplyPadding(Content);
	if (r != Content)
	{
		// Draw the padding in the background colour
		GCss::ColorDef BackgroundDef = Css->BackgroundColor();
		GColour Background;
		if (BackgroundDef.IsValid())
		{
			if (BackgroundDef.Type == GCss::ColorRgb)
				Background.c32(BackgroundDef.Rgb32);
			else
				LgiAssert(!"Unsupported");
		}
		else
			Background.c24(LC_MED);
		
		pDC->Colour(Background);
		if (r.x1 > Content.x1)
			pDC->Rectangle(Content.x1, Content.y1, r.x1 - 1, Content.y2);
		if (r.x2 < Content.x2)
			pDC->Rectangle(r.x2 + 1, Content.y1, Content.x2, Content.y2);
		if (r.y1 > Content.y1)
			pDC->Rectangle(r.x1, Content.y1, r.x2, r.y1 - 1);
		if (r.y2 < Content.y2)
			pDC->Rectangle(r.x1, r.y2 + 1, r.x2, Content.y2);
	}
	
	return Content;
}
