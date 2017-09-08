/*hdr
**      FILE:           GWidgets.cpp
**      AUTHOR:         Matthew Allen
**      DATE:           8/9/1998
**      DESCRIPTION:    Dialog components
**
**      Copyright (C) 1998-2001 Matthew Allen
**              fret@memecode.com
*/

#include <stdlib.h>
#include <stdio.h>
#include "Lgi.h"
#include <commctrl.h>
#include "GTableLayout.h"
#include "GDisplayString.h"

#define USE_DIALOGBOXINDIRECTPARAM         0

struct GDialogPriv
{
	bool IsModal, _Resizable;
	int ModalStatus;

	#if WINNATIVE
    #if USE_DIALOGBOXINDIRECTPARAM
	GMem *Mem;
    #else
    int ModalResult;
	#endif
	#elif defined BEOS

	sem_id ModalSem;
	int ModalRet;

	#endif

    GDialogPriv()
    {
        #if USE_DIALOGBOXINDIRECTPARAM
    	Mem = 0;
    	#else
    	ModalResult = -1;
    	#endif
    	IsModal = true;
    }
    
    ~GDialogPriv()
    {
    	#if USE_DIALOGBOXINDIRECTPARAM
        DeleteObj(Mem);
        #endif
    }
};

///////////////////////////////////////////////////////////////////////////////////////////
#if USE_DIALOGBOXINDIRECTPARAM
short *DlgStrCopy(short *A, char *N)
{
	uchar *n = (uchar*) N;
	if (n)
	{
		while (*n)
		{
			*A++ = *n++;
		}
	}
	*A++ = 0;
	return A;
}

short *DlgStrCopy(short *A, char16 *n)
{
	if (n)
	{
		while (*n)
		{
			*A++ = *n++;
		}
	}
	*A++ = 0;
	return A;
}

short *DlgPadToDWord(short *A, bool Seek = false)
{
	char *c = (char*) A;
	while (((NativeInt) c) & 3)
	{
		if (Seek)
		{
			c++;
		}
		else
		{
			*c++ = 0;
		}
	}
	return (short*) c;
}
#endif

///////////////////////////////////////////////////////////////////////////////////////////
GDialog::GDialog()
	: ResObject(Res_Dialog)
{
	d = new GDialogPriv;
	_Window = this;
	Name("Dialog");

	#if USE_DIALOGBOXINDIRECTPARAM
	WndFlags |= GWF_DIALOG;
	SetExStyle(GetExStyle() | WS_EX_CONTROLPARENT);
	#else
	SetStyle(GetStyle() & ~(WS_MINIMIZEBOX | WS_MAXIMIZEBOX));
	SetStyle(GetStyle() | WS_DLGFRAME);
	#endif
}

GDialog::~GDialog()
{
    DeleteObj(d);
}

bool GDialog::LoadFromResource(int Resource, char *TagList)
{
	GAutoString n;
	bool Status = GLgiRes::LoadFromResource(Resource, this, &Pos, &n, TagList);
	if (Status && n)
		Name(n);
	return Status;
}

LRESULT CALLBACK DlgRedir(HWND hWnd, UINT m, WPARAM a, LPARAM b)
{
	if (m == WM_INITDIALOG)
	{
		GDialog *NewWnd = (GDialog*) b;
		NewWnd->_View = hWnd;
		#if _MSC_VER >= _MSC_VER_VS2005
        SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)(GViewI*)NewWnd);
		#else
        SetWindowLong(hWnd, GWL_USERDATA, (LONG)(GViewI*)NewWnd);
		#endif
	}

	GViewI *Wnd = (GViewI*)
		#if _MSC_VER >= _MSC_VER_VS2005
		#pragma warning(disable : 4312)
		GetWindowLongPtr(hWnd, GWLP_USERDATA);
		#pragma warning(default : 4312)
		#else
		GetWindowLong(hWnd, GWL_USERDATA);
		#endif
	if (Wnd)
	{
		GMessage Msg(m, a, b);
		return Wnd->OnEvent(&Msg);
	}

	return 0;
}

bool GDialog::OnRequestClose(bool OsClose)
{
	return true;
}

bool GDialog::IsModal()
{
	return d->IsModal;
}

int GDialog::DoModal(OsView ParentHnd)
{
	int Status = -1;

	d->IsModal = true;
	
	#if USE_DIALOGBOXINDIRECTPARAM
	
	d->Mem = new GMem(16<<10);
	if (d->Mem)
	{
		DLGTEMPLATE *Template = (DLGTEMPLATE*) d->Mem->Lock();
		if (Template)
		{
			GRect r = GetPos();

			r.x1 = (double)r.x1 / DIALOG_X;
			r.y1 = (double)r.y1 / DIALOG_Y;
			r.x2 = (double)r.x2 / DIALOG_X;
			r.y2 = (double)r.y2 / DIALOG_Y;

			Template->style =	// DS_ABSALIGN |
								DS_SETFONT |
								DS_NOFAILCREATE |
								DS_3DLOOK |
								WS_SYSMENU |
								WS_CAPTION |
								DS_SETFOREGROUND |
								DS_MODALFRAME;

			Template->dwExtendedStyle = WS_EX_DLGMODALFRAME;
			Template->cdit = 0;
			Template->x = r.x1;
			Template->y = r.y1;
			Template->cx = r.X();
			Template->cy = r.Y();

			short *A = (short*) (Template+1);
			// menu
			*A++ = 0;
			// class
			*A++ = 0;
			// title
			A = DlgStrCopy(A, NameW());
			// font
			*A++ = SysFont->PointSize();
			A = DlgStrCopy(A, SysFont->Face());
			A = DlgPadToDWord(A);

			GViewI *p = GetParent();
			while (	p &&
					!p->Handle() &&
					p->GetParent())
			{
				p = p->GetParent();
			}

			HWND hWindow = 0;
			if (ParentHnd)
			{
				hWindow = ParentHnd;
			}
			else if (p)
			{
				hWindow = p->Handle();
			}

			Status = DialogBoxIndirectParam(LgiProcessInst(),
											Template,
											hWindow,
											(DLGPROC) DlgRedir, 
											(LPARAM) this);
		}

		DeleteObj(d->Mem);
	}
	
	#else

    GViewI *p = GetParent();
    if (p && p->GetWindow() != p)
        p = p->GetWindow();
    
	if (Attach(0))
	{
	    AttachChildren();
	    
	    GWindow *ParentWnd = dynamic_cast<GWindow*>(p);
	    if (ParentWnd)
	        ParentWnd->_Dialog = this;

	    if (p)
	    {
	        GRect pp = p->GetPos();
	        int cx = pp.x1 + (pp.X() >> 1);
	        int cy = pp.y1 + (pp.Y() >> 1);
	        
	        GRect np = GetPos();
	        np.Offset(  cx - (np.X() >> 1) - np.x1,
	                    cy - (np.Y() >> 1) - np.y1);
	        
	        SetPos(np);
	        MoveOnScreen();
	    }
	    
	    Visible(true);
	    
	    OsView pwnd = ParentHnd ? ParentHnd : (p ? p->Handle() : NULL);
	    if (pwnd) EnableWindow(pwnd, false);
	    while (d->ModalResult < 0)
	    {
	        LgiYield();
	        LgiSleep(20);
	    }
	    if (pwnd) EnableWindow(pwnd, true);
	    if (ParentWnd)
	        ParentWnd->_Dialog = NULL;
	    
	    Visible(false);
	    Status = d->ModalResult;
	}
	else
	{
	    LgiAssert(!"Attach failed.");
	}
	
	#endif

	return Status;
}

static char *BaseStr = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

int GDialog::DoModeless()
{
	int Status = -1;

	LgiAssert(!_View);
	if (_View)
		return Status;

	#if USE_DIALOGBOXINDIRECTPARAM
	d->Mem = new GMem(16<<10);
	d->IsModal = false;
	GRect OldPos = GetPos();
	if (d->Mem)
	{
		LONG DlgUnits = GetDialogBaseUnits();
		uint16 old_baseunitX = DlgUnits & 0xffff;
		uint16 old_baseunitY = DlgUnits >> 16;
		GDisplayString Bs(SysFont, BaseStr);
		int baseunitX = (Bs.X() / 26 + 1) / 2;
		int baseunitY = Bs.Y();
		
		DLGTEMPLATE *Template = (DLGTEMPLATE*) d->Mem->Lock();
		if (Template)
		{
			GRect r = Pos;
			r.x1 = MulDiv(r.x1, 4, baseunitX);
			r.y1 = MulDiv(r.y1, 8, baseunitY);
			r.x2 = MulDiv(r.x2, 4, baseunitX);
			r.y2 = MulDiv(r.y2, 8, baseunitY);

			Template->style =	WS_VISIBLE |
								DS_ABSALIGN |
								WS_SYSMENU |
								WS_CAPTION |
								DS_SETFONT |
								DS_NOFAILCREATE |
								DS_3DLOOK;

			Template->style |= DS_MODALFRAME;
			Template->dwExtendedStyle = WS_EX_DLGMODALFRAME;
			Template->cdit = 0;
			Template->x = r.x1;
			Template->y = r.y1;
			Template->cx = r.X();
			Template->cy = r.Y();

			d->IsModal = false;

			short *A = (short*) (Template+1);
			// menu
			*A++ = 0;
			// class
			*A++ = 0;
			// title
			A = DlgStrCopy(A, NameW());
			// font
			*A++ = SysFont->PointSize(); // point size
			A = DlgStrCopy(A, SysFont->Face());
			A = DlgPadToDWord(A);

			GViewI *p = GetParent();
			while (	p &&
					!p->Handle() &&
					p->GetParent())
			{
				p = p->GetParent();
			}

			HWND hWindow = (p)?p->Handle():0;

			CreateDialogIndirectParam(	LgiProcessInst(),
										Template,
										hWindow,
										(DLGPROC) DlgRedir, 
										(LPARAM) this);
		}
	}

	#else

    GViewI *p = GetParent();
    if (p && p->GetWindow() != p)
        p = p->GetWindow();
    
    d->IsModal = false;
	if (Attach(0))
	{
	    AttachChildren();
	    
	    if (p)
	    {
	        GRect pp = p->GetPos();
	        int cx = pp.x1 + (pp.X() >> 1);
	        int cy = pp.y1 + (pp.Y() >> 1);
	        
	        GRect np = GetPos();
	        np.Offset(  cx - (np.X() >> 1) - np.x1,
	                    cy - (np.Y() >> 1) - np.y1);
	        
	        SetPos(np);
	        MoveOnScreen();
	    }
	    
	    Visible(true);
        Status = true;
	}
	else
	{
	    LgiAssert(!"Attach failed.");
	}

	#endif

	return Status;
}

GMessage::Result GDialog::OnEvent(GMessage *Msg)
{
	switch (Msg->m)
	{
		#if USE_DIALOGBOXINDIRECTPARAM
		case WM_INITDIALOG:
		#else
		case WM_CREATE:
		#endif
		{
			GRect r = Pos;
			Pos.ZOff(-1, -1);
			SetPos(r);	// resets the dialog to the correct
						// size when large fonts are used

			if (GetAlwaysOnTop())
				SetAlwaysOnTop(true);

			AttachChildren();
			if (!_Default)
				SetDefault(FindControl(IDOK));

			// This was commented out. I've re-introduced it until such time
			// as there is a good reason not to have it enabled. If such a reason
			// arises, update this comment to reflect that.
			OnCreate();

    		#if USE_DIALOGBOXINDIRECTPARAM
			GViewI *v = LgiApp->GetFocus();
			GWindow *w = v ? v->GetWindow() : 0;
			if (v && (w != v) && (w == this))
			{
				// Application has set the focus, don't set to default focus.
				return false;
			}
			else
			{
				// Set the default focus.
				return true;
			}
			#endif
			
			// If we don't return true here the GWindow::OnEvent handler for
			// WM_CREATE will call OnCreate again.
			return true;
		}
		/*
		case WM_CLOSE:
		{
			if (d->IsModal)
				EndModal(0);
			else
				EndModeless(0);
			return 0;
			break;
		}
		*/
	}

    return GWindow::OnEvent(Msg);
}

void GDialog::Quit(bool DontDelete)
{
	if (d->IsModal)
		EndModal(0);
	else
		GView::Quit(DontDelete);
}

void GDialog::OnPosChange()
{
    if (Children.Length() == 1)
    {
        List<GViewI>::I it = Children.Start();
        GLayout *t = dynamic_cast<GLayout*>((GViewI*)it.First());
        if (t)
        {
            GRect r = GetClient();
            r.Size(GTableLayout::CellSpacing, GTableLayout::CellSpacing);
            t->SetPos(r);
        }
    }
}

void GDialog::EndModal(int Code)
{
    #if USE_DIALOGBOXINDIRECTPARAM
	EndDialog(Handle(), Code);
	#else
	d->ModalResult = max(Code, 0);
	#endif
}

void GDialog::EndModeless(int Code)
{
	Quit(Code);
}

///////////////////////////////////////////////////////////////////////////////////////////
GControl::GControl(char *SubClassName) : GView(0)
{
	SubClass = 0;
	SetOnDelete = NULL;
	if (SubClassName)
	{
		SetClassW32(SubClassName);
		SubClass = GWin32Class::Create(SubClassName); // Owned by the GWin32Class object
	}
	Pos.ZOff(10, 10);
}

GControl::~GControl()
{
	if (SetOnDelete)
		*SetOnDelete = true;
}

GMessage::Result GControl::OnEvent(GMessage *Msg)
{
	GMessage::Result Status = 0;
	
	// Pre-OS event handler
	switch (Msg->m)
	{
		case WM_CREATE:
		{
			SetId(GetId());
			OnCreate();
			break;
		}
		case WM_SETTEXT:
		{
			if (IsWindowUnicode(_View))
				GBase::NameW((char16*)Msg->b);
			else
				GBase::Name((char*)Msg->b);
			break;
		}
		case WM_GETDLGCODE:
		case WM_NOTIFY:
		case WM_COMMAND:
		case WM_SYSKEYDOWN:
		case WM_SYSKEYUP:
		case WM_KEYDOWN:
		case WM_KEYUP:
		{
			bool Deleted = false;
			SetOnDelete = &Deleted;
			Status = GView::OnEvent(Msg);
			if (Deleted)
				return Status;
			
			SetOnDelete = NULL;
			break;
		}
		case WM_NCDESTROY:
		{
			Status = GView::OnEvent(Msg);
			break;
		}
	}
	
	// OS event handler
	if (SubClass)
		Status = SubClass->CallParent(Handle(), Msg->m, Msg->a, Msg->b);

	return Status;
}

GdcPt2 GControl::SizeOfStr(const char *Str)
{
	GdcPt2 Pt(0, 0);
	if (Str)
	{
		for (const char *s=Str; s && *s; )
		{
			const char *e = strchr(s, '\n');
			if (!e) e = s + strlen(s);

			GDisplayString ds(SysFont, (char*)s, e - s);

			Pt.y += ds.Y();
			Pt.x = max(Pt.x, ds.X());

			s = (*e=='\n') ? e + 1 : 0;
		}
	}

	return Pt;
}

