#include <stdlib.h>
#include <stdio.h>

#include "Lgi.h"
#include "GVariant.h"
#include "GSkinEngine.h"
#include "GCombo.h"

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
public:
	int Current;
	bool SortItems;
	int Sub;
	GRect Arrow;
	List<char> Items;
	int LastKey;
	GAutoString Find;
	GSubMenu *Menu;
	GDisplayString *Text;
	GCombo::SelectedState SelState;

	#ifdef MAC
	ThemeButtonDrawInfo Cur;
	#endif

	GComboPrivate()
	{
		SelState = GCombo::SelectedDisable;
		SortItems = false;
		Sub = false;
		Current = 0;
		LastKey = 0;
		Menu = 0;
		Text = 0;

		#ifdef MAC
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
};

GCombo::GCombo(int id, int x, int y, int cx, int cy, const char *name) :
	ResObject(Res_ComboBox)
{
	d = new GComboPrivate;
	
	SetId(id);
	GRect e(x, y, x+cx, y+cy);
	SetPos(e);
	Name(name);
	SetTabStop(true);

	#if WIN32NATIVE
	SetDlgCode(DLGC_WANTARROWS);
	SetStyle(GetStyle() | WS_CHILD | WS_VISIBLE);
	#endif
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

int GCombo::GetItems()
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
			DeleteObj(d->Text);
			Invalidate();
		}
		else return false;
	}
	else
	{
		d->Current = 0;
		DeleteObj(d->Text);
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
		d->Current = i;
		DeleteObj(d->Text);
		Invalidate();

		SendNotify(d->Current);
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
	while (Delete(0));
	Name(0);
}

bool GCombo::Delete()
{
	return Delete(d->Current);
}

bool GCombo::Delete(int i)
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
	if (p AND d->Items.HasItem(p))
	{
		d->Items.Delete(p);
		DeleteArray(p);
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
		int Result = d->Menu->Float(this, p.x, p.y, true);
		if (Result)
		{
			GetWindow()->OnCommand(Result, 0, Handle());
		}
	}
	else
	{
		GSubMenu *RClick = new GSubMenu;
		if (RClick)
		{
			int Base = 1000;
			int i=0;

			if (d->Sub)
			{
				if (d->Sub == GV_INT32)
				{
					d->Items.Sort(IntCompare, 0);
				}
				else if (d->Sub == GV_DOUBLE)
				{
					d->Items.Sort(DblCompare, 0);
				}
				else
				{
					d->Items.Sort(StringCompare, 0);
				}
			}
			else if (d->SortItems)
			{
				d->Items.Sort(StringCompare, 0);
			}

			char *c = d->Items.First();
			if (c)
			{
				if (d->Sub)
				{
					int n = 0;
					double dbl = 0;
					char f = 0;
					GSubMenu *m = 0;
					for (; c; c = d->Items.Next(), i++)
					{
						if (d->Sub == GV_INT32)
						{
							int ci = atoi(c);
							if (!m OR n != ci)
							{
								char Name[16];
								sprintf(Name, "%i...", n = ci);
								m = RClick->AppendSub(Name);
							}
						}
						else if (d->Sub == GV_DOUBLE)
						{
							double cd = atof(c);
							if (!m OR dbl != cd)
							{
								char Name[16];
								sprintf(Name, "%f", dbl = cd);
								char *e = Name+strlen(Name)-1;
								while (e > Name AND e[0] == '0') *e-- = 0;
								strcat(Name, "...");
								m = RClick->AppendSub(Name);
							}
						}
						else
						{
							if (!m OR f != *c)
							{
								char Name[16];
								sprintf(Name, "%c...", f = *c);
								m = RClick->AppendSub(Name);
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
								RClick->AppendItem(c, Base+i, i != d->Current);
								break;
							case SelectedHide:
								if (i != d->Current)
									RClick->AppendItem(c, Base+i, true);
								break;
							case SelectedShow:
								RClick->AppendItem(c, Base+i, true);
								break;
						}
					}
				}
			}
			else
			{
				RClick->AppendItem("", Base+i, false);
			}

			int Result = RClick->Float(this, p.x, p.y, true);
			if (Result >= Base)
			{
				d->Current = Result - Base;
				DeleteObj(d->Text);
				Invalidate();

				SendNotify(d->Current);
			}
			
			DeleteObj(RClick);
		}
	}
}

void GCombo::OnMouseClick(GMouse &m)
{
	if (m.Down() AND
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
				int i = Value();
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
				if (k.IsChar AND k.c16 > ' ')
				{
					int Now = LgiCurrentTime();
					if (d->LastKey + 2000 < Now)
					{
						d->Find.Reset();
					}

					int Len = d->Find ? strlen(d->Find) : 0;
					GAutoString n(new char[Len+2]);
					if (n)
					{
						if (d->Find) strcpy(n, d->Find);
						n[Len++] = k.c16;
						n[Len] = 0;
						d->Find = n;
					}

					if (Len > 0 AND d->Find)
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
	DeleteObj(d->Text);
	char *n = Name();
	if (n)
	{
		d->Text = new GDisplayString(GetFont(), n);
	}
	Invalidate();
}

void GCombo::OnPaint(GSurface *pDC)
{
	if (!d->Text)
	{
		char *n = Name();
		if (n)
			d->Text = new GDisplayString(GetFont(), n);
	}

	#ifdef MAC

	pDC->Colour(LC_MED, 24);	
	pDC->Rectangle();

	GRect c = GetClient();
	Rect Bounds = { c.y1, c.x1+2, c.y2-1, c.x2-1 };
	ThemeButtonDrawInfo Info;
	Info.state = Enabled() ? kThemeStateActive : kThemeStateInactive;
	Info.value = Focus() ? kThemeButtonOn : kThemeButtonOff;
	Info.adornment = Focus() ? kThemeAdornmentFocus : kThemeAdornmentNone;
	
	GLabelData User;
	User.Ctrl = this;
	User.pDC = pDC;
	User.r.y1 = 1;
	User.Justification = teJustLeft;
	OSStatus e = DrawThemeButton(&Bounds,
								kThemePopupButton,
								&Info,
								&d->Cur,
								0,
								LgiLabelUPP ? LgiLabelUPP : LgiLabelUPP = NewThemeButtonDrawUPP(LgiLabelProc),
								(UInt32)&User);
	
	d->Cur = Info;
	
	#else
	
	if (GApp::SkinEngine AND
		TestFlag(GApp::SkinEngine->GetFeatures(), GSKIN_COMBO))
	{
		GSkinState State;
		State.pScreen = pDC;
		State.Text = &d->Text;
		GApp::SkinEngine->OnPaint_GCombo(this, &State);
	}
	else
	{
		GRect r(0, 0, X()-1, Y()-1);

		/*
		if (r.Y() != COMBO_HEIGHT)
		{
			GRect t = GetPos();
			t.y2 = t.y1 + COMBO_HEIGHT - 1;
			SetPos(t, true);
			r.ZOff(t.X()-1, t.Y()-1);
		}
		*/

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
		pDC->Colour(LC_MED, 24);
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
			if (d->Text)
			{
				if (Enabled())
				{
					bool f = Focus();
					SysFont->Colour(f ? LC_SEL_TEXT : LC_TEXT, f ? LC_SELECTION : LC_MED);
					SysFont->Transparent(false);
					d->Text->Draw(pDC, r.x1, r.y1, &r);
				}
				else
				{
					SysFont->Transparent(false);
					SysFont->Colour(LC_LIGHT, LC_MED);
					d->Text->Draw(pDC, r.x1+1, r.y1+1, &r);

					SysFont->Transparent(true);
					SysFont->Colour(LC_LOW, 0);
					d->Text->Draw(pDC, r.x1, r.y1, &r);
				}
			}
			else
			{
				pDC->Colour(Focus() ? LC_SELECTION : LC_MED, 24);
				pDC->Rectangle(&r);
			}
		}
	}
	#endif
}

void GCombo::OnAttach()
{
}

bool GCombo::SetPos(GRect &p, bool r)
{
	return GView::SetPos(p, r);
}

GCombo::SelectedState GCombo::GetSelectedState()
{
	return d->SelState;
}

void GCombo::SetSelectedState(SelectedState s)
{
	d->SelState = s;
}

