#include <stdio.h>

#include "Lgi.h"
#include "LgiIde.h"
#include "GList.h"
#include "GUtf8.h"

class CharItem : public GListItem
{
	GSurface *pDC;

public:
	CharItem(GSurface *pdc)
	{
		pDC = pdc;
	}

	~CharItem()
	{
		DeleteObj(pDC);
	}

	void OnPaintColumn(GItem::ItemPaintCtx &Ctx, int i, GListColumn *c)
	{
		GListItem::OnPaintColumn(Ctx, i, c);

		if (i == 1)
		{
			Ctx.pDC->Blt(Ctx.x1, Ctx.y1, pDC);
		}
	}

	void OnMeasure(GMeasureInfo *Info)
	{
		GListItem::OnMeasure(Info);
		if (pDC)
			Info->y = max(Info->y, pDC->Y());
	}
};

class SysCharSupportPriv : public GLgiRes
{
public:
	GList *Match, *NonMatch;

	SysCharSupportPriv()
	{
		Match = NonMatch = 0;
	}

	void Search(char *ch)
	{
		if (ch AND Match AND NonMatch)
		{
			Match->Empty();
			NonMatch->Empty();

			GUtf8Str Utf(ch);
			uint32 c = Utf;

			char msg[256];
			sprintf(msg, "%i 0x%x", c, c);
			Match->GetWindow()->SetCtrlName(IDC_VALUE, msg);

			GFontSystem *s = GFontSystem::Inst();
			if (s)
			{
				List<const char> Fonts;
				if (s->EnumerateFonts(Fonts))
				{
					for (const char *f=Fonts.First(); f; f=Fonts.Next())
					{
						GFont *Fnt = new GFont;
						if (Fnt)
						{
							Fnt->SubGlyphs(false);

							if (Fnt->Create(f, 16))
							{
								GList *m = _HasUnicodeGlyph(Fnt->GetGlyphMap(), c) ? Match : NonMatch;

								GMemDC *pDC = new GMemDC;
								if (pDC)
								{
									char16 Str[] = { c, 0 };
									GDisplayString d(Fnt, Str);
									if (pDC->Create(d.X(), d.Y(), 32))
									{
										pDC->Colour(LC_WORKSPACE, 24);
										pDC->Colour(LC_TEXT, 24);
										d.Draw(pDC, 0, 0);
									}
								}

								GListItem *i;
								m->Insert(i = new CharItem(pDC));
								if (i)
								{
									i->SetText(f);
								}
							}

							DeleteObj(Fnt);
						}
					}
				}				
			}
		}
	}
};

SysCharSupport::SysCharSupport(AppWnd *app)
{
	d = new SysCharSupportPriv;
	Name("System Character Support");

	if (Attach(0))
	{
		d->LoadFromResource(IDD_CHAR, this, &GetPos());
		MoveToCenter();
		AttachChildren();
		GetViewById(IDC_MATCH, d->Match);
		GetViewById(IDC_NOT_MATCH, d->NonMatch);
		OnPosChange();

		Visible(true);
	}
}

SysCharSupport::~SysCharSupport()
{
	DeleteObj(d);
}

void SysCharSupport::OnPosChange()
{
	/*
	if (d->Match)
	{
		GRect c = GetClient();
		GRect r = d->Lst->GetPos();
		r.x2 = c.x2 - r.x1;
		r.y2 = c.y2 - r.x1;
		d->Lst->SetPos(r);
	}
	*/
}

int SysCharSupport::OnNotify(GViewI *v, int f)
{
	switch (v->GetId())
	{
		case IDOK:
		{
			d->Search(GetCtrlName(IDC_CHAR));
			break;
		}
	}

	return 0;
}

