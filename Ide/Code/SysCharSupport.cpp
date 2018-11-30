#include <stdio.h>

#include "Lgi.h"
#include "LgiIde.h"
#include "LList.h"
#include "GDisplayString.h"

class CharItem : public LListItem
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

	void OnPaintColumn(GItem::ItemPaintCtx &Ctx, int i, GItemColumn *c)
	{
		LListItem::OnPaintColumn(Ctx, i, c);

		if (i == 1)
		{
			Ctx.pDC->Blt(Ctx.x1, Ctx.y1, pDC);
		}
	}

	void OnMeasure(GdcPt2 *Info)
	{
		LListItem::OnMeasure(Info);
		if (pDC)
			Info->y = MAX(Info->y, pDC->Y());
	}
};

class SysCharSupportPriv : public GLgiRes
{
public:
	LList *Match, *NonMatch;

	SysCharSupportPriv()
	{
		Match = NonMatch = 0;
	}

	void Search(char *ch)
	{
		if (ch && Match && NonMatch)
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
				GString::Array Fonts;
				if (s->EnumerateFonts(Fonts))
				{
					for (auto &f : Fonts)
					{
						GFont *Fnt = new GFont;
						if (Fnt)
						{
							Fnt->SubGlyphs(false);

							if (Fnt->Create(f, "16pt"))
							{
								LList *m = _HasUnicodeGlyph(Fnt->GetGlyphMap(), c) ? Match : NonMatch;

								GMemDC *pDC = new GMemDC;
								if (pDC)
								{
									char16 Str[] = { (char16)c, 0 };
									GDisplayString d(Fnt, Str);
									if (pDC->Create(d.X(), d.Y(), System32BitColourSpace))
									{
										pDC->Colour(LC_WORKSPACE, 24);
										pDC->Colour(LC_TEXT, 24);
										d.Draw(pDC, 0, 0);
									}
								}

								LListItem *i;
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

