#include "Lgi.h"
#include "GCssTools.h"

GColour &GCssTools::GetFore(GColour *Default)
{
	if (!ForeInit)
	{
		ForeInit = 1;
		if (Default)
			Fore = *Default;
		else
			Fore.Set(LC_TEXT, 24);

		if (View)
		{
			Fore = View->StyleColour(GCss::PropColor, Fore);
		}
		else if (Css)
		{
			GCss::ColorDef Fill = Css->Color();
			if (Fill.Type == GCss::ColorRgb)
				Fore.Set(Fill.Rgb32, 32);
			else if (Fill.Type == GCss::ColorTransparent)
				Fore.Empty();
		}
	}
	
	return Fore;
}
	
GColour &GCssTools::GetBack(GColour *Default)
{
	if (!BackInit)
	{
		BackInit = 1;

		if (Default)
			Back = *Default;
		else
			Back.Set(LC_MED, 24);

		if (View)
		{
			Back = View->StyleColour(GCss::PropBackgroundColor, Back);
		}
		else if (Css)
		{
			GCss::ColorDef Fill = Css->BackgroundColor();
			if (Fill.Type == GCss::ColorRgb)
				Back.Set(Fill.Rgb32, 32);
			else if (Fill.Type == GCss::ColorTransparent)
				Back.Empty();
		}
	}
	
	return Back;
}

GRect GCssTools::ApplyMargin(GRect &in)
{
	if (!Css)
		return in;

	GRect r = in;
	
	// Insert by the margin
	GCss::Len margin = Css->Padding();
	#define DoMargin(name, edge, box, sign) \
		{ GCss::Len m = Css->Padding##name(); \
		r.edge sign (m.IsValid() ? m : margin).ToPx(in.box(), Font); }
	DoMargin(Left, x1, X, +=)
	DoMargin(Top, y1, Y, +=)
	DoMargin(Right, x2, X, -=)
	DoMargin(Bottom, y2, Y, -=)
	
	return r;
}

GRect GCssTools::GetPadding(GRect &box)
{
	GRect r;
	GCss::Len padding = Css->Padding();
	#define DoPadding(name, edge, dim) \
		{ \
			GCss::Len p = Css->Padding##name(); \
			r.edge = (p.IsValid() ? p : padding).ToPx(box.dim(), Font); \
		}
	DoPadding(Left, x1, X)
	DoPadding(Top, y1, Y)
	DoPadding(Right, x2, X)
	DoPadding(Bottom, y2, Y)
	#undef DoPadding
	return r;
}

GRect GCssTools::GetBorder(GRect &box)
{
	GRect r;
	#define _(name, edge, dim) \
		{ \
			GCss::Len p = Css->Border##name(); \
			r.edge = p.IsValid() ? p.ToPx(box.dim(), Font) : 0; \
		}
	_(Left, x1, X)
	_(Top, y1, Y)
	_(Right, x2, X)
	_(Bottom, y2, Y)
	#undef _
	return r;
}

GRect GCssTools::ApplyBorder(GRect &in)
{
	if (!Css)
		return in;

	// Insert by the padding
	GRect b = GetBorder(in);
	return GRect(in.x1 + b.x1,
				 in.y1 + b.y1,
				 in.x2 - b.x2,
				 in.y2 - b.y2);
}

GRect GCssTools::ApplyPadding(GRect &in)
{
	if (!Css)
		return in;

	// Insert by the padding
	GRect pad = GetPadding(in);
	return GRect(in.x1 + pad.x1,
				 in.y1 + pad.y1,
				 in.x2 - pad.x2,
				 in.y2 - pad.y2);
}

bool GCssTools::SetLineStyle(GSurface *pDC, GCss::BorderDef &b)
{
	if (b.Color.Type == GCss::ColorRgb)
	{
		pDC->Colour(b.Color.Rgb32, 32);
		switch (b.Style)
		{
			case GCss::BorderHidden:
				return false;
			case GCss::BorderDotted:
				pDC->LineStyle(GSurface::LineDot);
				break;
			case GCss::BorderDashed:
				pDC->LineStyle(GSurface::LineDash);
				break;
			default:
				pDC->LineStyle(GSurface::LineSolid);
				break;
		}
	}
	else return false;
	return true;
}

GRect GCssTools::PaintBorder(GSurface *pDC, GRect &in)
{
	GRect Content = in;
	if (!Css)
		return Content;

	// Draw the border
	GCss::BorderDef b = Css->Border();
	bool Drawn = false;
	switch (b.Style)
	{
		default:
		case GCss::BorderNone:
		case GCss::BorderHidden:
			// Do nothing
			break;
		case GCss::BorderInset: // Sunken
		{
			int Px = b.ToPx(Content.X(), Font);
			Drawn = true;
			if (Px == 1)
				LgiThinBorder(pDC, Content, DefaultSunkenEdge);
			else if (Px == 2)
				LgiWideBorder(pDC, Content, DefaultSunkenEdge);
			else
				LgiAssert(!"Unsupported sunken border width");
			break;
		}
		case GCss::BorderOutset: // Raised
		{
			int Px = b.ToPx(Content.X(), Font);
			Drawn = true;
			if (Px == 1)
				LgiThinBorder(pDC, Content, DefaultRaisedEdge);
			else if (Px == 2)
				LgiWideBorder(pDC, Content, DefaultRaisedEdge);
			else
				LgiAssert(!"Unsupported raised border width");
			break;
		}
		case GCss::BorderSolid:
		{
			int Px = b.ToPx(Content.X(), Font);
			if (SetLineStyle(pDC, b))
			{
				Drawn = true;
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
	}

	if (!Drawn)
	{
		// Check for individual borders of different styles...
		GCss::BorderDef Top = Css->BorderTop();
		if (SetLineStyle(pDC, Top))
		{
			int Px = Top.ToPx(Content.Y(), Font);
			for (int i=0; i<Px; i++)
				pDC->HLine(Content.x1, Content.x2, Content.y1++);
		}
		GCss::BorderDef Bottom = Css->BorderBottom();
		if (SetLineStyle(pDC, Bottom))
		{
			int Px = Bottom.ToPx(Content.Y(), Font);
			for (int i=0; i<Px; i++)
				pDC->HLine(Content.x1, Content.x2, Content.y2--);
		}
		GCss::BorderDef Left = Css->BorderLeft();
		if (SetLineStyle(pDC, Left))
		{
			int Px = Left.ToPx(Content.X(), Font);
			for (int i=0; i<Px; i++)
				pDC->VLine(Content.x1++, Content.y1, Content.y2);
		}
		GCss::BorderDef Right = Css->BorderRight();
		if (SetLineStyle(pDC, Right))
		{
			int Px = Right.ToPx(Content.X(), Font);
			for (int i=0; i<Px; i++)
				pDC->VLine(Content.x2--, Content.y1, Content.y2);
		}
		
		pDC->LineStyle(GSurface::LineSolid);
	}

	return Content;
}

GRect GCssTools::PaintPadding(GSurface *pDC, GRect &in)
{
	GRect Content = in;
	if (!Css)
		return Content;

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
		
		Content = r;
	}
	
	return Content;
}
