/*hdr
**      FILE:           GFindRepalce.cpp
**      AUTHOR:         Matthew Allen
**      DATE:           14/11/2001
**      DESCRIPTION:    Common find and replace windows
**
**      Copyright (C) 2001, Matthew Allen
**              fret@memecode.com
*/

#include <stdio.h>
#include "Lgi.h"
#include "GTextView3.h"
#include "GTextLabel.h"
#include "GEdit.h"
#include "GCheckBox.h"
#include "GButton.h"
#include "GTableLayout.h"
#include "LgiRes.h"

////////////////////////////////////////////////////////////////////////////
GFindReplaceCommon::GFindReplaceCommon()
{
	MatchWord = false;
	MatchCase = false;
	SelectionOnly = false;
}

////////////////////////////////////////////////////////////////////////////
// Find Window
#define IDS_16						1000
#define IDC_TEXT					1001
#define IDC_MATCH_WORD				1004
#define IDC_MATCH_CASE				1005
#define IDC_PREV_SEARCH				1006
#define IDC_SELECTION_ONLY			1007
#define IDC_FIND_TABLE              1008
#define IDC_SEARCH_UP				1009

class GFindDlgPrivate
{
public:
	GEdit *Edit;
	GFrCallback Callback;
	void *CallbackData;
};

GFindDlg::GFindDlg(GView *Parent, char *Init, GFrCallback Callback, void *UserData)
{
	d = new GFindDlgPrivate;
	if (Init)
		Find = Init;
	d->Callback = Callback;
	d->CallbackData = UserData;

	SetParent(Parent);
	Name(LgiLoadString(L_FR_FIND, "Find"));

	GRect r(0, 0, 450, 370);
	SetPos(r);
	MoveSameScreen(Parent);

    GTableLayout *t;
    if (AddView(t = new GTableLayout(IDC_FIND_TABLE)))
    {        
        int Row = 0;
        GLayoutCell *c = t->GetCell(0, Row);
        c->Add(new GText(IDS_16, 14, 14, -1, -1, LgiLoadString(L_FR_FIND_WHAT, "Find what:")));
        
        c = t->GetCell(1, Row);
    	c->Add(d->Edit = new GEdit(IDC_TEXT, 91, 7, 168, 21, ""));
    	
    	c = t->GetCell(2, Row++);
    	c->Add(new GButton(IDOK, 294, 7, 70, 21, LgiLoadString(L_FR_FIND_NEXT, "Find Next")));
    	
    	c = t->GetCell(0, Row, true, 2, 1);
    	c->Add(new GCheckBox(IDC_MATCH_WORD, 14, 42, -1, -1, LgiLoadString(L_FR_MATCH_WORD, "Match &whole word only")));
    	
    	c = t->GetCell(2, Row++);
	    c->Add(new GButton(IDCANCEL, 294, 35, 70, 21, LgiLoadString(L_BTN_CANCEL, "Cancel")));
	    
	    c = t->GetCell(0, Row++, true, 2, 1);
	    c->Add(new GCheckBox(IDC_MATCH_CASE, 14, 63, -1, -1, LgiLoadString(L_FR_MATCH_CASE, "Match &case")));
	    
	    c = t->GetCell(0, Row);
	    c->Add(new GCheckBox(IDC_SELECTION_ONLY, 14, 84, -1, -1, LgiLoadString(L_FR_SELECTION_ONLY, "&Selection only")));

	    c = t->GetCell(1, Row++);
	    c->Add(new GCheckBox(IDC_SEARCH_UP, 0, 0, -1, -1, "Search &upwards"));
	    
	    OnPosChange();
	    
	    GRect TblPos = t->GetPos();
		int TblY = TblPos.Y();
		GRect Used = t->GetUsedArea();
		int UsedY = Used.Y();
	    
		int Over = TblY - UsedY;
	    r = GetPos();
        r.y2 -= Over - (GTableLayout::CellSpacing<<1);
	    SetPos(r);
	}	
    
	if (d->Edit)
		d->Edit->Focus(true);

	RegisterHook(this, GKeyEvents);
}

GFindDlg::~GFindDlg()
{
	DeleteObj(d);
}

bool GFindReplaceCommon::OnViewKey(GView *v, GKey &k)
{
	if (k.Down() &&
		k.Alt())
	{
		GViewI *Tbl;
		if (GetViewById(IDC_FIND_TABLE, Tbl))
		{
			GAutoPtr<GViewIterator> Ch(Tbl->IterateViews());
			for (GViewI *c = Ch->First(); c; c = Ch->Next())
			{
				char *n = c->Name(), *amp;
				if (!n || !(amp = strchr(n, '&')))
					continue;

				if (ToLower(amp[1]) != ToLower(k.c16))
					continue;

				if (dynamic_cast<GCheckBox*>(c))
					c->Value(!c->Value());
				else if (dynamic_cast<GText*>(c))
				{
					GEdit *e = dynamic_cast<GEdit*>(Ch->Next());
					if (e)
						e->Focus(true);
				}
				else
					c->Value(1);
				return true;
			}
		}
	}

	return false;
}

void GFindDlg::OnPosChange()
{
    GTableLayout *t;
    if (GetViewById(IDC_FIND_TABLE, t))
    {
        GRect c = GetClient();
        c.Size(GTableLayout::CellSpacing, GTableLayout::CellSpacing);
        t->SetPos(c);
    }
}

void GFindDlg::OnCreate()
{
	// Load controls
	if (Find)
		SetCtrlName(IDC_TEXT, Find);
	SetCtrlValue(IDC_MATCH_WORD, MatchWord);
	SetCtrlValue(IDC_MATCH_CASE, MatchCase);
	
	if (d->Edit)
	{
		d->Edit->Select(0);
		d->Edit->Focus(true);
	}
}

int GFindDlg::OnNotify(GViewI *Ctrl, int Flags)
{
	switch (Ctrl->GetId())
	{
		case IDOK:
		{
			// Save controls
			Find = GetCtrlName(IDC_TEXT);
			MatchWord = GetCtrlValue(IDC_MATCH_WORD);
			MatchCase = GetCtrlValue(IDC_MATCH_CASE);
			
			// printf("%s:%i Find OnNot %s, %i, %i\n", _FL, Find, MatchWord, MatchCase);

			if (d->Callback)
			{
				d->Callback(this, false, d->CallbackData);
				break;
			}
			// else fall thru
		}
		case IDCANCEL:
		{
			if (IsModal())
				EndModal(Ctrl->GetId());
			else
				EndModeless();
			break;
		}
	}

	return 0;
}

////////////////////////////////////////////////////////////////////////////
// Replace Window

#define IDS_33						1009
#define IDC_REPLACE_WITH			1010
#define IDC_PREV_REPLACE			1011

class GReplaceDlgPrivate
{
public:
	GFrCallback Callback;
	void *CallbackData;
};

GReplaceDlg::GReplaceDlg(GView *Parent, char *InitFind, char *InitReplace, GFrCallback Callback, void *UserData)
{
	d = new GReplaceDlgPrivate;
	d->Callback = Callback;
	d->CallbackData = UserData;
	if (InitFind)
		Find = InitFind;
	Replace = NewStr(InitReplace);
	MatchWord = false;
	MatchCase = false;

	SetParent(Parent);
	Name(LgiLoadString(L_FR_REPLACE, "Replace"));

	GView *f = 0;
	
	GRect r(0, 0, 450, 300);
	SetPos(r);
	MoveToCenter();
	
	GTableLayout *t;
	if (AddView(t = new GTableLayout(IDC_FIND_TABLE)))
	{
	    int Row = 0;
        GLayoutCell *c = t->GetCell(0, Row);
    	c->Add(new GText(-1, 14, 14, -1, -1, LgiLoadString(L_FR_FIND_WHAT, "Find what:")));

        c = t->GetCell(1, Row);
    	c->Add(f = new GEdit(IDC_TEXT, 0, 0, 60, 20, ""));

        c = t->GetCell(2, Row++);
    	c->Add(new GButton(IDC_FR_FIND, 0, 0, -1, -1, LgiLoadString(L_FR_FIND_NEXT, "Find Next")));
	
        c = t->GetCell(0, Row);	
	    c->Add(new GText(-1, 0, 0, -1, -1, LgiLoadString(L_FR_REPLACE_WITH, "Replace with:")));

        c = t->GetCell(1, Row);	
    	c->Add(new GEdit(IDC_REPLACE_WITH, 0, 0, -1, -1, ""));

        c = t->GetCell(2, Row++);	
    	c->Add(new GButton(IDC_FR_REPLACE, 0, 0, -1, -1, LgiLoadString(L_FR_REPLACE, "Replace")));

        c = t->GetCell(0, Row, true, 2);
    	c->Add(new GCheckBox(IDC_MATCH_WORD, 14, 70, -1, -1, LgiLoadString(L_FR_MATCH_WORD, "Match whole &word only")));

        c = t->GetCell(2, Row++);	
    	c->Add(new GButton(IDOK, 0, 0, -1, -1, LgiLoadString(L_FR_REPLACE_ALL, "Replace All")));

        c = t->GetCell(0, Row, true, 2);
    	c->Add(new GCheckBox(IDC_MATCH_CASE, 14, 91, -1, -1, LgiLoadString(L_FR_MATCH_CASE, "Match &case")));

        c = t->GetCell(2, Row++);	
    	c->Add(new GButton(IDCANCEL, 0, 0, -1, -1, LgiLoadString(L_BTN_CANCEL, "Cancel")));

        c = t->GetCell(0, Row);
	    c->Add(new GCheckBox(IDC_SELECTION_ONLY, 14, 112, -1, -1, LgiLoadString(L_FR_SELECTION_ONLY, "&Selection only")));
        c = t->GetCell(1, Row);
    	c->Add(new GCheckBox(IDC_SEARCH_UP, 14, 91, -1, -1, "Search &upwards"));

	
	    OnPosChange();
	    
	    GRect u = t->GetUsedArea();
	    int Over = t->GetPos().Y() - u.Y();
	    r = GetPos();
        r.y2 -= Over - (GTableLayout::CellSpacing << 1);
	    SetPos(r);
	}
	
	if (f) f->Focus(true);

	RegisterHook(this, GKeyEvents);
}

GReplaceDlg::~GReplaceDlg()
{
	DeleteObj(d);
}

void GReplaceDlg::OnCreate()
{
	if (Find)
		SetCtrlName(IDC_TEXT, Find);
	if (Replace)
		SetCtrlName(IDC_REPLACE_WITH, Replace);
	SetCtrlValue(IDC_MATCH_WORD, MatchWord);
	SetCtrlValue(IDC_MATCH_CASE, MatchCase);
	SetCtrlValue(IDC_SELECTION_ONLY, SelectionOnly);
	SetCtrlValue(IDC_SEARCH_UP, SearchUpwards);
}

int GReplaceDlg::OnNotify(GViewI *Ctrl, int Flags)
{
	switch (Ctrl->GetId())
	{
		case IDOK:
		case IDC_FR_FIND:
		case IDC_FR_REPLACE:
		{
			Find = GetCtrlName(IDC_TEXT);
			Replace = GetCtrlName(IDC_REPLACE_WITH);
			MatchWord = GetCtrlValue(IDC_MATCH_WORD);
			MatchCase = GetCtrlValue(IDC_MATCH_CASE);
			SelectionOnly = GetCtrlValue(IDC_SELECTION_ONLY);
			SearchUpwards = GetCtrlValue(IDC_SEARCH_UP);

			if (d->Callback)
			{
				d->Callback(this, Ctrl->GetId() == IDC_FR_REPLACE, d->CallbackData);
				break;
			}
			// else fall thru
		}
		case IDCANCEL:
		{
			EndModal(Ctrl->GetId());
			break;
		}
	}

	return 0;
}

void GReplaceDlg::OnPosChange()
{
    GTableLayout *t;
    if (GetViewById(IDC_FIND_TABLE, t))
    {
        GRect c = GetClient();
        c.Size(GTableLayout::CellSpacing, GTableLayout::CellSpacing);
        t->SetPos(c);
    }
}

