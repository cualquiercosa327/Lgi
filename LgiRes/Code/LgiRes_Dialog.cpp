/*
**	FILE:			LgiRes_Dialog.cpp
**	AUTHOR:			Matthew Allen
**	DATE:			5/8/1999
**	DESCRIPTION:	Dialog Resource Editor
**
**
**	Copyright (C) 1999, Matthew Allen
**		fret@memecode.com
*/

// Old control offset 3,17

////////////////////////////////////////////////////////////////////
#include <stdlib.h>
#include "LgiResEdit.h"
#include "LgiRes_Dialog.h"
#include "GButton.h"
#include "GVariant.h"
#include "GToken.h"
#include "GDisplayString.h"
#include "GClipBoard.h"
#include "resdefs.h"

////////////////////////////////////////////////////////////////////
#define IDC_UP						101
#define IDC_DOWN					102

// Name mapping table
class LgiObjectName
{
public:
	int Type;
	const char *ObjectName;
	char *ResourceName;
	bool ToolbarBtn;
}
NameMap[] =
{
	// ID				Lgi's name		Resource editor name
	{UI_DIALOG,			"GDialog",		Res_Dialog,			false},
	{UI_TABLE,			"GTableLayout",	Res_Table,			true},
	{UI_TEXT,			"GText",		Res_StaticText,		true},
	{UI_EDITBOX,		"GEdit",		Res_EditBox,		true},
	{UI_CHECKBOX,		"GCheckBox",	Res_CheckBox,		true},
	{UI_BUTTON,			"GButton",		Res_Button,			true},
	{UI_GROUP,			"GRadioGroup",	Res_Group,			true},
	{UI_RADIO,			"GRadioButton",	Res_RadioBox,		true},
	{UI_TABS,			"GTabView",		Res_TabView,		true},
	{UI_TAB,			"GTabPage",		Res_Tab,			false},
	{UI_LIST,			"GList",		Res_ListView,		true},
	{UI_COLUMN,			"GListColumn",	Res_Column,			false},
	{UI_COMBO,			"GCombo",		Res_ComboBox,		true},
	{UI_TREE,			"GTree",		Res_TreeView,		true},
	{UI_BITMAP,			"GBitmap",		Res_Bitmap,			true},
	{UI_PROGRESS,		"GProgress",	Res_Progress,		true},
	{UI_SCROLL_BAR,		"GScrollBar",	Res_ScrollBar,		true},
	{UI_CUSTOM,			"GCustom",		Res_Custom,			true},
	{UI_CONTROL_TREE,	"GControlTree", Res_ControlTree,	true},

	// If you add a new control here update ResDialog::CreateCtrl(int Tool) as well

	{0, 0, 0, 0}
};

class CtrlItem : public GListItem
{
	friend class TabOrder;

	ResDialogCtrl *Ctrl;

public:
	CtrlItem(ResDialogCtrl *ctrl)
	{
		Ctrl = ctrl;
	}

	char *GetText(int Col)
	{
		switch (Col)
		{
			case 0:
			{
				if (Ctrl &&
					Ctrl->Str &&
					Ctrl->Str->GetDefine())
				{
					return Ctrl->Str->GetDefine();
				}
				break;
			}
			case 1:
			{
				if (Ctrl &&
					Ctrl->Str &&
					Ctrl->Str->Get())
				{
					return Ctrl->Str->Get();
				}
				break;
			}
		}
		return (char*)"";
	}
};

class TabOrder : public GDialog
{
	ResDialogCtrl *Top;
	GList *Lst;
	GButton *Ok;
	GButton *Cancel;
	GButton *Up;
	GButton *Down;

public:
	TabOrder(GView *Parent, ResDialogCtrl *top)
	{
		Top = top;
		SetParent(Parent);

		Children.Insert(Lst = new GList(IDC_LIST, 10, 10, 350, 300));
		Children.Insert(Ok = new GButton(IDOK, Lst->GetPos().x2 + 10, 10, 60, 20, "Ok"));
		Children.Insert(Cancel = new GButton(IDCANCEL, Lst->GetPos().x2 + 10, Ok->GetPos().y2 + 5, 60, 20, "Cancel"));
		Children.Insert(Up = new GButton(IDC_UP, Lst->GetPos().x2 + 10, Cancel->GetPos().y2 + 15, 60, 20, "Up"));
		Children.Insert(Down = new GButton(IDC_DOWN, Lst->GetPos().x2 + 10, Up->GetPos().y2 + 5, 60, 20, "Down"));

		GRect r(0, 0, Ok->GetPos().x2 + 17, Lst->GetPos().y2 + 40);
		SetPos(r);
		MoveToCenter();

		Lst->AddColumn("#define", 150);
		Lst->AddColumn("Text", 150);
		Lst->MultiSelect(false);

		if (Top)
		{
			GAutoPtr<GViewIterator> It(Top->View()->IterateViews());
			for (ResDialogCtrl *Ctrl=dynamic_cast<ResDialogCtrl*>(It->First());
				Ctrl; Ctrl=dynamic_cast<ResDialogCtrl*>(It->Next()))
			{
				if (Ctrl->GetType() != UI_TEXT)
				{
					Lst->Insert(new CtrlItem(Ctrl));
				}
			}

			char s[256];
			sprintf(s, "Set Tab Order: %s", Top->Str->GetDefine());
			Name(s);

			DoModal();
		}
	}

	int OnNotify(GViewI *Ctrl, int Flags)
	{
		int MoveDir = 1;
		switch (Ctrl->GetId())
		{
			case IDC_UP:
			{
				MoveDir = -1;
				// fall through
			}
			case IDC_DOWN:
			{
				if (Lst)
				{
					CtrlItem *Sel = dynamic_cast<CtrlItem*>(Lst->GetSelected());
					if (Sel)
					{
						int Index = Lst->IndexOf(Sel);
						CtrlItem *Up = dynamic_cast<CtrlItem*>(Lst->ItemAt(Index+MoveDir));
						if (Up)
						{
							Lst->Remove(Sel);
							Lst->Insert(Sel, Index+MoveDir);
							Sel->Select(true);
							Sel->ScrollTo();
						}
					}
				}
				break;
			}
			case IDOK:
			{
				if (Lst)
				{
					int i=0;

					List<CtrlItem> All;
					Lst->GetAll(All);
					for (CtrlItem *n=All.First(); n; n=All.Next())
					{
						Top->View()->DelView(n->Ctrl->View());
						Top->View()->AddView(n->Ctrl->View(), i++);
					}
				}
				// fall through
			}
			case IDCANCEL:
			{
				EndModal(0);
				break;
			}
		}

		return 0;
	}
};

////////////////////////////////////////////////////////////////////
void DrawGoobers(GSurface *pDC, GRect &r, GRect *Goobers, COLOUR c)
{
	int Mx = (r.x2 + r.x1) / 2 - (GOOBER_SIZE / 2);
	int My = (r.y2 + r.y1) / 2 - (GOOBER_SIZE / 2);

	pDC->Colour(c, 24);

	Goobers[0].x1 = r.x1 - GOOBER_BORDER;
	Goobers[0].y1 = r.y1 - GOOBER_BORDER;
	Goobers[0].x2 = r.x1 - (GOOBER_BORDER - GOOBER_SIZE);
	Goobers[0].y2 = r.y1 - (GOOBER_BORDER - GOOBER_SIZE);

	Goobers[1].x1 = Mx;
	Goobers[1].y1 = r.y1 - GOOBER_BORDER;
	Goobers[1].x2 = Mx + GOOBER_SIZE;
	Goobers[1].y2 = r.y1 - (GOOBER_BORDER - GOOBER_SIZE);

	Goobers[2].x1 = r.x2 + (GOOBER_BORDER - GOOBER_SIZE);
	Goobers[2].y1 = r.y1 - GOOBER_BORDER;
	Goobers[2].x2 = r.x2 + GOOBER_BORDER;
	Goobers[2].y2 = r.y1 - (GOOBER_BORDER - GOOBER_SIZE);

	Goobers[3].x1 = r.x2 + (GOOBER_BORDER - GOOBER_SIZE);
	Goobers[3].y1 = My;
	Goobers[3].x2 = r.x2 + GOOBER_BORDER;
	Goobers[3].y2 = My + GOOBER_SIZE;

	Goobers[4].x1 = r.x2 + (GOOBER_BORDER - GOOBER_SIZE);
	Goobers[4].y1 = r.y2 + (GOOBER_BORDER - GOOBER_SIZE);
	Goobers[4].x2 = r.x2 + GOOBER_BORDER;
	Goobers[4].y2 = r.y2 + GOOBER_BORDER;

	Goobers[5].x1 = Mx;
	Goobers[5].y1 = r.y2 + (GOOBER_BORDER - GOOBER_SIZE);
	Goobers[5].x2 = Mx + GOOBER_SIZE;
	Goobers[5].y2 = r.y2 + GOOBER_BORDER;

	Goobers[6].x1 = r.x1 - GOOBER_BORDER;
	Goobers[6].y1 = r.y2 + (GOOBER_BORDER - GOOBER_SIZE);
	Goobers[6].x2 = r.x1 - (GOOBER_BORDER - GOOBER_SIZE);
	Goobers[6].y2 = r.y2 + GOOBER_BORDER;

	Goobers[7].x1 = r.x1 - GOOBER_BORDER;
	Goobers[7].y1 = My;
	Goobers[7].x2 = r.x1 - (GOOBER_BORDER - GOOBER_SIZE);
	Goobers[7].y2 = My + GOOBER_SIZE;

	for (int i=0; i<8; i++)
	{
		pDC->Box(Goobers+i);
	}
}

////////////////////////////////////////////////////////////////////
int ResDialogCtrl::TabDepth = 0;

ResDialogCtrl::ResDialogCtrl(ResDialog *dlg, char *CtrlTypeName, GXmlTag *load) :
	ResObject(CtrlTypeName)
{
	Dlg = dlg;
	DragCtrl = -1;
	AcceptChildren = false;
	Movable = true;
	MoveCtrl = false;
	Vis = true;
	Client.ZOff(-1, -1);
	SelectMode = SelNone;
	SelectStart.ZOff(-1, -1);

	if (load)
	{
		// Base a string off the xml
		int r = load->GetAsInt("ref");
		if (Dlg)
		{
			Str = Dlg->Symbols->FindRef(r);
			LgiAssert(Str);

			if (!Str) // oh well we should have one anyway... fix things up so to speak.
				Dlg->Symbols->CreateStr();
		}

		LgiAssert(Str);
	}
	else if (Dlg->CreateSymbols)
	{
		// We create a symbol for this resource
		Str = (Dlg && Dlg->Symbols) ? Dlg->Symbols->CreateStr() : 0;
		if (Str)
		{
			char Def[256];
			sprintf(Def, "IDC_%i", Str->GetRef());
			Str->SetDefine(Def);
		}
	}
	else
	{
		// Someone else is going to create the symbol
		Str = 0;
	}

	LgiAssert(Str);
}

ResDialogCtrl::~ResDialogCtrl()
{
	if (ResDialog::Symbols)
	{
		ResDialog::Symbols->DeleteStr(Str);
	}

	if (Dlg)
	{
		Dlg->OnDeselect(this);
	}
}

char *ResDialogCtrl::GetRefText()
{
	static char Buf[64];
	if (Str)
	{
		sprintf(Buf, "Ref=%i", Str->GetRef());
	}
	else
	{
		Buf[0] = 0;
	}
	return Buf;
}

void ResDialogCtrl::ListChildren(List<ResDialogCtrl> &l, bool Deep)
{
	GAutoPtr<GViewIterator> it(View()->IterateViews());
	for (GViewI *w = it->First(); w; w = it->Next())
	{
		ResDialogCtrl *c = dynamic_cast<ResDialogCtrl*>(w);
		LgiAssert(c);
		if (c)
		{
			l.Insert(c);
			if (Deep)
			{
				c->ListChildren(l);
			}
		}
	}
}

GRect ResDialogCtrl::GetMinSize()
{
	GRect m(0, 0, GRID_X-1, GRID_Y-1);

	if (IsContainer())
	{
		GRect cli = View()->GetClient(false);

		GAutoPtr<GViewIterator> it(View()->IterateViews());
		for (GViewI *c=it->First(); c; c=it->Next())
		{
			GRect cpos = c->GetPos();
			cpos.Offset(cli.x1, cli.y1);
			m.Union(&cpos);
		}
	}

	return m;
}

bool ResDialogCtrl::SetPos(GRect &p, bool Repaint)
{
	GRect m = GetMinSize();
	if (m.X() > p.X()) p.x2 = p.x1 + m.X() - 1;
	if (m.Y() > p.Y()) p.y2 = p.y1 + m.Y() - 1;

	if (p != View()->GetPos())
	{
		/*
		if (ParentCtrl)
		{
			if (p.x1 < ParentCtrl->Client.x1)
			{
				p.Offset(ParentCtrl->Client.x1 - p.x1, 0);
			}

			if (p.y1 < ParentCtrl->Client.y1)
			{
				p.Offset(0, ParentCtrl->Client.y1 - p.y1);
			}
		}
		*/

		// set our size
		bool Status = View()->SetPos(p, Repaint);
		
		// tell everyone else about the change
		OnFieldChange();

		GRect r(0, 0, p.X()-1, p.Y()-1);
		r.Size(-GOOBER_BORDER, -GOOBER_BORDER);
		View()->Invalidate(&r, false, true);
		
		// check our parents are big enough to show us...
		ResDialogCtrl *Par = ParentCtrl();
		if (Par)
		{
			GRect t = Par->View()->GetPos();
			Par->ResDialogCtrl::SetPos(t, true);
		}
		
		return Status;
	}

	return true;
}

void ResDialogCtrl::TabString(char *Str)
{
	if (Str)
	{
		for (int i=0; i<TabDepth; i++)
		{
			*Str++ = 9;
		}
		*Str = 0;
	}
}

GRect ResDialogCtrl::AbsPos()
{
	GViewI *w = View();
	GRect r = w->GetPos();
	r.Offset(-r.x1, -r.y1);

	for (; w && w != Dlg; w = w->GetParent())
	{
		GRect pos = w->GetPos();
		if (w->GetParent())
		{
			// GView *Ctrl = w->GetParent()->GetGView();

			GRect client = w->GetParent()->GetClient(false);
			r.Offset(pos.x1 + client.x1, pos.y1 + client.y1);
		}
		else
		{
			r.Offset(pos.x1, pos.y1);
		}
	}

	return r;
}

void ResDialogCtrl::StrFromRef(int Ref)
{
	// get the string object
	Str = Dlg->App()->GetStrFromRef(Ref);
	
	if (!Str)
	{
		LgiTrace("%s:%i - String with ref '%i' missing.\n", _FL, Ref);
		LgiAssert(0);
		if ((Str = Dlg->App()->GetDialogSymbols()->CreateStr()))
		{
			Str->SetRef(Ref);
		}
	}
	
	// if this assert fails then the Ref doesn't exist
	// and the string can't be found

	// if this assert fails then the Str is already
	// associated with a control, and thus we would
	// duplicate the pointer to the string if we let
	// it go by
	// LgiAssert(Str->UpdateWnd == 0);

	// set the string's control to us
	Str->UpdateWnd = View();

	// make the strings refid unique
	Str->UnDupelicate();
	
	View()->Name(Str->Get());
}

bool ResDialogCtrl::GetFields(FieldTree &Fields)
{
	if (Str)
	{
		Str->GetFields(Fields);
	}

	int Id = 101;
	Fields.Insert(this, DATA_STR, Id++, VAL_Pos, "Pos");
	Fields.Insert(this, DATA_BOOL, Id++, VAL_Visible, "Visible");
	Fields.Insert(this, DATA_BOOL, Id++, VAL_Enabled, "Enabled");
	Fields.Insert(this, DATA_STR, Id++, VAL_Class, "Class", -1);
	Fields.Insert(this, DATA_STR, Id++, VAL_Style, "Style", -1, true);

	return true;
}

bool ResDialogCtrl::Serialize(FieldTree &Fields)
{
	if ((Fields.GetMode() == FieldTree::ObjToUi ||
		Fields.GetMode() == FieldTree::UiToObj) &&
		Str)
	{
		Str->Serialize(Fields);
	}

	GRect r = View()->GetPos(), Old = View()->GetPos();
	bool e = true;

	if (Fields.GetMode() == FieldTree::ObjToUi ||
		Fields.GetMode() == FieldTree::ObjToStore)
	{
		r = View()->GetPos();
		e = View()->Enabled();
	}

	Fields.Serialize(this, VAL_Pos, r);
	Fields.Serialize(this, VAL_Visible, Vis, true);
	Fields.Serialize(this, VAL_Enabled, e, true);
	Fields.Serialize(this, VAL_Class, CssClass);
	Fields.Serialize(this, VAL_Style, CssStyle);

	if (Fields.GetMode() == FieldTree::UiToObj ||
		Fields.GetMode() == FieldTree::StoreToObj)
	{
		View()->Enabled(e);
		SetPos(r);
		r.Union(&Old);
		r.Size(-GOOBER_BORDER, -GOOBER_BORDER);
		if (View()->GetParent())
		{
			View()->GetParent()->Invalidate(&r);
		}
	}

	return true;
}

void ResDialogCtrl::CopyText()
{
	if (Str)
	{
		Str->CopyText();
	}
}

void ResDialogCtrl::PasteText()
{
	if (Str)
	{
		Str->PasteText();
		View()->Invalidate();
	}
}

bool ResDialogCtrl::AttachCtrl(ResDialogCtrl *Ctrl, GRect *r)
{
	bool Status = false;
	if (Ctrl)
	{
		Ctrl->View()->Visible(true);
		View()->AddView(Ctrl->View());
		Ctrl->View()->SetParent(View());
		if (r)
		{
			if (!dynamic_cast<CtrlTable*>(Ctrl->View()->GetParent()))
			{
				Dlg->SnapRect(r, this);
			}
			Ctrl->SetPos(*r);
		}

		if (Dlg->Resource::IsSelected())
		{
			Dlg->OnSelect(Ctrl);
		}

		Status = true;
	}
	return Status;
}

void ResDialogCtrl::OnPaint(GSurface *pDC)
{
	if (DragCtrl >= 0)
	{
		GRect r = DragRgn;
		r.Normal();
		pDC->Colour(LC_FOCUS_SEL_BACK, 24);
		pDC->Box(&r);
	}
}

void ResDialogCtrl::OnMouseClick(GMouse &m)
{
	if (m.Down())
	{
		if (m.Left())
		{
			if (Dlg)
			{
				bool Processed = false;
				GRect c = View()->GetClient();
				bool ClickedThis = c.Overlap(m.x, m.y);

				GRect Cli = View()->GetClient(false);
				GMouse Ms = m;
				GdcPt2 Off;
				View()->WindowVirtualOffset(&Off);
				Ms.x += Off.x + Cli.x1;
				Ms.y += Off.y + Cli.y1;

				Dlg->OnMouseClick(Ms);

				if (ClickedThis &&
					!Dlg->IsDraging())
				{
					DragCtrl = Dlg->CurrentTool();
					if ((DragCtrl > 0 && AcceptChildren) ||
						((DragCtrl == 0) && !Movable))
					{
						GdcPt2 p(m.x, m.y);
						Dlg->SnapPoint(&p, ParentCtrl());

						DragStart.x = DragRgn.x1 = DragRgn.x2 = p.x;
						DragStart.y = DragRgn.y1 = DragRgn.y2 = p.y;
						View()->Capture(true);
						Processed = true;
					}
					else
					{
						DragCtrl = -1;
						if (Movable)
						{
							DragRgn.x1 = m.x;
							DragRgn.y1 = m.y;
							MoveCtrl = true;
							Processed = true;
							View()->Capture(true);
						}
					}

					SelectMode = (m.Shift()) ? SelAdd : SelSet;
					SelectStart = View()->GetPos();
				}
			}
		}
		else if (m.IsContextMenu())
		{
			GSubMenu RClick;
			bool PasteData = false;
			bool PasteTranslations = false;

			{
				GClipBoard c(Dlg);
				char *Clip = c.Text();
				if (Clip)
				{
					PasteTranslations = strstr(Clip, TranslationStrMagic);

					char *p = Clip;
					while (*p && strchr(" \r\n\t", *p)) p++;
					PasteData = *p == '<';
					DeleteArray(Clip);
				}
			}

			RClick.AppendItem("Cu&t", IDM_CUT, Dlg->Selection.Length()>0);
			RClick.AppendItem("&Copy Control", IDM_COPY, Dlg->Selection.Length()>0);
			RClick.AppendItem("&Paste", IDM_PASTE, PasteData);
			RClick.AppendSeparator();

			RClick.AppendItem("Copy Text", IDM_COPY_TEXT, Dlg->Selection.Length()==1);
			RClick.AppendItem("Paste Text", IDM_PASTE_TEXT, PasteTranslations);
			RClick.AppendSeparator();

			RClick.AppendItem("&Delete", IDM_DELETE, Dlg->Selection.Length()>0);

			if (Dlg->GetMouse(m, true))
			{
				int Cmd = 0;
				switch (Cmd = RClick.Float(Dlg, m.x, m.y))
				{
					case IDM_DELETE:
					{
						Dlg->Delete();
						break;
					}
					case IDM_CUT:
					{
						Dlg->Copy(true);
						break;
					}
					case IDM_COPY:
					{
						Dlg->Copy();
						break;
					}
					case IDM_PASTE:
					{
						Dlg->Paste();
						break;
					}
					case IDM_COPY_TEXT:
					{
						ResDialogCtrl *Ctrl = Dlg->Selection.First();
						if (Ctrl)
						{
							Ctrl->CopyText();
						}
						break;
					}
					case IDM_PASTE_TEXT:
					{
						PasteText();
						break;
					}
				}
			}
		}
	}
	else
	{
		if (DragCtrl >= 0)
		{
			bool Exit = false;

			if (Dlg &&
				(DragRgn.X() > 1 ||
				DragRgn.Y() > 1))
			{
				if (DragCtrl == 0)
				{
					Dlg->SelectRect(this, &DragRgn, SelectMode != SelAdd);
				}
				else
				{
					ResDialogCtrl *Ctrl = Dlg->CreateCtrl(DragCtrl, 0);
					if (Ctrl)
					{
						AttachCtrl(Ctrl, &DragRgn);
					}
				}

				Exit = true;
			}

			DragCtrl = -1;
			View()->Invalidate();
			View()->Capture(false);

			if (Exit)
			{
				return;
			}
		}
		
		if (MoveCtrl)
		{
			View()->Capture(false);
			MoveCtrl = false;
		}
		
		if (SelectMode > SelNone)
		{
			GRect r = View()->GetPos();
			if (SelectStart == r)
			{
				Dlg->OnSelect(this, SelectMode != SelAdd);
			}
			SelectMode = SelNone;
		}
	}
}

void ResDialogCtrl::OnMouseMove(GMouse &m)
{
	// Drag a rubber band...
	if (DragCtrl >= 0)
	{
		GRect Old = DragRgn;

		DragRgn.x1 = DragStart.x;
		DragRgn.y1 = DragStart.y;
		DragRgn.x2 = m.x;
		DragRgn.y2 = m.y;
		DragRgn.Normal();

		Dlg->SnapRect(&DragRgn, this);

		Old.Union(&DragRgn);
		Old.Size(-1, -1);

		View()->Invalidate(&Old);
	}
	
	// Move some ctrl(s)
	if (MoveCtrl && !m.Shift())
	{
		int Dx = m.x - DragRgn.x1;
		int Dy = m.y - DragRgn.y1;
		if (Dx != 0 || Dy != 0)
		{
			if (!Dlg->IsSelected(this))
			{
				Dlg->OnSelect(this);
			}

			GRect Old = View()->GetPos();
			GRect New = Old;
			New.Offset(	m.x - DragRgn.x1,
						m.y - DragRgn.y1);

			GdcPt2 p(New.x1, New.y1);
			Dlg->SnapPoint(&p, ParentCtrl());
			New.Set(p.x, p.y, p.x + New.X() - 1, p.y + New.Y() - 1);

			if (New != Old)
			{
				Dlg->MoveSelection(New.x1 - Old.x1, New.y1 - Old.y1);
			}
		}
	}
}

char *ReadInt(char *s, int &Value)
{
	char *c = strchr(s, ',');
	if (c)
	{
		*c = 0;
		Value = atoi(s);
		return c+1;
	}
	Value = atoi(s);
	return 0;
}

void ResDialogCtrl::ReadPos(char *Str)
{
	if (Str)
	{
		char *s = NewStr(Str);
		if (s)
		{
			GRect r = View()->GetPos();

			char *p = ReadInt(s, r.x1);
			if (p) p = ReadInt(p, r.y1);
			if (p) p = ReadInt(p, r.x2);
			if (p) p = ReadInt(p, r.y2);

			DeleteArray(s);
		}
	}
}

////////////////////////////////////////////////////////////////////
CtrlDlg::CtrlDlg(ResDialog *dlg, GXmlTag *load) :
	ResDialogCtrl(dlg, Res_Dialog, load)
{
	Movable = false;
	AcceptChildren = true;
	Str->UpdateWnd = View();
	View()->Name("CtrlDlg");
}

IMPL_DIALOG_CTRL(CtrlDlg)

void CtrlDlg::OnPaint(GSurface *pDC)
{
	// Draw any rubber band
	ResDialogCtrl::OnPaint(pDC);
}

GViewI *CtrlDlg::WindowFromPoint(int x, int y)
{
	return GView::WindowFromPoint(x, y);
}

GRect &CtrlDlg::GetClient(bool InClientSpace)
{
	static GRect r;
	
	Client.Set(0, 0, View()->X()-1, View()->Y()-1);
	Client.Size(3, 3);
	Client.y1 += LgiApp->GetMetric(LGI_MET_DECOR_CAPTION);
	if (Client.y1 > Client.y2) Client.y1 = Client.y2;

	r = Client;
	if (InClientSpace)
		r.Offset(-r.x1, -r.y1);

	return r;
}

void CtrlDlg::_Paint(GSurface *pDC, int Ox, int Oy)
{
	Client = GetClient(false);

	// Draw the border
	GRect r(0, 0, View()->X()-1, View()->Y()-1);
	LgiWideBorder(pDC, r, RAISED);
	pDC->Colour(LC_MED, 24);
	LgiFlatBorder(pDC, r, 1);

	// Draw the title bar
	Title = r;
	Title.y2 = Client.y1 - 1;
	pDC->Colour(LC_ACTIVE_TITLE, 24);
	pDC->Rectangle(&Title);
	
	if (Str)
	{
		GDisplayString ds(SysFont, Str->Get());
		SysFont->Fore(LC_ACTIVE_TITLE_TEXT);
		SysFont->Transparent(true);
		ds.Draw(pDC, Title.x1 + 10, Title.y1 + ((Title.Y()-ds.Y())/2));
	}

	// Draw the client area
	GRect c = Client;
	c.Offset(Ox, Oy);
	pDC->SetClient(&c);

	// Draw the grid
	pDC->Colour(LC_MED, 24);
	pDC->Rectangle(0, 0, Client.X()-1, Client.Y()-1);
	pDC->Colour(Rgb24(0x80, 0x80, 0x80), 24);
	for (int y=0; y<Client.Y(); y+=GRID_Y)
	{
		for (int x=0; x<Client.X(); x+=GRID_X)
		{
			pDC->Set(x, y);
		}
	}

	// Paint children
	GView::_Paint(pDC, c.x1, c.y1);

	pDC->SetOrigin(Ox, Oy);
}

/////////////////////////////////////////////////////////////////////
// Text box
CtrlText::CtrlText(ResDialog *dlg, GXmlTag *load) :
	ResDialogCtrl(dlg, Res_StaticText, load)
{
	if (Str && !load)
	{
		Str->SetDefine("IDC_STATIC");
	}
}

IMPL_DIALOG_CTRL(CtrlText)

void CtrlText::OnPaint(GSurface *pDC)
{
	Client.ZOff(X()-1, Y()-1);
	char *Text = Str->Get();
	SysFont->Fore(0);
	SysFont->Transparent(true);

	if (Text)
	{
		GRect Client = GetClient();

		int y = 0;
		char *Start = Text;
		for (char *s = Text; 1; s++)
		{
			if ((*s == '\\' && *(s+1) == 'n') || (*s == 0))
			{
				GDisplayString ds(SysFont, Start, s - Start);
				ds.Draw(pDC, 0, y, &Client);
				y += 15;
				Start = s + 2;
				if (*s == '\\') s++;
			}

			if (*s == 0) break;
		}
	}
}

// Editbox

CtrlEditbox::CtrlEditbox(ResDialog *dlg, GXmlTag *load) :
	ResDialogCtrl(dlg, Res_EditBox, load)
{
	Password = false;
	MultiLine = false;
}

IMPL_DIALOG_CTRL(CtrlEditbox)

void CtrlEditbox::OnPaint(GSurface *pDC)
{
	GRect r(0, 0, X()-1, Y()-1);
	Client = r;
	
	// Draw the ctrl
	LgiWideBorder(pDC, r, SUNKEN);
	pDC->Colour(Enabled() ? LC_WORKSPACE : LC_MED, 24);
	pDC->Rectangle(&r);

	char *Text = Str->Get();
	SysFont->Fore(Enabled() ? 0 : LC_LOW);
	SysFont->Transparent(true);
	if (Text)
	{
		if (Password)
		{
			char *t = NewStr(Text);
			if (t)
			{
				for (char *p = t; *p; p++) *p = '*';
				GDisplayString ds(SysFont, t);
				ds.Draw(pDC, 4, 4);
				DeleteArray(t);
			}
		}
		else
		{
			GDisplayString ds(SysFont, Text);
			ds.Draw(pDC, 4, 4);
		}
	}

	// Draw any rubber band
	ResDialogCtrl::OnPaint(pDC);
}

#define VAL_Password		"Pw"
#define VAL_MultiLine		"MultiLine"

bool CtrlEditbox::GetFields(FieldTree &Fields)
{
	bool Status = ResDialogCtrl::GetFields(Fields);
	if (Status)
	{
		Fields.Insert(this, DATA_BOOL, 300, VAL_Password, "Password");
		Fields.Insert(this, DATA_BOOL, 301, VAL_MultiLine, "MultiLine");
	}
	return Status;
}

bool CtrlEditbox::Serialize(FieldTree &Fields)
{
	bool Status = ResDialogCtrl::Serialize(Fields);
	if (Status)
	{
		Fields.Serialize(this, VAL_Password, Password, false);
		Fields.Serialize(this, VAL_MultiLine, MultiLine, false);
	}
	return Status;
}

// Check box
CtrlCheckbox::CtrlCheckbox(ResDialog *dlg, GXmlTag *load) :
	ResDialogCtrl(dlg, Res_CheckBox, load)
{
}

IMPL_DIALOG_CTRL(CtrlCheckbox)

void CtrlCheckbox::OnPaint(GSurface *pDC)
{
	Client.ZOff(X()-1, Y()-1);
	GRect r(0, 0, 12, 12);
	
	// Draw the ctrl
	LgiWideBorder(pDC, r, SUNKEN);
	pDC->Colour(LC_WORKSPACE, 24);
	pDC->Rectangle(&r);

	GdcPt2 Pt[6] = {
		GdcPt2(3, 4),
		GdcPt2(3, 7),
		GdcPt2(5, 10),
		GdcPt2(10, 5),
		GdcPt2(10, 2),
		GdcPt2(5, 7)};
	pDC->Colour(0);
	pDC->Polygon(6, Pt);
	pDC->Set(3, 5);

	char *Text = Str->Get();
	if (Text)
	{
		SysFont->Fore(LC_TEXT);
		SysFont->Transparent(true);
		GDisplayString ds(SysFont, Text);
		ds.Draw(pDC, r.x2 + 10, r.y1-2);
	}

	// Draw any rubber band
	ResDialogCtrl::OnPaint(pDC);
}

// Button

CtrlButton::CtrlButton(ResDialog *dlg, GXmlTag *load) :
	ResDialogCtrl(dlg, Res_Button, load)
{
}

IMPL_DIALOG_CTRL(CtrlButton)

void CtrlButton::OnPaint(GSurface *pDC)
{
	Client.ZOff(X()-1, Y()-1);
	GRect r = Client;
	char *Text = Str->Get();
	
	// Draw the ctrl
	LgiWideBorder(pDC, r, RAISED);
	SysFont->Fore(LC_TEXT);

	if (ValidStr(Text))
	{
		SysFont->Back(LC_MED);
		SysFont->Transparent(false);

		GDisplayString ds(SysFont, Text);
		ds.Draw(pDC, r.x1 + ((r.X()-ds.X())/2), r.y1 + ((r.Y()-ds.Y())/2), &r);
	}
	else
	{
		pDC->Colour(LC_MED, 24);
		pDC->Rectangle(&r);
	}

	// Draw any rubber band
	ResDialogCtrl::OnPaint(pDC);
}

// Group

CtrlGroup::CtrlGroup(ResDialog *dlg, GXmlTag *load) :
	ResDialogCtrl(dlg, Res_Group, load)
{
	AcceptChildren = true;

	if (Str && !load)
	{
		Str->SetDefine("IDC_STATIC");
	}
}

IMPL_DIALOG_CTRL(CtrlGroup)

void CtrlGroup::OnPaint(GSurface *pDC)
{
	Client.ZOff(X()-1, Y()-1);
	GRect r = Client;
	
	// Draw the ctrl
	r.y1 += 5;
	LgiWideBorder(pDC, r, CHISEL);
	r.y1 -= 5;
	SysFont->Fore(LC_TEXT);
	SysFont->Back(LC_MED);
	SysFont->Transparent(false);
	char *Text = Str->Get();
	GDisplayString ds(SysFont, Text);
	ds.Draw(pDC, r.x1 + 8, r.y1 - 2);

	// Draw children
	//GWindow::OnPaint(pDC);

	// Draw any rubber band
	ResDialogCtrl::OnPaint(pDC);
}

//Radio button

uint32 RadioBmp[] = {
0xC0C0C0C0, 0xC0C0C0C0, 0xC0C0C0C0, 0x80808080, 0x80808080, 0x80808080, 0xC0C0C0C0, 0xC0C0C0C0, 
0xC0C0C0C0, 0xC0C0C0C0, 0x8080C0C0, 0x80808080, 0x00000000, 0x00000000, 0x00000000, 0x80808080, 
0xC0C08080, 0xC0C0C0C0, 0x80C0C0C0, 0x00008080, 0x00000000, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 
0x00000000, 0xFFFF0000, 0xC0C0C0FF, 0x80C0C0C0, 0x00008080, 0xFFFFFF00, 0xFFFFFFFF, 0xFFFFFFFF, 
0xFFFFFFFF, 0xDFFFFFFF, 0xFFFFDFDF, 0xC0C0C0FF, 0x00808080, 0xFFFF0000, 0xFFFFFFFF, 0x00FFFFFF, 
0x00000000, 0xFFFFFF00, 0xFFFFFFFF, 0xDFDFFFFF, 0xFFFFFFDF, 0x00808080, 0xFFFF0000, 0xFFFFFFFF, 
0x00000000, 0x00000000, 0x00000000, 0xFFFFFFFF, 0xDFDFFFFF, 0xFFFFFFDF, 0x00808080, 0xFFFF0000, 
0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000, 0xFFFFFFFF, 0xDFDFFFFF, 0xFFFFFFDF, 0x00808080, 
0xFFFF0000, 0xFFFFFFFF, 0x00FFFFFF, 0x00000000, 0xFFFFFF00, 0xFFFFFFFF, 0xDFDFFFFF, 0xFFFFFFDF, 
0x80C0C0C0, 0x00008080, 0xFFFFFF00, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xDFFFFFFF, 0xFFFFDFDF, 
0xC0C0C0FF, 0x80C0C0C0, 0xDFDF8080, 0xDFDFDFDF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xDFDFDFDF, 
0xFFFFDFDF, 0xC0C0C0FF, 0xC0C0C0C0, 0xFFFFC0C0, 0xFFFFFFFF, 0xDFDFDFDF, 0xDFDFDFDF, 0xDFDFDFDF, 
0xFFFFFFFF, 0xC0C0FFFF, 0xC0C0C0C0, 0xC0C0C0C0, 0xC0C0C0C0, 0xC0C0C0C0, 0xFFFFFFFF, 0xFFFFFFFF, 
0xFFFFFFFF, 0xC0C0C0C0, 0xC0C0C0C0, 0xC0C0C0C0};

CtrlRadio::CtrlRadio(ResDialog *dlg, GXmlTag *load) :
	ResDialogCtrl(dlg, Res_RadioBox, load)
{
	Bmp = new GMemDC;
	if (Bmp && Bmp->Create(12, 12,
		#ifdef MAC
		32
		#else
		24
		#endif
		))
	{
		int Len = ((Bmp->X()*Bmp->GetBits())+31)/32*4;
		for (int y=0; y<Bmp->Y(); y++)
		{
			for (int x=0; x<Bmp->X(); x++)
			{
				uchar *s = ((uchar*) RadioBmp) + (y * Len) + (x * 3);
				Bmp->Colour(Rgb24(s[0], s[1], s[2]), 24);
				Bmp->Set(x, y);
			}
		}
	}
}

CtrlRadio::~CtrlRadio()
{
	DeleteObj(Bmp);
}

IMPL_DIALOG_CTRL(CtrlRadio)

void CtrlRadio::OnPaint(GSurface *pDC)
{
	Client.ZOff(X()-1, Y()-1);
	GRect r(0, 0, 12, 12);
	
	// Draw the ctrl
	if (RadioBmp)
	{
		pDC->Blt(r.x1, r.y1, Bmp);
	}

	char *Text = Str->Get();
	if (Text)
	{
		SysFont->Fore(LC_TEXT);
		SysFont->Transparent(true);
		GDisplayString ds(SysFont, Text);
		ds.Draw(pDC, r.x2 + 10, r.y1-2);
	}

	// Draw any rubber band
	ResDialogCtrl::OnPaint(pDC);
}

// Tab

CtrlTab::CtrlTab(ResDialog *dlg, GXmlTag *load) :
	ResDialogCtrl(dlg, Res_Tab, load)
{
}

IMPL_DIALOG_CTRL(CtrlTab)

void CtrlTab::OnPaint(GSurface *pDC)
{
}

void CtrlTab::ListChildren(List<ResDialogCtrl> &l, bool Deep)
{
	ResDialogCtrl *Ctrl = dynamic_cast<ResDialogCtrl*>(GetParent());
	CtrlTabs *Par = dynamic_cast<CtrlTabs*>(Ctrl);
	LgiAssert(Par);

	int MyIndex = Par->Tabs.IndexOf(this);
	LgiAssert(MyIndex >= 0);

	List<GViewI> *CList = (Par->Current == MyIndex) ? &Par->Children : &Children;
	for (GViewI *w = CList->First(); w; w = CList->Next())
	{
		ResDialogCtrl *c = dynamic_cast<ResDialogCtrl*>(w);
		if (c)
		{
			l.Insert(c);
			if (Deep)
			{
				c->ListChildren(l, Deep);
			}
		}
	}
}

// Tab control

CtrlTabs::CtrlTabs(ResDialog *dlg, GXmlTag *load) :
	ResDialogCtrl(dlg, Res_TabView, load)
{
	AcceptChildren = true;
	Current = 0;

	if (!load)
	{
		for (int i=0; i<3; i++)
		{
			CtrlTab *t = new CtrlTab(dlg, load);
			if (t)
			{
				char Text[256];
				sprintf(Text, "Tab %i", i+1);
				if (t->Str)
				{
					t->Str->Set(Text);
					t->SetParent(this);
					Tabs.Insert(t);
				}
				else
				{
					DeleteObj(t);
				}
			}
		}
	}
}

CtrlTabs::~CtrlTabs()
{
	Empty();
}

void CtrlTabs::OnMouseMove(GMouse &m)
{
	ResDialogCtrl::OnMouseMove(m);
}

void CtrlTabs::ShowMe(ResDialogCtrl *Child)
{
	CtrlTab *t = dynamic_cast<CtrlTab*>(Child);
	if (t)
	{
		int Idx = Tabs.IndexOf(t);
		if (Idx >= 0)
		{
			ToTab();
			Current = Idx;
			FromTab();
		}
	}
}

void CtrlTabs::EnumCtrls(List<ResDialogCtrl> &Ctrls)
{
	List<CtrlTab>::I it = Tabs.Start();
	for (CtrlTab *t = *it; t; t = *++it)
	{
		t->EnumCtrls(Ctrls);
	}

	ResDialogCtrl::EnumCtrls(Ctrls);
}

GRect CtrlTabs::GetMinSize()
{
	List<ResDialogCtrl> l;
	ListChildren(l, false);

	GRect r(0, 0, GRID_X-1, GRID_Y-1);

	/*
	// don't resize smaller than the tabs
	for (CtrlTab *t = Tabs.First(); t; t = Tabs.Next())
	{
		r.x2 = max(r.x2, t->r.x2 + 10);
		r.y2 = max(r.y2, t->r.y2 + 10);
	}
	*/

	// don't resize smaller than any of the children
	// on any of the tabs
	GRect cli = GetClient(false);
	for (ResDialogCtrl *c=l.First(); c; c=l.Next())
	{
		GRect cpos = c->View()->GetPos();
		cpos.Offset(cli.x1, cli.y1);
		r.Union(&cpos);
	}

	return r;
}

void CtrlTabs::ListChildren(List<ResDialogCtrl> &l, bool Deep)
{
	int n=0;
	for (CtrlTab *t = Tabs.First(); t; t = Tabs.Next(), n++)
	{
	    l.Add(t);
	    
		GAutoPtr<GViewIterator> It((Current == n ? (GViewI*)this : (GViewI*)t)->IterateViews());
		for (GViewI *w = It->First(); w; w = It->Next())
		{
			ResDialogCtrl *c = dynamic_cast<ResDialogCtrl*>(w);
			if (c)
			{
				l.Insert(c);
				c->ListChildren(l, Deep);
			}
		}
	}
}

void CtrlTabs::Empty()
{
	ToTab();
	Tabs.DeleteObjects();
}

void CtrlTabs::OnPaint(GSurface *pDC)
{
	// Draw the ctrl
	Title.ZOff(X()-1, 17);
	Client.ZOff(X()-1, Y()-1);
	Client.y1 = Title.y2;
	GRect r = Client;
	LgiWideBorder(pDC, r, RAISED);

	// Draw the tabs
	int i = 0;
	int x = 2;
	for (CtrlTab *Tab = Tabs.First(); Tab; Tab = Tabs.Next(), i++)
	{
		char *Str = Tab->Str ? Tab->Str->Get() : 0;
		GDisplayString ds(SysFont, Str);

		int Width = 12 + ds.X();
		GRect t(x, Title.y1 + 2, x + Width - 1, Title.y2 - 1);

		if (Current == i)
		{
			t.Size(-2, -2);

			GAutoPtr<GViewIterator> It(Tab->IterateViews());
			if (It->Length() > 0)
			{
				FromTab();
			}
		}

		Tab->View()->SetPos(t);

		pDC->Colour(LC_LIGHT, 24);
		pDC->Line(t.x1, t.y1+2, t.x1, t.y2);
		pDC->Set(t.x1+1, t.y1+1);
		pDC->Line(t.x1+2, t.y1, t.x2-2, t.y1);

		pDC->Colour(LC_MED, 24);
		pDC->Line(t.x1+1, t.y1+2, t.x1+1, t.y2);
		pDC->Line(t.x1+2, t.y1+1, t.x2-2, t.y1+1);

		pDC->Colour(LC_LOW, 24);
		pDC->Line(t.x2-1, t.y1, t.x2-1, t.y2);
		pDC->Colour(0, 24);
		pDC->Line(t.x2, t.y1+2, t.x2, t.y2);

		t.Size(2, 2);
		t.y2 += 2;

		SysFont->Fore(LC_TEXT);
		SysFont->Back(LC_MED);
		SysFont->Transparent(false);
		ds.Draw(pDC, t.x1 + ((t.X()-ds.X())/2), t.y1 + ((t.Y()-ds.Y())/2), &t);

		x += Width + ((Current == i) ? 2 : 1);
	}

	// Draw children
	//GWindow::OnPaint(pDC);

	// Draw any rubber band
	ResDialogCtrl::OnPaint(pDC);
}

void CtrlTabs::ToTab()
{
	CtrlTab *Cur = Tabs.ItemAt(Current);
	if (Cur)
	{
		// move all our children into the tab losing focus
		GAutoPtr<GViewIterator> CurIt(Cur->IterateViews());
		GAutoPtr<GViewIterator> ThisIt(IterateViews());
		GViewI *v;
		while ((v = CurIt->First()))
		{
			Cur->DelView(v);
		}
		while ((v = ThisIt->First()))
		{
			DelView(v);
			Cur->AddView(v);
		}
	}
}

void CtrlTabs::FromTab()
{
	CtrlTab *Cur = Tabs.ItemAt(Current);
	if (Cur)
	{
		// load all our children from the new tab
		GAutoPtr<GViewIterator> CurIt(Cur->IterateViews());
		GAutoPtr<GViewIterator> ThisIt(IterateViews());
		GViewI *v;
		while ((v = ThisIt->First()))
		{
			DelView(v);
		}
		while ((v = CurIt->First()))
		{
			Cur->DelView(v);
			AddView(v);
		}
	}
}

void CtrlTabs::OnMouseClick(GMouse &m)
{
	if (m.Down())
	{
		if (Title.Overlap(m.x, m.y))
		{
			// select current tab
			int i = 0;
			for (CtrlTab *Tab = Tabs.First(); Tab; Tab = Tabs.Next(), i++)
			{
				if (Tab->View()->GetPos().Overlap(m.x, m.y) /* && i != Current*/)
				{
					ToTab();
					Current = i;
					FromTab();

					Dlg->OnSelect(Tab);
					Invalidate();

					break;
				}
			}
		}

		if (m.IsContextMenu() && Title.Overlap(m.x, m.y))
		{
			GSubMenu *RClick = new GSubMenu;
			if (RClick)
			{
				bool HasTab = Tabs.ItemAt(Current);

				RClick->AppendItem("New tab", IDM_NEW, true);
				RClick->AppendItem("Delete tab", IDM_DELETE, HasTab);
				RClick->AppendItem("Rename tab", IDM_RENAME, HasTab);
				RClick->AppendItem("Move tab left", IDM_MOVE_LEFT, HasTab);
				RClick->AppendItem("Move tab right", IDM_MOVE_RIGHT, HasTab);
				RClick->AppendSeparator();

				RClick->AppendItem("Copy Text", IDM_COPY_TEXT, Dlg->Selection.Length()==1);
				RClick->AppendItem("Paste Text", IDM_PASTE_TEXT, true);

				if (GetMouse(m, true))
				{
					switch (RClick->Float(this, m.x, m.y, false))
					{
						case IDM_NEW:
						{
							CtrlTab *t = new CtrlTab(Dlg, 0);
							if (t)
							{
								char Text[256];
								sprintf(Text, "Tab %i", Tabs.Length()+1);
								t->Str->Set(Text);
								t->SetParent(this);

								Tabs.Insert(t);
							}
							break;
						}
						case IDM_DELETE:
						{
							CtrlTab *t = Tabs.ItemAt(Current);
							if (t)
							{
								ToTab();
								Tabs.Delete(t);
								DeleteObj(t);
								FromTab();
							}
							break;
						}
						case IDM_RENAME:
						{
							CtrlTab *t = Tabs.ItemAt(Current);
							if (t)
							{
								if (!t->Str)
									t->Str = Dlg->CreateSymbol();

								GInput Input(this, t->Str->Get(), "Enter tab name:", "Rename");
								Input.SetParent(Dlg);
								if (Input.DoModal() && Input.Str)
								{
									t->Str->Set(Input.Str);
								}
							}
							break;
						}
						case IDM_MOVE_LEFT:
						{
							CtrlTab *t = Tabs.ItemAt(Current);
							if (t && Current > 0)
							{
								Tabs.Delete(t);
								Tabs.Insert(t, --Current);
							}
							break;
						}
						case IDM_MOVE_RIGHT:
						{
							CtrlTab *t = Tabs.ItemAt(Current);
							if (t && Current < Tabs.Length()-1)
							{
								Tabs.Delete(t);
								Tabs.Insert(t, ++Current);
							}
							break;
						}
						case IDM_COPY_TEXT:
						{
							CtrlTab *t = Tabs.ItemAt(Current);
							if (t)
							{
								t->CopyText();
							}
							break;
						}
						case IDM_PASTE_TEXT:
						{
							CtrlTab *t = Tabs.ItemAt(Current);
							if (t)
							{
								t->PasteText();
							}
							break;
						}
					}

					Invalidate();
				}
			}

			return;
		}
	}
	else
	{
		int asd=0;
	}

	ResDialogCtrl::OnMouseClick(m);
}

// List column

ListCol::ListCol(ResDialog *dlg, GXmlTag *load, char *s, int Width) :
	ResDialogCtrl(dlg, Res_Column, load)
{
	if (s && Str)
	{
		Str->Set(s);
	}
	
	GRect r(0, 0, Width-1, 18);
	ResDialogCtrl::SetPos(r);
}

IMPL_DIALOG_CTRL(ListCol)
void ListCol::OnPaint(GSurface *pDC)
{
}

// List control

CtrlList::CtrlList(ResDialog *dlg, GXmlTag *load) :
	ResDialogCtrl(dlg, Res_ListView, load)
{
	DragCol = -1;
}

CtrlList::~CtrlList()
{
	Empty();
}

void CtrlList::ListChildren(List<ResDialogCtrl> &l, bool Deep)
{
	if (Deep)
	{
		for (GView *w = Cols.First(); w; w = Cols.Next())
		{
			ResDialogCtrl *c = dynamic_cast<ResDialogCtrl*>(w);
			if (c)
			{
				l.Insert(c);
				c->ListChildren(l);
			}
		}
	}
}

void CtrlList::Empty()
{
	for (ListCol *c = Cols.First(); c; c = Cols.Next())
	{
		DeleteObj(c);
	}
	Cols.Empty();
}

void CtrlList::OnMouseClick(GMouse &m)
{
	if (m.Down())
	{
		if (Title.Overlap(m.x, m.y))
		{
			int x=0;
			ListCol *c = 0;
			int DragOver = -1;
			for (ListCol *Col = Cols.First(); Col; Col = Cols.Next())
			{
				if (m.x >= Col->r().x1 && m.x <= Col->r().x2)
				{
					if (m.x > Col->r().x2 - 6)
					{
						DragOver = Cols.IndexOf(Col);
					}
					if (m.x < Col->r().x1 + 6)
					{
						DragOver = Cols.IndexOf(Col) - 1;
					}
					c = Col;
					break;
				}
				x += Col->r().X();
			}

			if (m.Left())
			{
				if (DragOver >= 0)
				{
					DragCol = DragOver;
					Capture(true);
				}
			}
			else if (m.IsContextMenu())
			{
				GSubMenu *RClick = new GSubMenu;
				if (RClick)
				{
					bool HasCol = c != 0;
					RClick->AppendItem("New column", IDM_NEW, true);
					RClick->AppendItem("Delete column", IDM_DELETE, HasCol);
					RClick->AppendItem("Rename column", IDM_RENAME, HasCol);
					RClick->AppendItem("Move column left", IDM_MOVE_LEFT, HasCol);
					RClick->AppendItem("Move column right", IDM_MOVE_RIGHT, HasCol);
					RClick->AppendSeparator();

					RClick->AppendItem("Copy Text", IDM_COPY_TEXT, HasCol);
					RClick->AppendItem("Paste Text", IDM_PASTE_TEXT, HasCol);

					if (GetMouse(m, true))
					{
						switch (RClick->Float(this, m.x, m.y, false))
						{
							case IDM_COPY_TEXT:
							{
								if (c && c->Str)
								{
									c->Str->CopyText();
								}
								break;
							}
							case IDM_PASTE_TEXT:
							{
								if (c && c->Str)
								{
									c->Str->PasteText();
								}
								break;
							}
							case IDM_NEW:
							{
								ListCol *c = dynamic_cast<ListCol*>(Dlg->CreateCtrl(UI_COLUMN,0));
								if (c)
								{
									char Text[256];
									sprintf(Text, "Col %i", Cols.Length()+1);
									c->Str->Set(Text);
									Cols.Insert(c);
								}
								break;
							}
							case IDM_DELETE:
							{
								Cols.Delete(c);
								DeleteObj(c);
								break;
							}
							case IDM_RENAME:
							{
								if (c)
								{
									GInput Input(this, c->Str->Get(), "Enter column name:", "Rename");
									Input.SetParent(Dlg);
									if (Input.DoModal())
									{
										c->Str->Set(Input.Str);
									}
								}
								break;
							}
							case IDM_MOVE_LEFT:
							{
								int Current = Cols.IndexOf(c);
								if (c && Current > 0)
								{
									Cols.Delete(c);
									Cols.Insert(c, --Current);
								}
								break;
							}
							case IDM_MOVE_RIGHT:
							{
								int Current = Cols.IndexOf(c);
								if (c && Current < Cols.Length()-1)
								{
									Cols.Delete(c);
									Cols.Insert(c, ++Current);
								}
								break;
							}
						}

						Invalidate();
					}

					DeleteObj(RClick);
					return;
				}
			}

			return;
		}
	}
	else
	{
		if (DragCol >= 0)
		{
			Capture(false);
			DragCol = -1;
		}
	}

	ResDialogCtrl::OnMouseClick(m);
}

void CtrlList::OnMouseMove(GMouse &m)
{
	if (DragCol >= 0)
	{
		int i=0, x=0;;
		for (ListCol *Col = Cols.First(); Col; Col = Cols.Next(), i++)
		{
			if (i == DragCol)
			{
				int Dx = (m.x - x - Title.x1);
				
				GRect r = Col->GetPos();
				r.x2 = r.x1 + Dx;
				Col->ResDialogCtrl::SetPos(r);
				break;
			}

			x += Col->r().X();
		}

		Invalidate();
	}

	ResDialogCtrl::OnMouseMove(m);
}

void CtrlList::OnPaint(GSurface *pDC)
{
	GRect r(0, 0, X()-1, Y()-1);

	// Draw the ctrl
	LgiWideBorder(pDC, r, SUNKEN);
	Title = r;
	Title.y2 = Title.y1 + 15;
	Client = r;
	Client.y1 = Title.y2 + 1;

	pDC->Colour(LC_WORKSPACE, 24);
	pDC->Rectangle(&Client);

	int x = Title.x1;
	for (ListCol *c = Cols.First(); c; c = Cols.Next())
	{
		int Width = c->r().X();
		c->r().Set(x, Title.y1, x + Width - 1, Title.y2);
		GRect r = c->r();
		r.x2 = min(r.x2, Title.x2);
		x = r.x2 + 1;
		if (r.Valid())
		{
			LgiWideBorder(pDC, r, RAISED);

			SysFont->Fore(LC_TEXT);
			SysFont->Back(LC_MED);
			SysFont->Transparent(false);

			const char *Str = c->Str->Get();
			if (!Str) Str = "";
			GDisplayString ds(SysFont, Str);
			ds.Draw(pDC, r.x1 + 2, r.y1 + ((r.Y()-ds.Y())/2) - 1, &r);
		}
	}

	GRect Client(x, Title.y1, Title.x2, Title.y2);
	if (Client.Valid())
	{
		LgiWideBorder(pDC, Client, RAISED);
		pDC->Colour(LC_MED, 24);;
		pDC->Rectangle(&Client);
	}

	// Draw any rubber band
	ResDialogCtrl::OnPaint(pDC);
}

// Combo box
CtrlComboBox::CtrlComboBox(ResDialog *dlg, GXmlTag *load) :
	ResDialogCtrl(dlg, Res_ComboBox, load)
{
}

IMPL_DIALOG_CTRL(CtrlComboBox)

void CtrlComboBox::OnPaint(GSurface *pDC)
{
	GRect r(0, 0, X()-1, Y()-1);
	Client = r;
	
	// Draw the ctrl
	LgiWideBorder(pDC, r, SUNKEN);
	
	// Allocate space
	GRect e = r;
	e.x2 -= 15;
	GRect d = r;
	d.x1 = e.x2 + 1;

	// Draw edit
	pDC->Colour(LC_WORKSPACE, 24);
	pDC->Rectangle(&e);

	// Draw drap down
	LgiWideBorder(pDC, d, RAISED);
	pDC->Colour(LC_MED, 24);
	pDC->Rectangle(&d);

	pDC->Colour(0, 24);
	int Size = 4;
	int cx = (d.X()/2) + d.x1 - 4;
	int cy = (d.Y()/2) + d.y1 - 3;
	for (int i=1; i<=Size; i++)
	{
		pDC->Line(cx+i, cy+i, cx+(Size*2)-i, cy+i);
	}

	// Text
	char *Text = Str->Get();
	SysFont->Fore(0);
	SysFont->Transparent(true);
	if (Text)
	{
		GDisplayString ds(SysFont, Text);
		ds.Draw(pDC, 4, 4);
	}

	// Draw any rubber band
	ResDialogCtrl::OnPaint(pDC);
}

////////////////////////////////////////////////////////////////////
CtrlScrollBar::CtrlScrollBar(ResDialog *dlg, GXmlTag *load) :
	ResDialogCtrl(dlg, Res_ScrollBar, load)
{
}

IMPL_DIALOG_CTRL(CtrlScrollBar)

void CtrlScrollBar::OnPaint(GSurface *pDC)
{
	GRect r(0, 0, X()-1, Y()-1);
	Client = r;
	
	// Draw the ctrl
	bool Vertical = r.Y() > r.X();
	int ButSize = Vertical ? r.X() : r.Y();
	GRect a, b, c;
	if (Vertical)
	{
		a.Set(r.x1, r.y1, r.x2, r.y1 + ButSize);
		c.Set(r.x1, r.y2 - ButSize, r.x2, r.y2);
		b.Set(r.x1, a.y2 + 1, r.x2, c.y1 - 1);
	}
	else
	{
		a.Set(r.x1, r.y1, r.x1 + ButSize, r.y2);
		c.Set(r.x2 - ButSize, r.y1, r.x2, r.y2);
		b.Set(a.x2 + 1, r.y1, c.x1 - 1, r.y2);
	}

	// Buttons
	LgiWideBorder(pDC, a, RAISED);
	LgiWideBorder(pDC, c, RAISED);
	pDC->Colour(LC_MED, 24);
	pDC->Rectangle(&a);
	pDC->Rectangle(&c);

	// Arrows
	pDC->Colour(0);
	int x = a.x1 + (a.X()/2) - 2;
	int y = a.y1 + (a.Y()/2);
	int i;
	for (i=0; i<5; i++)
	{
		pDC->Line(x+i, y-i, x+i, y+i);
	}
	x = c.x1 + (c.X()/2) - 2;
	y = c.y1 + (c.Y()/2);
	for (i=0; i<5; i++)
	{
		pDC->Line(x+i, y-(4-i), x+i, y+(4-i));
	}

	// Slider
	pDC->Colour(0xd0d0d0, 24);
	pDC->Rectangle(&b);

	// Draw any rubber band
	ResDialogCtrl::OnPaint(pDC);
}

////////////////////////////////////////////////////////////////////
CtrlTree::CtrlTree(ResDialog *dlg, GXmlTag *load) :
	ResDialogCtrl(dlg, Res_TreeView, load)
{
}

IMPL_DIALOG_CTRL(CtrlTree)

void CtrlTree::OnPaint(GSurface *pDC)
{
	GRect r(0, 0, X()-1, Y()-1);
	Client = r;

	LgiWideBorder(pDC, r, SUNKEN);
	pDC->Colour(Rgb24(255, 255, 255), 24);
	pDC->Rectangle(&r);
	SysFont->Colour(0, 0xffffff);
	SysFont->Transparent(true);
	GDisplayString ds(SysFont, "Tree");
	ds.Draw(pDC, r.x1 + 3, r.y1 + 3, &r);

	// Draw any rubber band
	ResDialogCtrl::OnPaint(pDC);
}

////////////////////////////////////////////////////////////////////
CtrlBitmap::CtrlBitmap(ResDialog *dlg, GXmlTag *load) :
	ResDialogCtrl(dlg, Res_Bitmap, load)
{
}

IMPL_DIALOG_CTRL(CtrlBitmap)

void CtrlBitmap::OnPaint(GSurface *pDC)
{
	GRect r(0, 0, X()-1, Y()-1);
	Client = r;

	LgiWideBorder(pDC, r, SUNKEN);
	pDC->Colour(Rgb24(255, 255, 255), 24);
	pDC->Rectangle(&r);
	pDC->Colour(0, 24);
	pDC->Line(r.x1, r.y1, r.x2, r.y2);
	pDC->Line(r.x2, r.y1, r.x1, r.y2);

	// Draw any rubber band
	ResDialogCtrl::OnPaint(pDC);
}

////////////////////////////////////////////////////////////////////
CtrlProgress::CtrlProgress(ResDialog *dlg, GXmlTag *load) :
	ResDialogCtrl(dlg, Res_Progress, load)
{
}

IMPL_DIALOG_CTRL(CtrlProgress)

void CtrlProgress::OnPaint(GSurface *pDC)
{
	GRect r(0, 0, X()-1, Y()-1);
	Client = r;

	LgiWideBorder(pDC, r, SUNKEN);

	COLOUR Flat = Rgb24(0x7f, 0x7f, 0x7f);
	COLOUR White = Rgb24(0xff, 0xff, 0xff);
	int Pos = 60;
	int x = Pos * r.X() / 100;

	pDC->Colour(Flat, 24);
	pDC->Rectangle(r.x1, r.y1, r.x1 + x, r.y2);
	
	pDC->Colour(White, 24);
	pDC->Rectangle(r.x1 + x + 1, r.y1, r.x2, r.y2);

	// Draw any rubber band
	ResDialogCtrl::OnPaint(pDC);
}

////////////////////////////////////////////////////////////////////
CtrlCustom::CtrlCustom(ResDialog *dlg, GXmlTag *load) :
	ResDialogCtrl(dlg, Res_Custom, load)
{
	Control = 0;
}

IMPL_DIALOG_CTRL(CtrlCustom)

CtrlCustom::~CtrlCustom()
{
	DeleteArray(Control);
}

void CtrlCustom::OnPaint(GSurface *pDC)
{
	GRect r(0, 0, X()-1, Y()-1);
	Client = r;

	LgiWideBorder(pDC, r, SUNKEN);

	COLOUR White = Rgb24(0xff, 0xff, 0xff);

	pDC->Colour(White, 24);
	pDC->Rectangle(&r);

	char s[256] = "Custom: ";
	if (Control)
	{
		strcat(s, Control);
	}

	SysFont->Colour(LC_TEXT, LC_WORKSPACE);
	GDisplayString ds(SysFont, s);
	ds.Draw(pDC, r.x1+2, r.y1+1, &r);

	// Draw any rubber band
	ResDialogCtrl::OnPaint(pDC);
}

#define VAL_Control "Ctrl"
bool CtrlCustom::GetFields(FieldTree &Fields)
{
	bool Status = ResDialogCtrl::GetFields(Fields);
	if (Status)
	{
		Fields.Insert(this, DATA_STR, 320, VAL_Control, "Control", 1);
	}
	return Status;
}

bool CtrlCustom::Serialize(FieldTree &Fields)
{
	bool Status = ResDialogCtrl::Serialize(Fields);
	if (Status)
	{
		Fields.Serialize(this, VAL_Control, Control);
	}
	return Status;
}

////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////
ResStringGroup *ResDialog::Symbols = 0;
int ResDialog::SymbolRefs = 0;
bool ResDialog::CreateSymbols = true;

void ResDialog::AddLanguage(const char *Id)
{
	if (Symbols)
	{
		Symbols->AppendLanguage(Id);
	}
}

void ResDialogCtrl::EnumCtrls(List<ResDialogCtrl> &Ctrls)
{
	Ctrls.Insert(this);

	ChildIterator It(View()->IterateViews());
	for (GViewI *c = It->First(); c; c = It->Next())
	{
		ResDialogCtrl *dc = dynamic_cast<ResDialogCtrl*>(c);
		LgiAssert(dc);
		dc->EnumCtrls(Ctrls);
	}
}

void ResDialog::EnumCtrls(List<ResDialogCtrl> &Ctrls)
{
	for (ResDialogCtrl *c = dynamic_cast<ResDialogCtrl*>(Children.First());
					 c; c = dynamic_cast<ResDialogCtrl*>(Children.Next()))
	{
		c->EnumCtrls(Ctrls);
	}
}

int GooberCaps[] = {
	RESIZE_X1 | RESIZE_Y1,
	RESIZE_Y1,
	RESIZE_X2 | RESIZE_Y1,
	RESIZE_X2,
	RESIZE_X2 | RESIZE_Y2,
	RESIZE_Y2,
	RESIZE_X1 | RESIZE_Y2,
	RESIZE_X1};

ResDialog::ResDialog(AppWnd *w, int type) : 
	Resource(w, type)
{
	// Maintain a string group just for our dialog
	// defines 
	if (!Symbols && w)
	{
		// First check to see of the symbols have been loaded from a file
		// and we don't have a pointer to it yet
		Symbols = App()->GetDialogSymbols();
		if (!Symbols)
		{
			// else we need to create the symbols object
			Symbols = new ResStringGroup(w);
			if (Symbols)
			{
				Symbols->Name(StrDialogSymbols);
				w->InsertObject(TYPE_STRING, Symbols);
			}
		}

		if (Symbols)
		{
			Symbols->SystemObject(true);
		}
	}
	SymbolRefs++;

	// Init dialog resource
	Ui = 0;
	DlgPos.ZOff(500-1, 400-1);

	DragGoober = -1;
	DragX = 0;
	DragY = 0;
	DragCtrl = 0;
}

ResDialog::~ResDialog()
{
	// Decrement and delete the shared string group
	SymbolRefs--;
	if (SymbolRefs < 1)
	{
		App()->DelObject(Symbols);
		Symbols = 0;
	}

	// Delete our Ui
	DeleteObj(Ui);
}

void ResDialog::OnShowLanguages()
{
	// Current language changed.
	OnSelect(Selection.First());
	Invalidate();
	OnLanguageChange();
}

char *ResDialog::Name()
{
	GViewI *v = Children.First();
	ResDialogCtrl *Ctrl = dynamic_cast<ResDialogCtrl*>(v);
	if (Ctrl && Ctrl->Str && Ctrl->Str->GetDefine())
	{
		return Ctrl->Str->GetDefine();
	}
	return (char*)"<no object>";
}

bool ResDialog::Name(const char *n)
{
	ResDialogCtrl *Ctrl = dynamic_cast<ResDialogCtrl*>(Children.First());
	if (Ctrl && Ctrl->Str)
	{
		Ctrl->Str->SetDefine((n)?n:"");
		return Ctrl->Str->GetDefine() != 0;
	}
	return false;
}

char *ResDialog::StringFromRef(int Ref)
{
	if (Symbols)
	{
		ResString *Str = Symbols->FindRef(Ref);
		if (Str)
		{
			return Str->Get();
		}
	}

	return 0;
}

bool ResDialog::Res_GetProperties(ResObject *Obj, GDom *Props)
{
	ResDialogCtrl *Ctrl = dynamic_cast<ResDialogCtrl*>(Obj);
	if (Ctrl && Props)
	{
		int Next = -1000;
		FieldTree t(Next, false);
		t.SetStore(Props);
		t.SetMode(FieldTree::ObjToStore);
		Ctrl->GetFields(t);
		Ctrl->Serialize(t);

		return true;
	}
	return false;
}

GDom *ResDialog::Res_GetDom(ResObject *Obj)
{
	return dynamic_cast<GDom*>(Obj);
}

bool ResDialog::Res_SetProperties(ResObject *Obj, GDom *Props)
{
	ResDialogCtrl *Ctrl = dynamic_cast<ResDialogCtrl*>(Obj);
	if (Ctrl && Props)
	{
		int Next = -1000;
		FieldTree t(Next, false);
		t.SetStore(Props);
		t.SetMode(FieldTree::StoreToObj);
		Ctrl->GetFields(t);
		Ctrl->Serialize(t);

		return true;
	}
	return false;
}

ResObject *ResDialog::CreateObject(GXmlTag *Tag, ResObject *Parent)
{
	return dynamic_cast<ResObject*>(CreateCtrl(Tag));
}

void ResDialog::Res_SetPos(ResObject *Obj, int x1, int y1, int x2, int y2)
{
	if (Obj)
	{
		ResDialogCtrl *Ctrl = dynamic_cast<ResDialogCtrl*>((ResDialogCtrl*)Obj);
		if (Ctrl)
		{
			GRect r(x1, y1, x2, y2);
			Ctrl->SetPos(r);
		}
	}
}

void ResDialog::Res_SetPos(ResObject *Obj, char *s)
{
	if (Obj)
	{
		ResDialogCtrl *Ctrl = dynamic_cast<ResDialogCtrl*>((ResDialogCtrl*)Obj);
		if (Ctrl)
		{
			Ctrl->ReadPos(s);
		}
	}
}

GRect ResDialog::Res_GetPos(ResObject *Obj)
{
	if (Obj)
	{
		ResDialogCtrl *Ctrl = dynamic_cast<ResDialogCtrl*>((ResDialogCtrl*)Obj);
		LgiAssert(Ctrl);
		if (Ctrl)
		{
			return Ctrl->View()->GetPos();
		}
	}
	return GRect(0, 0, 0, 0);
}

int ResDialog::Res_GetStrRef(ResObject *Obj)
{
	if (Obj)
	{
		ResDialogCtrl *Ctrl = dynamic_cast<ResDialogCtrl*>((ResDialogCtrl*)Obj);
		if (Ctrl)
		{
			return Ctrl->Str->GetRef();
		}
	}
	return -1;
}

bool ResDialog::Res_SetStrRef(ResObject *Obj, int Ref, ResReadCtx *Ctx)
{
	ResDialogCtrl *Ctrl = 0;
	if (!Obj || !Symbols)
		return false;

	Ctrl = dynamic_cast<ResDialogCtrl*>((ResDialogCtrl*)Obj);
	if (!Ctrl)
		return false;

	Ctrl->StrFromRef(Ref);
	LgiAssert(Ctrl && Ctrl->Str);
	return Ctrl->Str != 0;
}

void ResDialog::Res_Attach(ResObject *Obj, ResObject *Parent)
{
	if (Obj && Parent)
	{
		ResDialogCtrl *Ctrl = dynamic_cast<ResDialogCtrl*>((ResDialogCtrl*)Obj);
		ResDialogCtrl *Par = dynamic_cast<ResDialogCtrl*>((ResDialogCtrl*)Parent);
		if (Ctrl && Par)
		{
			Par->AttachCtrl(Ctrl);
		}
	}
}

bool ResDialog::Res_GetChildren(ResObject *Obj, List<ResObject> *l, bool Deep)
{
	bool Status = false;
	if (Obj)
	{
		ResDialogCtrl *Ctrl = dynamic_cast<ResDialogCtrl*>((ResDialogCtrl*)Obj);
		if (Ctrl)
		{
			Status = true;

			List<ResDialogCtrl> Child;
			Ctrl->ListChildren(Child, Deep);
			for (ResDialogCtrl *o = Child.First(); o; o = Child.Next())
			{
				l->Insert(o);
			}
		}
	}

	return Status;
}

void ResDialog::Res_Append(ResObject *Obj, ResObject *Parent)
{
	if (Obj && Parent)
	{
		CtrlTabs *Tabs = dynamic_cast<CtrlTabs*>(Obj);
		CtrlTab *Tab = dynamic_cast<CtrlTab*>(Parent);
		if (Tabs && Tab)
		{
			Tab->SetParent(Tabs);
			Tabs->Tabs.Insert(Tab);

			if (Tabs->Tabs.Length() == 1)
			{
				Tabs->FromTab();
			}
			return;
		}

		CtrlList *Lst = dynamic_cast<CtrlList*>(Obj);
		ListCol *Col = dynamic_cast<ListCol*>(Parent);
		if (Lst && Col)
		{
			Lst->Cols.Insert(Col);
			return;
		}
	}
}

bool ResDialog::Res_GetItems(ResObject *Obj, List<ResObject> *l)
{
	if (Obj && l)
	{
		CtrlTabs *Tabs = dynamic_cast<CtrlTabs*>(Obj);
		if (Tabs)
		{
			for (	CtrlTab *Tab = Tabs->Tabs.First();
					Tab;
					Tab = Tabs->Tabs.Next())
			{
				l->Insert(Tab);
			}

			return true;
		}

		CtrlList *Lst = dynamic_cast<CtrlList*>(Obj);
		if (Lst)
		{
			for (	ListCol *Col = Lst->Cols.First();
					Col;
					Col = Lst->Cols.Next())
			{
				l->Insert(Col);
			}

			return true;
		}
	}

	return false;
}

void ResDialog::Create(GXmlTag *load)
{
	CtrlDlg *Dlg = new CtrlDlg(this, load);
	if (Dlg)
	{
		GRect r = DlgPos;
		r.Offset(GOOBER_BORDER, GOOBER_BORDER);
		Children.Insert(Dlg);
		Dlg->SetParent(this);

		if (load)
			Read(load, Lr8File);
		else
		{
			Dlg->ResDialogCtrl::SetPos(r);
			if (Dlg->Str)
				Dlg->Str->Set("Dialog");
		}
	}
}

void ResDialog::Delete()
{
	// Deselect the dialog ctrl
	OnDeselect(dynamic_cast<ResDialogCtrl*>(Children.First()));

	// Delete selected controls
	for (ResDialogCtrl *c = Selection.First(); c; c = Selection.First())
	{
		c->View()->Detach();
		DeleteObj(c);
	}

	// Repaint
	GView::Invalidate();
}

bool IsChild(ResDialogCtrl *Parent, ResDialogCtrl *Child)
{
	if (Parent && Child)
	{
		for (	ResDialogCtrl *c = Child->ParentCtrl();
				c;
				c = c->ParentCtrl())
		{
			if (c == Parent)
			{
				return true;
			}
		}
	}

	return false;
}

void GetTopCtrls(List<ResDialogCtrl> &Top, List<ResDialogCtrl> &Selection)
{
	// all children will automatically be cut as well

	for (ResDialogCtrl *c = Selection.First(); c; c = Selection.Next())
	{
		// is c a child of an item already in Top?
		bool Ignore = false;
		for (ResDialogCtrl *p = Top.First(); p; p = Top.Next())
		{
			if (IsChild(p, c))
			{
				Ignore = true;
				break;
			}
		}

		if (!Ignore)
		{
			// is not a child
			Top.Insert(c);
		}
	}
}

void ResDialog::Copy(bool Delete)
{
	bool Status = false;

	// Deselect the dialog... can't cut that
	OnDeselect(dynamic_cast<ResDialogCtrl*>(Children.First()));
	
	// Get top level list
	List<ResDialogCtrl> Top;
	GetTopCtrls(Top, Selection);

	// ok we have a top level list of ctrls
	// write them to the file
	List<ResDialogCtrl> All;
	ResDialogCtrl *c;
	GXmlTag *Root = new GXmlTag("Resources");
	if (Root)
	{
		if (Delete)
		{
			// remove selection from UI
			AppWindow->OnObjSelect(0);
		}

		// write the string resources first
		for (c = Top.First(); c; c = Top.Next())
		{
			All.Insert(c);
			c->ListChildren(All, true);
		}

		// write the strings out at the top of the block
		// so that we can reference them from the objects
		// below.
		for (c = All.First(); c; c = All.Next())
		{
			// Write the string out
			GXmlTag *t = new GXmlTag;
			if (t && c->Str->Write(t, Lr8File))
			{
				Root->InsertTag(t);
			}
			else
			{
				DeleteObj(t);
			}
		}

		// write the objects themselves
		for (c = Top.First(); c; c = Top.Next())
		{
			GXmlTag *t = new GXmlTag;
			if (t && Res_Write(c, t))
			{
				Root->InsertTag(t);
			}
			else
			{
				DeleteObj(t);
			}
		}


		// Read the file in and copy to the clipboard
		GStringPipe Xml;
		GXmlTree Tree;
		if (Tree.Write(Root, &Xml))
		{
			char *s = Xml.NewStr();
			{
				GClipBoard Clip(Ui);

				char16 *w = LgiNewUtf8To16(s);
				Clip.TextW(w);
				Status = Clip.Text(s, false);
				DeleteObj(w);
			}

			DeleteArray(s);
		}

		DeleteObj(Root);


		if (Delete && Status)
		{
			// delete them
			for (c = Selection.First(); c; c = Selection.First())
			{
				c->View()->Detach();
				Selection.Delete(c);
				DeleteObj(c);
			}
		}

		// Repaint
		GView::Invalidate();
	}
}

class StringId
{
public:
	ResString *Str;
	int OldRef;
	int NewRef;
};

void RemapAllRefs(GXmlTag *t, List<StringId> &Strs)
{
	char *RefStr;
	if ((RefStr = t->GetAttr("Ref")))
	{
		int r = atoi(RefStr);

		for (StringId *i=Strs.First(); i; i=Strs.Next())
		{
			if (i->OldRef == r)
			{
				// this string ref is to be remapped
				char Buf[32];
				sprintf(Buf, "%i", i->NewRef);
				t->SetAttr("Ref", Buf);

				// delete the string ref map
				// it's not needed anymore
				Strs.Delete(i);
				DeleteObj(i);
				
				// leave the loop
				RefStr = 0;
				break;
			}
		}

		if (RefStr)
		{
			// if this assert failes then no map for this
			// string was found. every incomming string needs
			// to be remapped
			LgiAssert(0);
		}
	}

	for (GXmlTag *c = t->Children.First(); c; c = t->Children.Next())
	{
		RemapAllRefs(c, Strs);
	}
}

void ResDialog::Paste()
{
	// Get the clipboard data
	char *Mem = 0;
	char *Data = 0;
	{
		GClipBoard Clip(Ui);
		GAutoWString w(Clip.TextW());
		if (w)
		{
			Data = Mem = LgiNewUtf16To8(w);
		}
		else
		{
			Data = Clip.Text();
		}
	}
	if (Data)
	{
		ResDialogCtrl *Container = 0;

		// Find a container to plonk the controls in
		ResDialogCtrl *c = Selection.First();
		if (c && c->IsContainer())
		{
			Container = c;
		}

		if (!Container)
		{
			// Otherwise just use the dialog as the container
			Container = dynamic_cast<ResDialogCtrl*>(Children.First());
		}

		if (Container)
		{
			// remap list
			List<StringId> Strings;
			int NextRef = 0;

			// Deselect everything
			OnSelect(NULL);

			// Parse the data
			List<ResString> NewStrs;
			GXmlTree Tree;
			GStringPipe p;
			p.Push(Data);
			
			// Create the new controls, strings first
			// that way we can setup the remapping properly to avoid
			// string ref duplicates
			GXmlTag Root;
			if (Tree.Read(&Root, &p, 0))
			{
				GXmlTag *t;
				for (t = Root.Children.First(); t; t = Root.Children.Next())
				{
					if (stricmp(t->Tag, "string") == 0)
					{
						// string tag
						LgiAssert(Symbols);

						ResString *Str = Symbols->CreateStr();
						if (Str && Str->Read(t, Lr8File))
						{
							// setup remap object, so that we can make all the strings
							// unique
							StringId *Id = new StringId;
							LgiAssert(Id);
							Id->Str = Str;
							Id->OldRef = Str->GetRef();
							NextRef = Str->SetRef(Id->NewRef = AppWindow->GetUniqueStrRef(NextRef + 1));
							Strings.Insert(Id);

							// insert out new string
							NewStrs.Insert(Str);
						}
						else
						{
							break;
						}
					}
					else
					{
						// object tag

						// all strings should have been processed by the time 
						// we get in here. We remap the incomming string refs to 
						// unique values so that we don't get conflicts later.
						RemapAllRefs(t, Strings);
					}
				}

				// load all the objects now
				List<ResDialogCtrl> NewCtrls;
				for (t = Root.Children.First(); t; t = Root.Children.Next())
				{
					if (stricmp(t->Tag, "string") != 0)
					{
						// object (not string)
						CreateSymbols = false;
						ResDialogCtrl *Ctrl = dynamic_cast<ResDialogCtrl*>(CreateCtrl(t));
						CreateSymbols = true;
						ResReadCtx Ctx;
						if (Ctrl && Res_Read(Ctrl, t, Ctx))
						{
							NewCtrls.Insert(Ctrl);
						}
						else
						{
							break;
						}
					}
				}

				// calculate the new control set's dimensions so we
				// can paste them in at the top of the container
				ResDialogCtrl *c = NewCtrls.First();
				if (c)
				{
					GRect All = c->View()->GetPos();
					while ((c = NewCtrls.Next()))
					{
						All.Union(&c->View()->GetPos());
					}

					// now paste in the controls
					for (c = NewCtrls.First(); c; c = NewCtrls.Next())
					{
						GRect *Preference = Container->GetPasteArea();
						GRect p = c->View()->GetPos();
						p.Offset(-All.x1, -All.y1);

						p.Offset(Preference ? Preference->x1 : Container->Client.x1 + GRID_X,
								 Preference ? Preference->y1 : Container->Client.y1 + GRID_Y);
						c->SetPos(p);
						Container->AttachCtrl(c, &c->View()->GetPos());
						OnSelect(c, false);
					}

					// reset parent size to fit
					GRect cp = Container->View()->GetPos();
					Container->SetPos(cp, true);
				}

				// Deduplicate all these new strings
				for (ResString *s = NewStrs.First(); s; s = NewStrs.Next())
				{
					s->UnDupelicate();
				}
			}

			// Repaint
			GView::Invalidate();
		}
	}
	DeleteArray(Mem);
}

void ResDialog::SnapPoint(GdcPt2 *p, ResDialogCtrl *From)
{
	ResDialogCtrl *Ctrl = dynamic_cast<ResDialogCtrl*>(Children.First());
	if (p && Ctrl)
	{
		int Ox = 0; // -Ctrl->Client.x1;
		int Oy = 0; // -Ctrl->Client.y1;
		GView *Parent = dynamic_cast<GView*>(Ctrl);
		if (From)
		{
			for (GViewI *w = From->View(); w && w != Parent; w = w->GetParent())
			{
				Ox += w->GetPos().x1;
				Oy += w->GetPos().y1;
			}
		}

		p->x += Ox;
		p->y += Oy;

		p->x = ((p->x + (GRID_X / 2)) / GRID_X * GRID_X);
		p->y = ((p->y + (GRID_X / 2)) / GRID_X * GRID_X);

		p->x -= Ox;
		p->y -= Oy;
	}
}

void ResDialog::SnapRect(GRect *r, ResDialogCtrl *From)
{
	ResDialogCtrl *Ctrl = dynamic_cast<ResDialogCtrl*>(Children.First());
	if (r && Ctrl)
	{
		int Ox = 0; // -Ctrl->Client.x1;
		int Oy = 0; // -Ctrl->Client.y1;
		GView *Parent = dynamic_cast<GView*>(Ctrl);
		for (GViewI *w = From->View(); w && w != Parent; w = w->GetParent())
		{
			Ox += w->GetPos().x1;
			Oy += w->GetPos().y1;
		}

		r->Normal();
		r->Offset(Ox, Oy);

		r->x1 = ((r->x1 + (GRID_X / 2)) / GRID_X * GRID_X);
		r->y1 = ((r->y1 + (GRID_X / 2)) / GRID_X * GRID_X);
		r->x2 = ((r->x2 + (GRID_X / 2)) / GRID_X * GRID_X) - 1;
		r->y2 = ((r->y2 + (GRID_X / 2)) / GRID_X * GRID_X) - 1;

		r->Offset(-Ox, -Oy);
	}
}

bool ResDialog::IsDraging()
{
	return DragGoober >= 0;
}

int ResDialog::CurrentTool()
{
	return (Ui) ? Ui->CurrentTool() : 0;
}

void ResDialog::MoveSelection(int Dx, int Dy)
{
	ResDialogCtrl *w = Selection.First();
	if (w)
	{
		ResDialogCtrl *Parent = w->ParentCtrl();
		if (Parent)
		{
			// find dimensions of group
			GRect All = w->View()->GetPos();
			for (; w; w = Selection.Next())
			{
				All.Union(&w->View()->GetPos());
			}

			// limit the move to the top-left corner of the parent's client
			GRect ParentClient = Parent->Client;
			if (dynamic_cast<CtrlDlg*>(Parent))
				ParentClient.ZOff(-ParentClient.x1, -ParentClient.y1);

			if (All.x1 + Dx < ParentClient.x1)
			{
				Dx = ParentClient.x1 - All.x1;
			}
			if (All.y1 + Dy < ParentClient.y1)
			{
				Dy = ParentClient.y1 - All.y1;
			}

			// move the ctrls
			GRegion Update;
			for (w = Selection.First(); w; w = Selection.Next())
			{
				GRect Old = w->View()->GetPos();
				GRect New = Old;
				New.Offset(Dx, Dy);

				// optionally limit the move to the containers bounds
				GRect *b = w->ParentCtrl()->GetChildArea(w);
				if (b)
				{
					if (New.x1 < b->x1)
					{
						New.Offset(b->x1 - New.x1, 0);
					}
					else if (New.x2 > b->x2)
					{
						New.Offset(b->x2 - New.x2, 0);
					}
					if (New.y1 < b->y1)
					{
						New.Offset(0, b->y1 - New.y1);
					}
					else if (New.y2 > b->y2)
					{
						New.Offset(0, b->y2 - New.y2);
					}
				}

				GRect Up = w->AbsPos();
				Up.Size(-GOOBER_BORDER, -GOOBER_BORDER);
				Update.Union(&Up);

				w->SetPos(New, false);

				Up = w->AbsPos();
				Up.Size(-GOOBER_BORDER, -GOOBER_BORDER);
				Update.Union(&Up);
			}

			Invalidate(&Update);
		}
	}
}

void ResDialog::SelectCtrl(ResDialogCtrl *c)
{

	Selection.Empty();
	if (c)
	{
		bool IsMine = false;
		
		for (GViewI *p = c->View(); p; p = p->GetParent())
		{
			if ((GViewI*)this == p)
			{
				IsMine = true;
				break;
			}
		}
		
		if (IsMine)
		{
			Selection.Insert(c);
			
			// int TabIdx = -1;
			ResDialogCtrl *Prev = 0;
			for (GViewI *p = c->View()->GetParent(); p; p = p->GetParent())
			{
				ResDialogCtrl *c = dynamic_cast<ResDialogCtrl*>(p);
				if (c)
				{
					c->ShowMe(Prev);
					Prev = c;
				}
				else break;
			}
		}
		else
		{
			printf("%s:%i - Ctrl doesn't belong to me.\n", __FILE__, __LINE__);
		}
	}
	else
	{
		printf("Selecting '0'\n");
	}
	
	Invalidate();
	if (AppWindow)
	{
		AppWindow->OnObjSelect(c);
	}
}

void ResDialog::SelectNone()
{
	Selection.Empty();

	Invalidate();
	if (AppWindow)
	{
		AppWindow->OnObjSelect(0);
	}
}

void ResDialog::SelectRect(ResDialogCtrl *Parent, GRect *r, bool ClearPrev)
{
	if (ClearPrev)
	{
		Selection.Empty();
	}

	if (Parent && r)
	{
		GAutoPtr<GViewIterator>It(Parent->View()->IterateViews());
		for (GViewI *c = It->First(); c; c = It->Next())
		{
			if (c->GetPos().Overlap(r))
			{
				ResDialogCtrl *Ctrl = dynamic_cast<ResDialogCtrl*>(c);
				if (Ctrl)
				{
					if (Selection.IndexOf(Ctrl) >= 0)
					{
						Selection.Delete(Ctrl);
					}
					else
					{
						Selection.Insert(Ctrl);
					}
				}
			}
		}
	}

	Invalidate();

	if (AppWindow)
	{
		AppWindow->OnObjSelect((Selection.Length() == 1) ? Selection.First() : 0);
	}
}

bool ResDialog::IsSelected(ResDialogCtrl *Ctrl)
{
	return Selection.IndexOf(Ctrl) >= 0;
}

void ResDialog::OnSelect(ResDialogCtrl *Wnd, bool ClearPrev)
{
	if (ClearPrev)
	{
		Selection.Empty();
	}

	if (Wnd)
	{
		ResDialogCtrl *Ctrl = dynamic_cast<ResDialogCtrl*>(Wnd);
		if (Ctrl)
		{
			if (!Selection.HasItem(Ctrl))
			{
				Selection.Insert(Ctrl);
			}

			if (Ui)
			{
				Ui->SelectTool(UI_CURSOR);
			}

			Invalidate();
		}

		if (AppWindow)
		{
			AppWindow->OnObjSelect((Selection.Length() == 1) ? Selection.First() : 0);
		}
	}
}

void ResDialog::OnDeselect(ResDialogCtrl *Wnd)
{
	if (Selection.HasItem(Wnd))
	{
		Selection.Delete(Wnd);

		AppWindow->OnObjSelect(0);
	}
}

ResDialogCtrl *ResDialog::CreateCtrl(GXmlTag *t)
{
	if (t)
	{
		for (LgiObjectName *o = NameMap; o->Type; o++)
		{
			if (stricmp(t->Tag, o->ResourceName) == 0)
			{
				return CreateCtrl(o->Type, t);
			}
		}
	}

	return 0;
}

ResDialogCtrl *ResDialog::CreateCtrl(int Tool, GXmlTag *load)
{
	ResDialogCtrl *Ctrl = 0;

	switch (Tool)
	{
		case UI_DIALOG:
		{
			Ctrl = new CtrlDlg(this, load);
			break;
		}
		case UI_TABLE:
		{
			Ctrl = new CtrlTable(this, load);
			break;
		}
		case UI_TEXT:
		{
			Ctrl = new CtrlText(this, load);
			if (Ctrl && Ctrl->Str && !load)
			{
				Ctrl->Str->Set("Some text");
			}
			break;
		}
		case UI_EDITBOX:
		{
			Ctrl = new CtrlEditbox(this, load);
			break;
		}
		case UI_CHECKBOX:
		{
			Ctrl = new CtrlCheckbox(this, load);
			if (Ctrl && Ctrl->Str && !load)
			{
				Ctrl->Str->Set("Checkbox");
			}
			break;
		}
		case UI_BUTTON:
		{
			Ctrl = new CtrlButton(this, load);
			if (Ctrl && Ctrl->Str && !load)
			{
				static int i = 1;
				char Text[256];
				sprintf(Text, "Button %i", i++);
				Ctrl->Str->Set(Text);
			}
			break;
		}
		case UI_GROUP:
		{
			Ctrl = new CtrlGroup(this, load);
			if (Ctrl && Ctrl->Str && !load)
			{
				Ctrl->Str->Set("Text");
			}
			break;
		}
		case UI_RADIO:
		{
			Ctrl = new CtrlRadio(this, load);
			if (Ctrl && Ctrl->Str && !load)
			{
				Ctrl->Str->Set("Radio");
			}
			break;
		}
		case UI_TAB:
		{
			Ctrl = new CtrlTab(this, load);
			break;
		}
		case UI_TABS:
		{
			Ctrl = new CtrlTabs(this, load);
			break;
		}
		case UI_LIST:
		{
			Ctrl = new CtrlList(this, load);
			break;
		}
		case UI_COLUMN:
		{
			Ctrl = new ListCol(this, load);
			break;
		}
		case UI_COMBO:
		{
			Ctrl = new CtrlComboBox(this, load);
			break;
		}
		case UI_TREE:
		{
			Ctrl = new CtrlTree(this, load);
			break;
		}
		case UI_BITMAP:
		{
			Ctrl = new CtrlBitmap(this, load);
			break;
		}
		case UI_SCROLL_BAR:
		{
			Ctrl = new CtrlScrollBar(this, load);
			break;
		}
		case UI_PROGRESS:
		{
			Ctrl = new CtrlProgress(this, load);
			break;
		}
		case UI_CUSTOM:
		{
			Ctrl = new CtrlCustom(this, load);
			break;
		}
		case UI_CONTROL_TREE:
		{
			Ctrl = new CtrlControlTree(this, load);
			break;
		}
		default:
		{
			LgiAssert(!"No control factory handler.");
			break;
		}
	}

	if (Ctrl && Ctrl->Str)
	{
		Ctrl->Str->UpdateWnd = Ctrl->View();
	}

	return Ctrl;
}

GView *ResDialog::CreateUI()
{
	return Ui = new ResDialogUi(this);
}

void ResDialog::DrawSelection(GSurface *pDC)
{
	// Draw selection
	for (ResDialogCtrl *Ctrl = Selection.First(); Ctrl; Ctrl = Selection.Next())
	{
		GRect r = Ctrl->AbsPos();
		COLOUR s = Rgb24(255, 0, 0);
		COLOUR c = GetParent()->Focus() ? s : GdcMixColour(s, LC_MED, 0.4);
		DrawGoobers(pDC, r, Ctrl->Goobers, c);
	}
}

void ResDialog::_Paint(GSurface *pDC, int Ox, int Oy)
{
	#ifndef MAC
	GScreenDC DC(this);
	pDC = &DC;
	#endif

	ResDialogCtrl *Ctrl = dynamic_cast<ResDialogCtrl*>(Children.First());
	if (Ctrl)
	{
		GRect c = Ctrl->View()->GetPos();
		c.Size(-GOOBER_BORDER, -GOOBER_BORDER);

		#ifdef MAC
		GSurface *pMemDC = pDC;
		if (pMemDC)
		#else
		GAutoPtr<GSurface> pMemDC(new GMemDC);
		if (pMemDC &&
			pMemDC->Create(c.X(), c.Y(), GdcD->GetBits()))
		#endif
		{
			// Draw client
			pMemDC->Colour(LC_WORKSPACE, 24);
			// pMemDC->Colour(Rgb24(0, 128, 0), 24);
			pMemDC->Rectangle();

			// Paint children
			GRect Pos = Ctrl->View()->GetPos();
			pMemDC->SetClient(&Pos);
			GView::_Paint(pMemDC);
			pMemDC->SetClient(0);
			
			if (GetParent())
			{
				// Draw selection
				DrawSelection(pMemDC);
			}

			#ifndef MAC
			// Put on screen
			pMemDC->SetOrigin(0, 0);
			pDC->Blt(0, 0, pMemDC);

			// Draw other non Mem-DC regions
			pDC->Colour(LC_WORKSPACE, 24);
			if (X() > c.X())
			{
				pDC->Rectangle(c.x2 + 1, 0, X()-1, c.y2);
			}
			if (Y() > c.Y())
			{
				pDC->Rectangle(0, c.y2 + 1, X()-1, Y()-1);
			}
			#endif
		}
		else
		{
			// error
			SysFont->Colour(LC_TEXT, LC_WORKSPACE);
			SysFont->Transparent(false);
			GDisplayString Ds(SysFont, "Can't create memory bitmap.");
			Ds.Draw(pDC, 2, 0, &GetClient());
		}
	}
	else
	{
		pDC->Colour(LC_WORKSPACE, 24);
		pDC->Rectangle();
	}
}

void ResDialog::OnLanguageChange()
{
	if (Ui && Ui->StatusInfo)
	{
		GLanguage *l = Symbols->GetLanguage(App()->GetCurLang()->Id);
		if (l)
		{
			char Str[256];
			sprintf(Str, "Current Language: %s (Id: %s)", l->Name, l->Id);
			Ui->StatusInfo->Name(Str);
		}
	}
}

bool ResDialog::OnKey(GKey &k)
{
	if (k.Ctrl())
	{
		switch (k.c16)
		{
			case VK_UP:
			{
				if (k.Down())
				{
					int Idx = Symbols->GetLangIdx(App()->GetCurLang()->Id);
					if (Idx > 0)
					{
						GLanguage *l = Symbols->GetLanguage(Idx - 1);
						if (l)
						{
							App()->SetCurLang(l);
						}
					}
					Invalidate();
					OnLanguageChange();
				}
				return true;
				break;
			}
			case VK_DOWN:
			{
				if (k.Down())
				{
					int Idx = Symbols->GetLangIdx(App()->GetCurLang()->Id);
					if (Idx < Symbols->GetLanguages() - 1)
					{
						GLanguage *l = Symbols->GetLanguage(Idx + 1);
						if (l)
						{
							App()->SetCurLang(l);
						}
					}
					Invalidate();
					OnLanguageChange();
				}
				return true;
				break;
			}
		}
	}

	return false;
}

void ResDialog::OnMouseClick(GMouse &m)
{
	if (m.Down())
	{	
		if (GetParent()) GetParent()->Focus(true);

		if (m.Left())
		{
			DragGoober = -1;
			for (ResDialogCtrl *c = Selection.First(); c; c = Selection.Next())
			{
				for (int i=0; i<8; i++)
				{
					if (c->Goobers[i].Overlap(m.x, m.y))
					{
						DragGoober = i;
						DragCtrl = c;
						break;
					}
				}
			}

			if (DragGoober >= 0)
			{
				DragRgn = DragCtrl->View()->GetPos();
				DragX = DragY = 0;
				int Cap = GooberCaps[DragGoober];
				
				// Lock the dialog to the top left corner
				if (dynamic_cast<CtrlDlg*>(DragCtrl))
				{
					Cap &= ~(RESIZE_X1|RESIZE_Y1);
				}
				
				if (!Cap)
				{
					// Can't move the object anyway so abort the drag
					DragGoober = -1;
				}
				else
				{
					// Allow movement along the appropriate edge
					if (Cap & RESIZE_X1)
					{
						DragX = &DragRgn.x1;
					}
					if (Cap & RESIZE_Y1)
					{
						DragY = &DragRgn.y1;
					}
					if (Cap & RESIZE_X2)
					{
						DragX = &DragRgn.x2;
					}
					if (Cap & RESIZE_Y2)
					{
						DragY = &DragRgn.y2;
					}

					// Remember the offset to the mouse from the egdes
					if (DragX) DragOx = m.x - *DragX;
					if (DragY) DragOy = m.y - *DragY;

					Capture(true);
				}
			}
		}
	}
	else
	{
		if (DragGoober >= 0)
		{
			Capture(false);
			DragGoober = -1;
			DragX = 0;
			DragY = 0;
		}
	}
}

void ResDialog::OnMouseMove(GMouse &m)
{
	if (DragGoober >= 0)
	{
		if (DragX)
		{
			*DragX = m.x - DragOx;
		}
		if (DragY)
		{
			*DragY = m.y - DragOy;
		}

		// DragRgn in real coords
		GRect Old = DragCtrl->View()->GetPos();
		GRect New = DragRgn;
		
		GAutoPtr<GViewIterator> It(IterateViews());
		if (DragCtrl->View() != It->First())
		{
			SnapRect(&New, DragCtrl->ParentCtrl());
		}

		// New now in snapped coords
		
		// If that means the dragging control changes then
		if (New != Old)
		{
			GRegion Update;

			// change everyone else by the same amount
			for (ResDialogCtrl *c = Selection.First(); c; c = Selection.Next())
			{
				GRect OldPos = c->View()->GetPos();
				GRect NewPos = OldPos;

				NewPos.x1 += New.x1 - Old.x1;
				NewPos.y1 += New.y1 - Old.y1;
				NewPos.x2 += New.x2 - Old.x2;
				NewPos.y2 += New.y2 - Old.y2;

				GRect Up = c->AbsPos();
				Up.Size(-GOOBER_BORDER, -GOOBER_BORDER);
				Update.Union(&Up);
				
				c->SetPos(NewPos);

				Up = c->AbsPos();
				Up.Size(-GOOBER_BORDER, -GOOBER_BORDER);
				Update.Union(&Up);
			}

			Invalidate(&Update);
		}
	}
}

bool ResDialog::Test(ErrorCollection *e)
{
	return true;
}

bool ResDialog::Read(GXmlTag *t, ResFileFormat Format)
{
	bool Status = false;

	// turn symbol creation off so that ctrls find their
	// symbol via Id matching instead of creating their own
	CreateSymbols = false;

	ResDialogCtrl *Dlg = dynamic_cast<ResDialogCtrl*>(Children.First());
	if (Dlg)
	{
		// Load the resource
		ResReadCtx Ctx;
		Status = Res_Read(Dlg, t, Ctx);
		Item->Update();
	}

	CreateSymbols = true;
	return Status;
}

void ResDialog::CleanSymbols()
{
	// removed unreferenced dialog strings
	if (Symbols)
	{
		Symbols->RemoveUnReferenced();
	}

	// re-ID duplicate entries
	ResDialogCtrl *Ctrl = dynamic_cast<ResDialogCtrl*>(Children.First());
	if (Ctrl)
	{
		// list all the entries
		List<ResDialogCtrl> l;
		Ctrl->ListChildren(l);
		l.Insert(Ctrl); // insert the dialog too

		// remove duplicate string entries
		for (ResDialogCtrl *c = l.First(); c; c = l.Next())
		{
			LgiAssert(c->Str);
			c->Str->UnDupelicate();
		}
	}

	// sort list (cause I need to read the file myself)
	if (Symbols)
	{
		Symbols->Sort();
	}
}

bool ResDialog::Write(GXmlTag *t, ResFileFormat Format)
{
	bool Status = false;
	ResDialogCtrl *Ctrl = dynamic_cast<ResDialogCtrl*>(Children.First());
	if (Ctrl)
	{
		// duplicates symbols should have been removed before the 
		// strings were written out
		// so we don't have to do it here.

		// write the resources out
		if (Ctrl)
		{
			Status = Res_Write(Ctrl, t);
		}
	}

	return Status;
}

void ResDialog::OnRightClick(GSubMenu *RClick)
{
	if (RClick)
	{
		if (Enabled())
		{
			RClick->AppendSeparator();
			if (Type() > 0)
			{
				RClick->AppendItem("Dump to C++", IDM_DUMP, true);

				GSubMenu *Export = RClick->AppendSub("Export to...");
				if (Export)
				{
					Export->AppendItem("Lgi File", IDM_EXPORT, true);
					Export->AppendItem("Win32 Resource Script", IDM_EXPORT_WIN32, false);
				}
			}
		}
	}
}

const char *TypeOfRes(ResDialogCtrl *Ctrl)
{

	// the default
	return "GWindow";
}

const char *TextOfCtrl(ResDialogCtrl *Ctrl)
{
	static char Buf[256];

	switch (Ctrl->GetType())
	{
		// Has text
		case UI_TEXT:
		case UI_EDITBOX:
		case UI_CHECKBOX:
		case UI_BUTTON:
		case UI_GROUP:
		case UI_RADIO:
		case UI_COMBO:
		{
			char *s = Ctrl->Str->Get();
			sprintf(Buf, ", \"%s\"", s?s:"");
			return Buf;
		}

		// Not processed
		case UI_COLUMN:
		case UI_TAB:
		{
			LgiAssert(0);
			break;
		}

		// No text...
		case UI_BITMAP:
		case UI_PROGRESS:
		case UI_TABS:
		case UI_LIST:
		case UI_TREE:
		case UI_SCROLL_BAR:
		{
			break;
		}
	}

	return "";
}

void OutputCtrl(GStringPipe &Def,
				GStringPipe &Var,
				GStringPipe &Inst,
				ResDialogCtrl *Ctrl,
				ResDialogCtrl *Parent,
				int &Index)
{
	char Str[256];
	const char *Type = "GView";

	for (LgiObjectName *on=NameMap; on->Type; on++)
	{
		if (Ctrl->GetType() == on->Type)
		{
			Type = on->ObjectName;
			break;
		}
	}

	if (stricmp(Type, "GDialog"))
	{
		if (ValidStr(Ctrl->Str->GetDefine()) &&
			stricmp(Ctrl->Str->GetDefine(), "IDOK") &&
			stricmp(Ctrl->Str->GetDefine(), "IDCANCEL") &&
			stricmp(Ctrl->Str->GetDefine(), "IDC_STATIC"))
		{
			char Tab[8];
			int Tabs = (32 - strlen(Ctrl->Str->GetDefine()) - 1) / 4;
			memset(Tab, '\t', Tabs);
			Tab[Tabs] = 0;

			sprintf(Str, "#define %s%s%i\n", Ctrl->Str->GetDefine(), Tab, 1000 + Index);
			Def.Push(Str);
		}

		sprintf(Str,
				"\t%s *Ctrl%i;\n",
				Type,
				Index);
		Var.Push(Str);

		sprintf(Str,
				"\tChildren.Insert(Ctrl%i = new %s(%s, %i, %i, %i, %i%s));\n",
				Index,
				Type,
				Ctrl->Str->GetDefine(),
				Ctrl->View()->GetPos().x1 - 3,
				Ctrl->View()->GetPos().y1 - 17,
				Ctrl->View()->X(),
				Ctrl->View()->Y(),
				TextOfCtrl(Ctrl));
		Inst.Push(Str);

		CtrlList *List = dynamic_cast<CtrlList*>(Ctrl);
		if (List)
		{
			// output columns
			for (ListCol *c=List->Cols.First(); c; c=List->Cols.Next())
			{
				sprintf(Str, "\tCtrl%i->AddColumn(\"%s\", %i);\n", Index, c->Str->Get(), c->X());
				Inst.Push(Str);
			}
		}

		Index++;
	}

	GAutoPtr<GViewIterator> It(Ctrl->View()->IterateViews());
	for (	ResDialogCtrl *c=dynamic_cast<ResDialogCtrl*>(It->First());
			c; c=dynamic_cast<ResDialogCtrl*>(It->Next()))
	{
		OutputCtrl(Def, Var, Inst, c, Ctrl, Index);
	}
}

void ResDialog::OnCommand(int Cmd)
{
	switch (Cmd)
	{
		case IDM_DUMP:
		{
			GStringPipe Def, Var, Inst;
			GStringPipe Buf;
			char Str[256];

			ResDialogCtrl *Dlg = dynamic_cast<ResDialogCtrl*>(Children.First());
			if (Dlg)
			{
				// List controls
				int i=0;
				OutputCtrl(Def, Var, Inst, Dlg, 0, i);

				// #define's
				char *Defs = Def.NewStr();
				if (Defs)
				{
					Buf.Push(Defs);
					Def.Empty();
				}

				// Class def
				Buf.Push(	"\nclass Dlg : public GDialog\n"
							"{\n");

				// Variables
				char *Vars = Var.NewStr();
				if (Vars)
				{
					Buf.Push(Vars);
					Var.Empty();
				}

				// Member functions
				Buf.Push(	"\n"
							"public:\n"
							"\tDlg(GView *Parent);\n"
							"\t~Dlg();\n"
							"\n"
							"\tint OnNotify(GViewI *Ctrl, int Flags);\n"
							"};\n"
							"\n");

				// Class impl
				Buf.Push(	"Dlg::Dlg(GView *Parent)\n"
							"{\n"
							"\tSetParent(Parent);\n");

				sprintf(Str,
						"\tName(\"%s\");\n"
						"\tGRegion r(0, 0, %i, %i);\n"
						"\tSetPos(r);\n",
						Dlg->Str->Get(),
						Dlg->View()->X(),
						Dlg->View()->Y());
				Buf.Push(Str);
				Buf.Push("\tMoveToCenter();\n\n");

				// Ctrl instancing
				char *NewCtrls = Inst.NewStr();
				if (NewCtrls)
				{
					Buf.Push(NewCtrls);
					Inst.Empty();
				}

				Buf.Push(	"}\n"
							"\n");

				// Destructor
				Buf.Push(	"Dlg::~Dlg()\n"
							"{\n"
							"}\n"
							"\n");

				// ::OnNotify
				Buf.Push(	"int Dlg::OnNotify(GViewI *Ctrl, int Flags)\n"
							"{\n"
							"\tswitch (Ctrl->GetId())\n"
							"\t{\n"
							"\t\tcase IDOK:\n"
							"\t\t{\n"
							"\t\t\t// Do something here\n"
							"\t\t\t// fall thru\n"
							"\t\t}\n"
							"\t\tcase IDCANCEL:\n"
							"\t\t{\n"
							"\t\t\tEndModal(Ctrl->GetId());\n"
							"\t\t\tbreak;\n"
							"\t\t}\n"
							"\t}\n"
							"\n"
							"\treturn 0;\n"
							"}\n");


				// Output to clipboard
				char *Text = Buf.NewStr();
				if (Text)
				{
					GClipBoard Clip(Ui);
					Clip.Text(Text);
					DeleteArray(Text);
				}
			}
			break;
		}
		case IDM_EXPORT:
		{
			GFileSelect Select;
			Select.Parent(AppWindow);
			Select.Type("Text", "*.txt");
			if (Select.Save())
			{
				GFile F;
				if (F.Open(Select.Name(), O_WRITE))
				{
					F.SetSize(0);
					// Serialize(F, true);
				}
				else
				{
					LgiMsg(AppWindow, "Couldn't open file for writing.");
				}
			}
			break;
		}
		case IDM_EXPORT_WIN32:
		{
			break;
		}
	}
}

int ResDialog::OnCommand(int Cmd, int Event, OsView hWnd)
{
	switch (Cmd)
	{
		/*
		case IDM_SET_LANG:
		{
			Symbols->SetCurrent();
			OnSelect(Selection.First());

			Invalidate();
			OnLanguageChange();
			break;
		}
		*/
		case IDM_TAB_ORDER:
		{
			ResDialogCtrl *Top = 0;
			if (Selection.Length() == 1 &&
				Selection.First()->IsContainer())
			{
				Top = Selection.First();
			}
			if (!Top)
			{
				Top = dynamic_cast<ResDialogCtrl*>(Children.First());
			}
			if (Top)
			{
				TabOrder Dlg(this, Top);
			}
			break;
		}
	}

	return 0;
}

ResString *ResDialog::CreateSymbol()
{
	return (Symbols) ? Symbols->CreateStr() : 0;
}

////////////////////////////////////////////////////////////////////
ResDialogUi::ResDialogUi(ResDialog *Res)
{
	Dialog = Res;
	Tools = 0;
	Status = 0;
	StatusInfo = 0;
	Name("ResDialogUi");

	if (Res)
	{
		Res->OnSelect(Res->Selection.First());
	}
}

ResDialogUi::~ResDialogUi()
{
	if (Dialog)
	{
		Dialog->Ui = 0;
	}
}

void ResDialogUi::OnPaint(GSurface *pDC)
{
	GRegion Client(0, 0, X()-1, Y()-1);
	for (GViewI *w = Children.First(); w; w = Children.Next())
	{
		GRect r = w->GetPos();
		Client.Subtract(&r);
	}

	pDC->Colour(LC_MED, 24);
	for (GRect *r = Client.First(); r; r = Client.Next())
	{
		pDC->Rectangle(r);
	}
}

void ResDialogUi::Pour()
{
	GRegion Client(GetClient());
	GRegion Update;

	for (GViewI *v = Children.First(); v; v = Children.Next())
	{
		GRect OldPos = v->GetPos();
		Update.Union(&OldPos);

		if (v->Pour(Client))
		{
			if (!v->Visible())
			{
				v->Visible(true);
			}

			if (OldPos != v->GetPos())
			{
				// position has changed update...
				v->Invalidate();
			}

			Client.Subtract(&v->GetPos());
			Update.Subtract(&v->GetPos());
		}
		else
		{
			// make the view not visible
			v->Visible(false);
		}
	}

	for (int i=0; i<Update.Length(); i++)
	{
		Invalidate(Update[i]);
	}
}

void ResDialogUi::OnPosChange()
{
	Pour();
}

void ResDialogUi::OnCreate()
{
	Tools = new GToolBar;
	if (Tools)
	{
		char *FileName = LgiFindFile("_DialogIcons.gif");
		if (FileName && Tools->SetBitmap(FileName, 16, 16))
		{
			Tools->Attach(this);

			Tools->AppendButton("Cursor", 0, TBT_RADIO);

			for (LgiObjectName *o=NameMap; o->Type; o++)
			{
				if (o->ToolbarBtn)
				{
					Tools->AppendButton(o->ObjectName, o->Type, TBT_RADIO);
				}
			}
			Tools->AppendSeparator();

			// Tools->AppendButton("Change language", IDM_SET_LANG, TBT_PUSH);
			Tools->AppendButton("Tab Order", IDM_TAB_ORDER, TBT_PUSH, true, 17);
		}
		else
		{
			DeleteObj(Tools);
		}
		DeleteArray(FileName);
	}

	Status = new GStatusBar;
	if (Status)
	{
		Status->Attach(this);
		StatusInfo = Status->AppendPane("", 2);
	}

	ResFrame *Frame = new ResFrame(Dialog);
	if (Frame)
	{
		Frame->Attach(this);
	}
	
	Pour();
}

int ResDialogUi::CurrentTool()
{
	if (Tools)
	{
		int i=0;
		GAutoPtr<GViewIterator> It(Tools->IterateViews());
		for (GViewI *w = It->First(); w; w = It->Next(), i++)
		{
			GToolButton *But = dynamic_cast<GToolButton*>(w);
			if (But)
			{
				if (But->Value())
				{
					return But->GetId();
				}
			}
		}
	}

	return -1;
}

void ResDialogUi::SelectTool(int i)
{
	if (Tools)
	{
		GAutoPtr<GViewIterator> It(Tools->IterateViews());
		GViewI *w = (*It)[i];
		if (w)
		{
			GToolButton *But = dynamic_cast<GToolButton*>(w);
			if (But)
			{
				But->Value(true);
			}
		}
	}
}

GMessage::Result ResDialogUi::OnEvent(GMessage *Msg)
{
	switch (MsgCode(Msg))
	{
		case M_COMMAND:
		{
			Dialog->OnCommand(MsgA(Msg)&0xffff, MsgA(Msg)>>16, (OsView) MsgB(Msg));
			break;
		}
		case M_DESCRIBE:
		{
			char *Text = (char*) MsgB(Msg);
			if (Text)
			{
				StatusInfo->Name(Text);
			}
			break;
		}
	}

	return GView::OnEvent(Msg);
}
