#include <stdio.h>
#include <stdlib.h>

#include "Lgi.h"
#include "GFontSelect.h"
#include "GToken.h"
#include "GCheckBox.h"
#include "GButton.h"
#include "LList.h"
#include "GRadioGroup.h"
#include "GEdit.h"
#include "GCombo.h"
#include "GBitmap.h"
#include "GTextLabel.h"
#include "GDisplayString.h"
#include "GdiLeak.h"
#include "GTableLayout.h"
#include "LgiRes.h"

#define IDD_FONT					1000
#define IDC_FONT					1001
#define IDC_UNDERLINE				1003
#define IDC_BOLD					1004
#define IDC_ITALIC					1005
#define IDC_PT_SIZE					1007
#define IDC_SELECT_SIZE				1008
#define IDC_PREVIEW					1009

int FontSizes[] = { 8, 9, 10, 11, 12, 14, 16, 18, 20, 24, 28, 32, 48, 72, 100, 0 };

struct GFontSelectPriv
{
	GTableLayout *Tbl;
	LList *Lst;
	GBitmap *Bmp;
	GCombo *PtSizes;
	bool Running;
	
	GFontSelectPriv()
	{
		Tbl = NULL;
		Lst = NULL;
		Bmp = NULL;
		PtSizes = NULL;
		Running = false;
	}
};

GFontSelect::GFontSelect(GView *Parent, void *Init, int InitLen)
{
	// Initialize
	d = new GFontSelectPriv;
	Size = 10;
	Bold = false;
	Underline = false;
	Italic = false;

	if (Init)
	{
		// Process initialization data
		Serialize(Init, InitLen, false);
	}

	SetParent(Parent);
	Name(LgiLoadString(L_FONTUI_TITLE, "Select Font"));
	GRect r(0, 0, 290 + LgiApp->GetMetric(LGI_MET_DECOR_X), 290 + LgiApp->GetMetric(LGI_MET_DECOR_Y));
	SetPos(r);
	MoveToCenter();

	// Setup dialog
	AddView(d->Tbl = new GTableLayout(50));

	GRadioGroup *rg;
	GLayoutCell *c = d->Tbl->GetCell(0, 0, true, 1, 2);
		c->Add(d->Lst = new LList(IDC_FONT, 14, 14, 161, 147));
		d->Lst->AddColumn(LgiLoadString(L_FONTUI_FACE, "Face"), 140);
		d->Lst->MultiSelect(false);
	
	c = d->Tbl->GetCell(1, 0, true, 1, 1);
		c->Add(rg = new GRadioGroup(IDM_STATIC, 0, 0, 100, 100, LgiLoadString(L_FONTUI_STYLE, "Style")));
		rg->AddView(new GCheckBox(IDC_BOLD, 0, 0, -1, -1, LgiLoadString(L_FONTUI_BOLD, "Bold")));
		rg->AddView(new GCheckBox(IDC_ITALIC, 0, 0, -1, -1, LgiLoadString(L_FONTUI_ITALIC, "Italic")));
		rg->AddView(new GCheckBox(IDC_UNDERLINE, 0, 0, -1, -1, LgiLoadString(L_FONTUI_UNDERLINE, "Underline")));
	
	c = d->Tbl->GetCell(1, 1, true, 1, 1);
		c->Add(rg = new GRadioGroup(IDM_STATIC, 0, 0, 100, 100, LgiLoadString(L_FONTUI_PTSIZE, "Pt Size")));
		rg->AddView(new GEdit(IDC_PT_SIZE, 0, 0, 56, 21, ""));
		rg->AddView(d->PtSizes = new GCombo(IDC_SELECT_SIZE, 0, 0, 20, 20, ""));
	
	c = d->Tbl->GetCell(0, 2, true, 2, 1);
		c->Add(new GText(IDM_STATIC, 0, 0, -1, -1, LgiLoadString(L_FONTUI_PREVIEW, "Preview:")));
	
	c = d->Tbl->GetCell(0, 3, true, 2, 1);
		c->Add(d->Bmp = new GBitmap(IDC_PREVIEW, 14, 182, 0));
	
	c = d->Tbl->GetCell(0, 4, true, 2, 1);
		c->TextAlign(GCss::Len(GCss::AlignRight));
		c->Add(new GButton(IDOK, 175, 259, 49, 21, LgiLoadString(L_BTN_OK, "Ok")));
		c->Add(new GButton(IDCANCEL, 231, 259, 49, 21, LgiLoadString(L_BTN_CANCEL, "Cancel")));

	// Populate the contents of the controls
	int n = 0;
	for (int *s=FontSizes; *s; s++, n++)
	{
		char Str[32];
		sprintf_s(Str, sizeof(Str), "%i", *s);
		d->PtSizes->Insert(Str);

		if (*s == Size)
		{
			d->PtSizes->Value(n);
		}
	}
	EnumerateFonts();
	SetCtrlValue(IDC_BOLD, Bold);
	SetCtrlValue(IDC_ITALIC, Italic);
	SetCtrlValue(IDC_UNDERLINE, Underline);

	d->Running = true;
	OnNotify(d->PtSizes, 0);
}

GFontSelect::~GFontSelect()
{
	DeleteObj(d);
}

char *GFontSelect::GetSelectedFace()
{
	LListItem *i = d->Lst->GetSelected();
	if (i)
	{
		return i->GetText(0);
	}
	return 0;
}

void GFontSelect::InsertFont(const char *f)
{
	if (f && f[0] != '@')
	{
		LListItem *i = new LListItem;
		if (i)
		{
			i->SetText(f, 0);
			d->Lst->Insert(i);

			if (Face && stricmp(f, Face) == 0)
			{
				i->Select(true);
			}
		}
	}
}

int SortFunc(LListItem *a, LListItem *b, NativeInt Data)
{
	char *A = a->GetText(0);
	char *B = b->GetText(0);
	if (A && B) return stricmp(A, B);
	return 0;
}

void GFontSelect::EnumerateFonts()
{
	List<const char> Fonts;
	if (GFontSystem::Inst()->EnumerateFonts(Fonts))
	{
		for (const char *n = Fonts.First(); n; n = Fonts.Next())
		{
			InsertFont(n);
			DeleteArray((char*&)n);
		}

		d->Lst->Sort(SortFunc, 0);
		d->Lst->ResizeColumnsToContent();
	}
}

void GFontSelect::UpdatePreview()
{
	if (!d->Running)
		return;
	UiToThis();
	
	GFont f;
	f.Bold(Bold);
	f.Underline(Underline);
	f.Italic(Italic);
	if (f.Create(Face, Size))
	{
		GMemDC *Dc = new GMemDC;
		if (Dc->Create(263, 65, GdcD->GetColourSpace()))
		{
			Dc->Colour(LC_WORKSPACE, 24);
			Dc->Rectangle();
			f.Colour(LC_BLACK, LC_WORKSPACE);
			f.Transparent(true);

			GDisplayString ds(&f, "AaBbCcDdEeFf");
			ds.Draw(Dc, 5, 5);

			if (Dc->GetBits() == 32)
			{
				// Hack the alpha channel back to "FF"
				// On win32 the font draws in "00000000" instead of "ff000000"
				switch (Dc->GetColourSpace())
				{
					#define AlphaHack(cs) \
						case Cs##cs: \
						{ \
							for (int y=0; y<Dc->Y(); y++) \
							{ \
								G##cs *p = (G##cs*) (*Dc)[y]; \
								G##cs *e = p + Dc->X(); \
								while (p < e) \
								{ \
									p->a = 255; \
									p++; \
								} \
							} \
							break; \
						}
					
					AlphaHack(Rgba32);
					AlphaHack(Argb32);
					AlphaHack(Abgr32);
					AlphaHack(Bgra32);
					default:
						LgiAssert(0);
						break;
				}
			}

			d->Bmp->SetDC(Dc);
		}
	}
}

void GFontSelect::OnCreate()
{
	LListItem *i = d->Lst->GetSelected();
	if (i)
	{
		i->ScrollTo();
	}
}

void GFontSelect::UiToThis()
{
	Face = GetSelectedFace();
	Size = (int)GetCtrlValue(IDC_PT_SIZE);
	Underline = GetCtrlValue(IDC_UNDERLINE);
	Bold = GetCtrlValue(IDC_BOLD);
	Italic = GetCtrlValue(IDC_ITALIC);
}

bool GFontSelect::Serialize(void *Data, int DataLen, bool Write)
{
	if (Write) // Dialog -> Data
	{
		#ifdef WINNATIVE

		if (DataLen == sizeof(LOGFONTA))
		{
			LOGFONTA *Fnt = (LOGFONTA*) Data;
			if (sizeof(*Fnt) > DataLen)
				return false;
			if (Face)
				sprintf_s(Fnt->lfFaceName, CountOf(Fnt->lfFaceName), "%s", Face.Get());
			Fnt->lfHeight = WinPointToHeight(Size);
			Fnt->lfWeight = Bold ? FW_BOLD : FW_NORMAL;
			Fnt->lfUnderline = Underline;
			Fnt->lfItalic = Italic;
		}
		else if (DataLen == sizeof(LOGFONTW))
		{
			LOGFONTW *Fnt = (LOGFONTW*) Data;
			if (sizeof(*Fnt) > DataLen)
				return false;
			if (Face)
				swprintf_s(Fnt->lfFaceName, CountOf(Fnt->lfFaceName), L"%S", Face.Get());
			Fnt->lfHeight = WinPointToHeight(Size);
			Fnt->lfWeight = Bold ? FW_BOLD : FW_NORMAL;
			Fnt->lfUnderline = Underline;
			Fnt->lfItalic = Italic;
		}
		else LgiAssert(!"Unknown object size.");

		#else
		
		char *Fnt = (char*)Data;
		sprintf_s(Fnt, DataLen,
				"%s,%i,%s%s%s",
				Face.Get(),
				Size,
				Bold?"b":"",
				Underline?"u":"",
				Italic?"b":"");

		#endif
	}
	else // Data -> Dialog
	{
		#ifdef WINNATIVE

		if (DataLen == sizeof(LOGFONTA))
		{
			LOGFONTA *Fnt = (LOGFONTA*) Data;
			Face = Fnt->lfFaceName;
			Size = WinHeightToPoint(Fnt->lfHeight);
			Bold = Fnt->lfWeight >= FW_BOLD;
			Underline = Fnt->lfUnderline;
			Italic = Fnt->lfItalic;
		}
		else if (DataLen == sizeof(LOGFONTW))
		{
			LOGFONTW *Fnt = (LOGFONTW*) Data;
			Face = Fnt->lfFaceName;
			Size = WinHeightToPoint(Fnt->lfHeight);
			Bold = Fnt->lfWeight >= FW_BOLD;
			Underline = Fnt->lfUnderline;
			Italic = Fnt->lfItalic;
		}

		#else

		GToken T((char*)Data, ",");
		if (T[0])
		{
			Face = T[0];
		}
		if (T[1])
		{
			Size = atoi(T[1]);
		}

		Bold = Underline = Italic = false;
		char *t = T[2];
		if (t)
		{
			if (strchr(t, 'b')) Bold = true;
			if (strchr(t, 'u')) Underline = true;
			if (strchr(t, 'i')) Italic = true;
		}

		#endif
	}

	return true;
}

int GFontSelect::OnNotify(GViewI *Ctrl, int Flags)
{
	switch (Ctrl->GetId())
	{
		case IDC_SELECT_SIZE:
		{
			char *n = Ctrl->Name();
			if (n)
				SetCtrlValue(IDC_PT_SIZE, atoi(n));
			else
				break;
			// fall thru
		}
		case IDC_BOLD:
		case IDC_ITALIC:
		case IDC_UNDERLINE:
		{
			UpdatePreview();
			break;
		}
		case IDC_FONT:
		{
			GNotifyType n = (GNotifyType)Flags;
			if (n == GNotifyItem_Select)
			{
				UpdatePreview();
			}
			break;
		}
		case IDOK:
		{
			UiToThis();
			// fall thru
		}
		case IDCANCEL:
		{
			EndModal(Ctrl->GetId());
			break;
		}
	}

	return 0;
}
