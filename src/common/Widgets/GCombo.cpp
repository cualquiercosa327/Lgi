#include <stdlib.h>
#include <stdio.h>

#include "Lgi.h"
#include "GVariant.h"
#include "GSkinEngine.h"
#include "GCombo.h"
#include "GDisplayString.h"
#include "LgiRes.h"

#define COMBO_HEIGHT			20

int IntCompare(char *a, char *b, NativeInt c)
{
	int A = atoi(a);
	int B = atoi(b);
	return A - B;
}

int DblCompare(char *a, char *b, NativeInt c)
{
	double A = atof(a);
	double B = atof(b);
	return A > B ? 1 : A < B ? -1 : 0;
}

int StringCompare(char *a, char *b, NativeInt c)
{
	return stricmp(a, b);
}

class GComboPrivate
{
	GDisplayString *Text;

public:
	ssize_t Current;
	bool SortItems;
	int Sub;
	GRect Arrow;
	List<char> Items;
	uint64 LastKey;
	GAutoString Find;
	GSubMenu *Menu;
	GCombo::SelectedState SelState;
	bool LayoutDirty;

	#if defined MAC && !defined COCOA
	ThemeButtonDrawInfo Cur;
	#endif

	GComboPrivate()
	{
		LayoutDirty = false;
		SelState = GCombo::SelectedDisable;
		SortItems = false;
		Sub = false;
		Current = 0;
		LastKey = 0;
		Menu = 0;
		Text = 0;

		#if defined MAC && !defined COCOA
		Cur.state = kThemeStateInactive;
		Cur.value = kThemeButtonOff;
		Cur.adornment = kThemeAdornmentNone;
		#endif
	}

	~GComboPrivate()
	{
		Items.DeleteArrays();
		DeleteObj(Menu);
		DeleteObj(Text);
	}
	
	GDisplayString *GetText(const char *File, int Line)
	{
		return Text;
	}

	void SetText(GDisplayString *ds, const char *File, int Line)
	{
		DeleteObj(Text);
		Text = ds;
	}
	
	GDisplayString **GetTextPtr()
	{
		return &Text;
	}
};

GRect GCombo::Pad(8, 4, 24, 4);

GCombo::GCombo(int id, int x, int y, int cx, int cy, const char *name) :
	ResObject(Res_ComboBox)
{
	d = new GComboPrivate;
	
	SetId(id);
	GRect e(x, y, x+cx, y+cy);
	SetPos(e);
	Name(name);
	SetTabStop(true);

	#if WINNATIVE
	SetDlgCode(DLGC_WANTARROWS);
	SetStyle(GetStyle() | WS_CHILD | WS_VISIBLE);
	#endif
	LgiResources::StyleElement(this);
}

GCombo::~GCombo()
{
	DeleteObj(d);
}

bool GCombo::Sort()
{
	return d->SortItems;
}

void GCombo::Sort(bool s)
{
	d->SortItems = s;
}

int GCombo::Sub()
{
	return d->Sub;
}

void GCombo::Sub(int s) // GV_???
{
	d->Sub = s;
}

GSubMenu *GCombo::GetMenu()
{
	return d->Menu;
}

void GCombo::SetMenu(GSubMenu *m)
{
	d->Menu = m;
}

size_t GCombo::Length()
{
	return d->Items.Length();
}

char *GCombo::operator [](int i)
{
	return d->Items.ItemAt(i);
}

bool GCombo::Name(const char *n)
{
	if (ValidStr(n))
	{
		int Index = 0;
		for (char *i=d->Items.First(); i; i=d->Items.Next(), Index++)
		{
			if (stricmp(n, i) == 0)
			{
				Value(Index);
				return true;
			}
		}

		char *New = NewStr(n);
		if (New)
		{
			d->Items.Insert(New);
			d->Current = d->Items.Length() - 1;
			d->SetText(NULL, _FL);
			Invalidate();
		}
		else return false;
	}
	else
	{
		d->Current = 0;
		d->SetText(NULL, _FL);
		Invalidate();
	}

	return true;
}

char *GCombo::Name()
{
	return d->Items.ItemAt(d->Current);
}

void GCombo::Value(int64 i)
{
	if (d->Current != i)
	{
		d->Current = (int)i;
		d->SetText(NULL, _FL);
		Invalidate();

		SendNotify((int)d->Current);
	}
}

int64 GCombo::Value()
{
	return d->Current;
}

GMessage::Result GCombo::OnEvent(GMessage *Msg)
{
	switch (MsgCode(Msg))
	{
	}

	return GView::OnEvent(Msg);
}

void GCombo::Empty()
{
	while (Delete((size_t)0))
		;
	Name(0);
}

bool GCombo::Delete()
{
	return Delete(d->Current);
}

bool GCombo::Delete(size_t i)
{
	char *c = d->Items.ItemAt(i);
	if (c)
	{
		d->Items.Delete(c);
		DeleteArray(c);
		return true;
	}
	return false;
}

bool GCombo::Delete(char *p)
{
	if (p && d->Items.HasItem(p))
	{
		d->Items.Delete(p);
		DeleteArray(p);
		d->LayoutDirty = true;
		return true;
	}
	return false;
}

bool GCombo::Insert(const char *p, int Index)
{
	bool Status = false;
	if (p)
	{
		Status = d->Items.Insert(NewStr(p), Index);
		if (Status)
		{
			d->LayoutDirty = true;
			Invalidate();
		}
	}
	return Status;
}

void GCombo::DoMenu()
{
	GdcPt2 p(0, Y());
	PointToScreen(p);

	if (d->Menu)
	{
		int Result = d->Menu->Float(this, p.x, p.y, GSubMenu::BtnLeft);
		if (Result)
		{
			GetWindow()->OnCommand(Result, 0, Handle());
		}
	}
	else
	{
		GSubMenu RClick;
		int Base = 1000;
		int i=0;

		if (d->Sub)
		{
			if (d->Sub == GV_INT32)
			{
				d->Items.Sort(IntCompare);
			}
			else if (d->Sub == GV_DOUBLE)
			{
				d->Items.Sort(DblCompare);
			}
			else
			{
				d->Items.Sort(StringCompare);
			}
		}
		else if (d->SortItems)
		{
			d->Items.Sort(StringCompare);
		}

		char *c = d->Items.First();
		if (c)
		{
			if (d->Sub)
			{
				int n = 0;
				double dbl = 0;
				char16 f = 0;
				GSubMenu *m = 0;
				for (; c; c = d->Items.Next(), i++)
				{
					GUtf8Ptr u(c);
					if (d->Sub == GV_INT32)
					{
						int ci = atoi(c);
						if (!m || n != ci)
						{
							char Name[16];
							sprintf_s(Name, sizeof(Name), "%i...", n = ci);
							m = RClick.AppendSub(Name);
						}
					}
					else if (d->Sub == GV_DOUBLE)
					{
						double cd = atof(c);
						if (!m || dbl != cd)
						{
							char Name[16];
							sprintf_s(Name, sizeof(Name), "%f", dbl = cd);
							char *e = Name+strlen(Name)-1;
							while (e > Name && e[0] == '0') *e-- = 0;
							strcat(Name, "...");
							m = RClick.AppendSub(Name);
						}
					}
					else
					{
						if (!m || f != u)
						{
							char16 Name[16], *n = Name;
							*n++ = (f = u);
							*n++ = '.';
							*n++ = '.';
							*n++ = '.';
							*n++ = 0;
							GAutoString a(WideToUtf8(Name));
							m = RClick.AppendSub(a);
						}
					}

					switch (d->SelState)
					{
						default:
							m->AppendItem(c, Base+i, i != d->Current);
							break;
						case SelectedHide:
							if (i != d->Current)
								m->AppendItem(c, Base+i, true);
							break;
						case SelectedShow:
							m->AppendItem(c, Base+i, true);
							break;
					}
				}
			}
			else
			{
				for (; c; c = d->Items.Next(), i++)
				{
					switch (d->SelState)
					{
						default:
							RClick.AppendItem(c, Base+i, i != d->Current);
							break;
						case SelectedHide:
							if (i != d->Current)
								RClick.AppendItem(c, Base+i, true);
							break;
						case SelectedShow:
							RClick.AppendItem(c, Base+i, true);
							break;
					}
				}
			}
		}
		else
		{
			RClick.AppendItem("", Base+i, false);
		}

		int Result = RClick.Float(this, p.x, p.y, GSubMenu::BtnLeft);
		if (Result >= Base)
		{
			d->Current = Result - Base;
			d->SetText(NULL, _FL);
			Invalidate();

			SendNotify((int)d->Current);
		}
	}
}

void GCombo::OnMouseClick(GMouse &m)
{
	if (m.Down() &&
		Enabled())
	{
		Focus(true);

		if (m.Left())
		{
			DoMenu();
		}
	}
}

bool GCombo::OnKey(GKey &k)
{
	bool Status = false;

	if (k.Down())
	{
		switch (k.vkey)
		{
			case ' ':
			{
				// Open the submenu
				Status = true;
				DoMenu();
				break;
			}
			case VK_UP:
			{
				// Previous value in the list
				int64 i = Value();
				if (i > 0)
				{
					Value(i-1);
				}
				Status = true;
				break;
			}
			case VK_DOWN:
			{
				// Next value in the list...
				int i = (int)Value();
				if (i < d->Items.Length()-1)
				{
					Value(i+1);
				}
				Status = true;
				break;
			}
			default:
			{
				// Search for value
				if (k.IsChar && k.c16 > ' ')
				{
					uint64 Now = LgiCurrentTime();
					if (d->LastKey + 2000 < Now)
					{
						d->Find.Reset();
					}

					size_t Len = d->Find ? strlen(d->Find) : 0;
					GAutoString n(new char[Len+2]);
					if (n)
					{
						if (d->Find) strcpy(n, d->Find);
						n[Len++] = k.c16;
						n[Len] = 0;
						d->Find = n;
					}

					if (Len > 0 && d->Find)
					{
						int Index = 0;
						for (char *i=d->Items.First(); i; i=d->Items.Next(), Index++)
						{
							if (strnicmp(i, d->Find, Len) == 0)
							{
								Value(Index);
								break;
							}
						}
					}
					d->LastKey = Now;
					Status = true;
				}
				break;
			}
		}
	}

	return Status;
}

void GCombo::OnFocus(bool f)
{
	Invalidate();
}

void GCombo::SetFont(GFont *Fnt, bool OwnIt)
{
	GView::SetFont(Fnt, OwnIt);
	d->SetText(new GDisplayString(GetFont(), Name()), _FL);
	Invalidate();
}

void GCombo::OnPosChange()
{
	GDisplayString *ds = d->GetText(_FL);
	if (ds && ds->IsTruncated())
		d->SetText(NULL, _FL);
}

void GCombo::OnPaint(GSurface *pDC)
{
	if (d->LayoutDirty)
	{
		d->LayoutDirty = false;
		SendNotify(GNotifyTableLayout_Refresh);
	}
	
	if (!d->GetText(_FL))
	{
		char *n = Name();
		if (n)
			d->SetText(new GDisplayString(GetFont(), n), _FL);
	}

	GColour cBack = StyleColour(GCss::PropBackgroundColor, GColour(LC_MED, 24));

	#if defined MAC && !defined COCOA && !defined(LGI_SDL)

		pDC->Colour(cBack);
		pDC->Rectangle();

		GRect Cli = GetClient();
		GRect c = Cli;
	
		c.Size(1, 1);
		c.y2 -= 1;
		HIRect Bounds = c;
		HIThemeButtonDrawInfo Info;
		HIRect LabelRect;

		Info.version = 0;
		Info.state = Enabled() ? kThemeStateActive : kThemeStateInactive;
		Info.kind = kThemePopupButton;
		Info.value = Focus() ? kThemeButtonOn : kThemeButtonOff;
		Info.adornment = Focus() ? kThemeAdornmentFocus : kThemeAdornmentNone;

		OSStatus e = HIThemeDrawButton(	  &Bounds,
										  &Info,
										  pDC->Handle(),
										  kHIThemeOrientationNormal,
										  &LabelRect);

		if (e) printf("%s:%i - HIThemeDrawButton failed %li\n", _FL, e);
		else if (d->GetText(_FL))
		{
			GRect Txt;
			Txt = LabelRect;
			GDisplayString *Ds = d->GetText(_FL);
			int y = Cli.y1 + ((Cli.Y() - Ds->Y()) / 2) + 1;

			auto f = Ds->GetFont();
			f->Transparent(true);
			f->Fore(LC_TEXT);
			Ds->Draw(pDC, Txt.x1, y);
			
			#if 0
			pDC->Colour(GColour(255, 0, 0));
			pDC->Box(Txt.x1, y, Txt.x1 + Ds->X()-1, y + Ds->Y() - 1);
			#endif
		}
	
	#else
	
	if (GApp::SkinEngine &&
		TestFlag(GApp::SkinEngine->GetFeatures(), GSKIN_COMBO))
	{
		GSkinState State;
		State.pScreen = pDC;
		State.ptrText = d->GetTextPtr();
		State.Enabled = Enabled();
		GApp::SkinEngine->OnPaint_GCombo(this, &State);
	}
	else
	{
		GRect r(0, 0, X()-1, Y()-1);

		// draw shadowed border
		pDC->Colour(LC_SHADOW, 24);
		pDC->Line(r.x1+2, r.y2, r.x2, r.y2);
		pDC->Line(r.x2, r.y1+2, r.x2, r.y2);

		pDC->Colour(LC_MED, 24);
		pDC->Set(r.x1, r.y2);
		pDC->Set(r.x1+1, r.y2);
		pDC->Set(r.x2, r.y1);
		pDC->Set(r.x2, r.y1+1);

		r.x2--;
		r.y2--;
		pDC->Colour(LC_LOW, 24);
		pDC->Box(&r);
		r.Size(1, 1);
		pDC->Colour(LC_HIGH, 24);
		pDC->Line(r.x1, r.y1, r.x2, r.y1);
		pDC->Line(r.x1, r.y1+1, r.x1, r.y2);
		r.x1++;
		r.y1++;

		// fill the background
		pDC->Colour(cBack);
		pDC->Rectangle(&r);

		// draw the drop down arrow
		d->Arrow = r;
		d->Arrow.x1 = d->Arrow.x2 - 10;
		pDC->Colour(Enabled() ? LC_SHADOW : LC_LOW, 24);
		int Cx = r.x2 - 5;
		int Cy = r.y1 + (r.Y()/2) + 2;
		for (int i=0; i<4; i++)
		{
			pDC->Line(Cx-i, Cy, Cx+i, Cy);
			Cy--;
		}
		r.x2 = d->Arrow.x1 - 1;
		pDC->Colour(LC_LOW, 24);
		pDC->Line(r.x2, r.y1+1, r.x2, r.y2-1);
		r.x2--;

		// draw the text
		r.Size(1, 1);
		if (r.Valid())
		{
			GDisplayString *ds = d->GetText(_FL);
			if (ds)
			{
				if (Enabled())
				{
					bool f = Focus();
					SysFont->Colour(f ? LC_FOCUS_SEL_FORE : LC_TEXT, f ? LC_FOCUS_SEL_BACK : LC_MED);
					SysFont->Transparent(false);
					ds->Draw(pDC, r.x1, r.y1, &r);
				}
				else
				{
					SysFont->Transparent(false);
					SysFont->Colour(GColour(LC_LIGHT,24), cBack);
					ds->Draw(pDC, r.x1+1, r.y1+1, &r);

					SysFont->Transparent(true);
					SysFont->Colour(LC_LOW, 0);
					ds->Draw(pDC, r.x1, r.y1, &r);
				}
			}
			else
			{
				pDC->Colour(Focus() ? LC_FOCUS_SEL_BACK : LC_MED, 24);
				pDC->Rectangle(&r);
			}
		}
	}
	#endif
}

void GCombo::OnAttach()
{
}

GCombo::SelectedState GCombo::GetSelectedState()
{
	return d->SelState;
}

void GCombo::SetSelectedState(SelectedState s)
{
	d->SelState = s;
}

