#include "Lgi.h"
#include "GBox.h"

struct GBoxPriv
{
};

GBox::GBox(int Id)
{
	d = new GBoxPriv;
}

GBox::~GBox()
{
	DeleteObj(d);
}

void GBox::OnCreate()
{
	AttachChildren();
	OnPosChange();
}

void GBox::OnPaint(GSurface *pDC)
{
	GRect cli = GetClient();
	GCss *css = GetCss();
	if (css)
	{
		GCss::BorderDef b = css->Border();
		switch (b.Style)
		{
			case GCss::BorderNone:
			case GCss::BorderHidden:
				// Do nothing
				break;
			case GCss::BorderInset: // Sunken
			{
				int Px = b.ToPx(cli.X(), GetFont());
				if (Px == 1)
					LgiThinBorder(pDC, cli, SUNKEN);
				else if (Px == 2)
					LgiWideBorder(pDC, cli, SUNKEN);
				else
					LgiAssert(!"Unsupported sunken border width");
				break;
			}
			case GCss::BorderOutset: // Raised
			{
				int Px = b.ToPx(cli.X(), GetFont());
				if (Px == 1)
					LgiThinBorder(pDC, cli, RAISED);
				else if (Px == 2)
					LgiWideBorder(pDC, cli, RAISED);
				else
					LgiAssert(!"Unsupported raised border width");
				break;
			}
			case GCss::BorderSolid:
			{
				int Px = b.ToPx(cli.X(), GetFont());
				if (b.Color.Type == GCss::ColorRgb)
				{
					pDC->Colour(b.Color.Rgb32, 32);
					for (int i=0; i<Px; i++)
					{
						pDC->Box(&cli);
						cli.Size(1, 1);
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

		GRect content = cli;		
		GCss::Len pad_left = css->PaddingLeft();
		content.x1 += pad_left.ToPx(cli.X(), GetFont());
		GCss::Len pad_top = css->PaddingTop();
		content.y1 += pad_top.ToPx(cli.Y(), GetFont());
		GCss::Len pad_right = css->PaddingRight();
		content.x2 -= pad_right.ToPx(cli.X(), GetFont());
		GCss::Len pad_bottom = css->PaddingBottom();
		content.y2 -= pad_bottom.ToPx(cli.Y(), GetFont());
	}
}

void GBox::OnPosChange()
{
}
