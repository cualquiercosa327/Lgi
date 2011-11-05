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
#include <COMMCTRL.H>

///////////////////////////////////////////////////////////////////////////////////////////
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

///////////////////////////////////////////////////////////////////////////////////////////
GDialog::GDialog()
	: ResObject(Res_Dialog)
{
	Name("Dialog");

	Mem = 0;
	WndFlags |= GWF_DIALOG;
	SetExStyle(GetExStyle() | WS_EX_CONTROLPARENT);
	IsModal = true;

	_Window = this;
}

GDialog::~GDialog()
{
	DeleteObj(Mem);
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
		#if _MSC_VER >= 1400
        SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)(GViewI*)NewWnd);
		#else
        SetWindowLong(hWnd, GWL_USERDATA, (LONG)(GViewI*)NewWnd);
		#endif
	}

	GViewI *Wnd = (GViewI*)
		#if _MSC_VER >= 1400
		GetWindowLongPtr(hWnd, GWLP_USERDATA);
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

int GDialog::DoModal(OsView ParentHnd)
{
	int Status = -1;

	IsModal = true;
	Mem = new GMem(16<<10);
	if (Mem)
	{
		DLGTEMPLATE *Template = (DLGTEMPLATE*) Mem->Lock();
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

		DeleteObj(Mem);
	}

	return Status;
}

static char *BaseStr = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

int GDialog::DoModeless()
{
	int Status = -1;

	LgiAssert(!_View);
	if (_View)
		return Status;

	Mem = new GMem(16<<10);
	IsModal = false;
	GRect OldPos = GetPos();
	if (Mem)
	{
		LONG DlgUnits = GetDialogBaseUnits();
		uint16 old_baseunitX = DlgUnits & 0xffff;
		uint16 old_baseunitY = DlgUnits >> 16;
		GDisplayString Bs(SysFont, BaseStr);
		int baseunitX = (Bs.X() / 26 + 1) / 2;
		int baseunitY = Bs.Y();
		
		DLGTEMPLATE *Template = (DLGTEMPLATE*) Mem->Lock();
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

			IsModal = false;

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
			
			#if 0
			if (_View)
			{
				GRect p = OldPos;
				double Rx = 1.0;
				double Ry = 1.0;
				RECT Rect;
				
				do 
				{
					Rect.left = OldPos.x1 * Rx;
					Rect.top = OldPos.y1 * Ry;
					Rect.right = OldPos.x2 * Rx;
					Rect.bottom = OldPos.y2 * Ry;

					BOOL b = MapDialogRect(_View, &Rect);
					if (!b)
						break;

					double rx = (double) (OldPos.x2 + 1) / Rect.right;
					double ry = (double) (OldPos.y2 + 1) / Rect.bottom;
					if (Rx == 1.0 && Ry == 1.0)
					{
						Rx = rx;
						Ry = ry;
					}
					else
					{
						Rx *= 1.0 + ((rx - 1.0) / 2);
						Ry *= 1.0 + ((ry - 1.0) / 2);
					}
				}
				while (	Rect.left != OldPos.x1 ||
						Rect.top != OldPos.y1 ||
						Rect.right - 1 != OldPos.x2 ||
						Rect.bottom - 1 != OldPos.y2);
					   
				int asd=0;
			}
			#endif
		}
	}

	return Status;
}

extern GButton *FindDefault(GView *w);

GMessage::Result GDialog::OnEvent(GMessage *Msg)
{
	if (Msg->Msg == 6)
	{
		int asd=0;
	}

	switch (Msg->Msg)
	{
		case WM_INITDIALOG:
		{
			//SetWindowLong(_View, GWL_STYLE, GetWindowLong(_View, GWL_STYLE) | WS_CLIPCHILDREN);

			GRect r = Pos;
			#ifdef SKIN_MAGIC
			GdcPt2 Border = GetWindowBorderSize();
			r.x2 += Border.x - LgiApp->GetMetric(LGI_MET_DECOR_X);
			r.y2 += Border.y - LgiApp->GetMetric(LGI_MET_DECOR_Y);
			#endif

			Pos.ZOff(-1, -1);
			SetPos(r);	// resets the dialog to the correct
						// size when large fonts are used

			AttachChildren();
			if (!_Default)
			{
				_Default = FindControl(IDOK);
			}

			OnCreate();

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
			break;
		}
		case WM_CLOSE:
		{
			if (IsModal)
			{
				EndModal(0);
			}
			else
			{
				EndModeless(0);
			}
			return 0;
			break;
		}
	}

	return GView::OnEvent(Msg);
}

void GDialog::OnPosChange()
{
}

void GDialog::EndModal(int Code)
{
	EndDialog(Handle(), Code);
}

void GDialog::EndModeless(int Code)
{
	Quit(Code);
}

///////////////////////////////////////////////////////////////////////////////////////////
GControl::GControl(char *SubClassName) : GView(0)
{
	SubClass = 0;
	if (SubClassName)
	{
		SetClassW32(SubClassName);
		SubClass = GWin32Class::Create(SubClassName); // Owned by the GWin32Class object
	}
	Pos.ZOff(10, 10);
}

GControl::~GControl()
{
}

GMessage::Result GControl::OnEvent(GMessage *Msg)
{
	int Status = 0;

	// Pre-OS event handler
	switch (Msg->Msg)
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
			{
				GBase::NameW((char16*)Msg->b);
			}
			else
			{
				GBase::Name((char*)Msg->b);
			}
			break;
		}
		case WM_GETDLGCODE:
		{
			return GView::OnEvent(Msg);
		}
		case WM_NOTIFY:
		case WM_COMMAND:
		case WM_KEYDOWN:
		case WM_KEYUP:
		case WM_SYSKEYDOWN:
		case WM_SYSKEYUP:
		{
			GView::OnEvent(Msg);
			break;
		}
	}
	
	// OS event handler
	if (SubClass)
	{
		Status = SubClass->CallParent(Handle(), Msg->Msg, Msg->a, Msg->b);
	}

	// Post-OS event handler
	switch (Msg->Msg)
	{
		case WM_CREATE:
		{
			SetFont(SysFont);
			break;
		}
	}

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

///////////////////////////////////////////////////////////////////////////////////////////
GItemContainer::GItemContainer()
{
	Flags = 0;
	ImageList = 0;
}

GItemContainer::~GItemContainer()
{
	if (OwnList())
	{
		DeleteObj(ImageList);
	}
	else
	{
		ImageList = 0;
	}
}

bool GItemContainer::SetImageList(GImageList *list, bool Own)
{
	ImageList = list;
	OwnList(Own);
	AskImage(true);
	return ImageList != NULL;
}

bool GItemContainer::LoadImageList(char *File, int x, int y)
{
	GSurface *pDC = LoadDC(File);
	if (pDC)
	{
		ImageList = new GImageList(x, y, pDC);
		if (ImageList)
		{
			#ifdef WIN32
			ImageList->Create(pDC->X(), pDC->Y(), pDC->GetBits());
			#endif
		}
	}
	return pDC != 0;
}
