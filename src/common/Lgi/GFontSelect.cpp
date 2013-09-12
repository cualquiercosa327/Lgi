#include <stdio.h>
#include <stdlib.h>

#include "Lgi.h"
#include "GFontSelect.h"
#include "GToken.h"
#include "GCheckBox.h"
#include "GButton.h"
#include "GList.h"
#include "GRadioGroup.h"
#include "GEdit.h"
#include "GCombo.h"
#include "GBitmap.h"
#include "GTextLabel.h"
#include "GDisplayString.h"
#include "GdiLeak.h"

#define IDD_FONT					1000
#define IDC_FONT					1001
#define IDC_UNDERLINE				1003
#define IDC_BOLD					1004
#define IDC_ITALIC					1005
#define IDC_PT_SIZE					1007
#define IDC_SELECT_SIZE				1008
#define IDC_PREVIEW					1009

int FontSizes[] = { 8, 9, 10, 11, 12, 14, 16, 18, 20, 24, 28, 32, 48, 72, 100, 0 };

GFontSelect::GFontSelect(GView *Parent, void *Init)
{
	// Initialize
	Face = 0;
	Size = 0;
	Bold = false;
	Underline = false;
	Italic = false;

	if (Init)
	{
		// Process initialization data
		Serialize(Init, false);
	}

	SetParent(Parent);
	Name(LgiLoadString(L_FONTUI_TITLE, "Select Font"));
	GRect r(0, 0, 290 + LgiApp->GetMetric(LGI_MET_DECOR_X), 290 + LgiApp->GetMetric(LGI_MET_DECOR_Y));
	SetPos(r);
	MoveToCenter();

	// Setup dialog
	int y = 20;
	Children.Insert(Ctrl1 = new GList(IDC_FONT, 14, 14, 161, 147));
		Ctrl1->AddColumn(LgiLoadString(L_FONTUI_FACE, "Face"), 140);
		Ctrl1->MultiSelect(false);
	Children.Insert(Ctrl2 = new GRadioGroup(IDM_STATIC, 182, 7, 98, 91, LgiLoadString(L_FONTUI_STYLE, "Style")));
		Ctrl2->AddView(Ctrl4 = new GCheckBox(IDC_BOLD,		11, y, -1, -1, LgiLoadString(L_FONTUI_BOLD, "Bold")));
		Ctrl2->AddView(Ctrl5 = new GCheckBox(IDC_ITALIC,	11, y+20, -1, -1, LgiLoadString(L_FONTUI_ITALIC, "Italic")));
		Ctrl2->AddView(Ctrl3 = new GCheckBox(IDC_UNDERLINE,	11, y+40, -1, -1, LgiLoadString(L_FONTUI_UNDERLINE, "Underline")));
	Children.Insert(Ctrl6 = new GRadioGroup(IDM_STATIC, 182, 105, 98, 56, LgiLoadString(L_FONTUI_PTSIZE, "Pt Size")));
		Ctrl6->AddView(Ctrl7 = new GEdit(IDC_PT_SIZE,		11, y, 56, 21, ""));
		Ctrl6->AddView(Ctrl8 = new GCombo(IDC_SELECT_SIZE,	70, y, 20, 21, ""));
	Children.Insert(Ctrl9 = new GBitmap(IDC_PREVIEW, 14, 182, 0));
	Children.Insert(Ctrl10 = new GText(IDM_STATIC, 14, 166, -1, -1, LgiLoadString(L_FONTUI_PREVIEW, "Preview:")));
	Children.Insert(Ctrl11 = new GButton(IDOK, 175, 259, 49, 21, LgiLoadString(L_BTN_OK, "Ok")));
	Children.Insert(Ctrl12 = new GButton(IDCANCEL, 231, 259, 49, 21, LgiLoadString(L_BTN_CANCEL, "Cancel")));

	// Populate the contents of the controls
	int n = 0;
	for (int *s=FontSizes; *s; s++, n++)
	{
		char Str[32];
		sprintf(Str, "%i", *s);
		Ctrl8->Insert(Str);

		if (*s == Size)
		{
			Ctrl8->Value(n);
		}
	}
	EnumerateFonts();
	Ctrl4->Value(Bold);
	Ctrl5->Value(Italic);
	Ctrl3->Value(Underline);

	OnNotify(Ctrl8, 0);
}

GFontSelect::~GFontSelect()
{
	DeleteArray(Face);
}

char *GFontSelect::GetSelectedFace()
{
	GListItem *i = Ctrl1->GetSelected();
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
		GListItem *i = new GListItem;
		if (i)
		{
			i->SetText(f, 0);
			Ctrl1->Insert(i);

			if (stricmp(f, Face) == 0)
			{
				i->Select(true);
			}
		}
	}
}

int SortFunc(GListItem *a, GListItem *b, NativeInt Data)
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

		Ctrl1->Sort(SortFunc, 0);
	}
}

void GFontSelect::UpdatePreview()
{
	UiToThis();
	
	GFont f;
	f.Bold(Bold);
	f.Underline(Underline);
	f.Italic(Italic);
	if (f.Create(Face, Size))
	{
		GMemDC *Dc = new GMemDC;
		if (Dc->Create(263, 65, GdcD->GetBits()))
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

			Ctrl9->SetDC(Dc);
		}
	}
}

void GFontSelect::OnCreate()
{
	GListItem *i = Ctrl1->GetSelected();
	if (i)
	{
		i->ScrollTo();
	}
}

void GFontSelect::UiToThis()
{
	DeleteArray(Face);
	Face = NewStr(GetSelectedFace());
	Size = Ctrl7->Name() ? atoi(Ctrl7->Name()) : 0;
	Underline = Ctrl3->Value();
	Bold = Ctrl4->Value();
	Italic = Ctrl5->Value();
}

bool GFontSelect::Serialize(void *Data, bool Write)
{
	if (Write) // Dialog -> Data
	{
		#ifdef WIN32

		LOGFONT *Fnt = (LOGFONT*) Data;
		if (Face)
		{
			strcpy(Fnt->lfFaceName, Face);
		}
		Fnt->lfHeight = WinPointToHeight(Size);
		Fnt->lfWeight = Bold ? FW_BOLD : FW_NORMAL;
		Fnt->lfUnderline = Underline;
		Fnt->lfItalic = Italic;

		#else
		
		char *Fnt = (char*)Data;
		sprintf(Fnt,
				"%s,%i,%s%s%s",
				Face,
				Size,
				Bold?"b":"",
				Underline?"u":"",
				Italic?"b":"");

		#endif
	}
	else // Data -> Dialog
	{
		#ifdef WIN32

		LOGFONT *Fnt = (LOGFONT*) Data;
		Face = NewStr(Fnt->lfFaceName);
		Size = WinHeightToPoint(Fnt->lfHeight);
		Bold = Fnt->lfWeight >= FW_BOLD;
		Underline = Fnt->lfUnderline;
		Italic = Fnt->lfItalic;

		#else

		GToken T((char*)Data, ",");
		if (T[0])
		{
			DeleteArray(Face);
			Face = NewStr(T[0]);
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
			SetCtrlValue(IDC_PT_SIZE, atoi(Ctrl8->Name()));
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
			if (Flags == GLIST_NOTIFY_SELECT)
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
