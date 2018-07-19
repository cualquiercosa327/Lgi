/*hdr
**      FILE:           GuiDlg.cpp
**      AUTHOR:         Matthew Allen
**      DATE:           8/9/1998
**      DESCRIPTION:    Dialog components
**
**      Copyright (C) 1998 Matthew Allen
**              fret@memecode.com
*/

#include <stdlib.h>
#include <stdio.h>

#include "Lgi.h"
#include "GSlider.h"
#include "GBitmap.h"
#include "GTableLayout.h"
#include "GDisplayString.h"

using namespace Gtk;
#include "LgiWidget.h"

///////////////////////////////////////////////////////////////////////////////////////////
#define GreyBackground()

struct GDialogPriv
{
	int ModalStatus;
	bool IsModal;
	bool Resizable;
	
	GDialogPriv()
	{
		IsModal = true;
		Resizable = true;
		ModalStatus = 0;
	}
};

///////////////////////////////////////////////////////////////////////////////////////////
GDialog::GDialog()
	: ResObject(Res_Dialog)
	#ifdef __GTK_H__
	, GWindow(gtk_dialog_new())
	#endif
{
	d = new GDialogPriv();
	Name("Dialog");
	_View = GTK_WIDGET(Wnd);
	_SetDynamic(false);
}

GDialog::~GDialog()
{
	DeleteObj(d);
}

bool GDialog::IsModal()
{
	return d->IsModal;
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
        List<GViewI>::I it = Children.begin();
        GTableLayout *t = dynamic_cast<GTableLayout*>((GViewI*)it);
        if (t)
        {
            GRect r = GetClient();
            r.Size(GTableLayout::CellSpacing, GTableLayout::CellSpacing);
            t->SetPos(r);
        }
    }
}

bool GDialog::LoadFromResource(int Resource, char *TagList)
{
	GAutoString n;
	GRect p;

	bool Status = GLgiRes::LoadFromResource(Resource, this, &p, &n, TagList);
	if (Status)
	{
		Name(n);
		SetPos(p);
	}
	return Status;
}

bool GDialog::OnRequestClose(bool OsClose)
{
	if (d->IsModal)
	{
		EndModal(0);
		return false;
	}

	return true;
}

#define MWM_DECOR_ALL						(1L << 0)
#define MWM_HINTS_INPUT_MODE				(1L << 2)
#define MWM_INPUT_FULL_APPLICATION_MODAL	3L
#define XA_ATOM								((xcb_atom_t) 4)

class MotifWmHints
{
public:
	ulong Flags, Functions, Decorations;
	long InputMode;
	ulong Status;
	
	MotifWmHints()
	{
		Flags = Functions = Status = 0;
		Decorations = MWM_DECOR_ALL;
		InputMode = 0;
	}
};

bool GDialog::IsResizeable()
{
    return d->Resizable;
}

void GDialog::IsResizeable(bool r)
{
	d->Resizable = r;
}

gboolean GDialog::OnGtkEvent(GtkWidget *widget, GdkEvent *event)
{
	if (!event)
	{
		printf("%s:%i - No event %i\n", _FL);
		return FALSE;
	}

	switch (event->type)
	{
		case GDK_DELETE:
		{
			Quit();
			OnGtkDelete();
			return false;
		}
		case GDK_CONFIGURE:
		{
			GdkEventConfigure *c = (GdkEventConfigure*)event;
			Pos.Set(c->x, c->y, c->x+c->width-1, c->y+c->height-1);
			OnPosChange();
			return false;
			break;
		}
		case GDK_FOCUS_CHANGE:
		{
			GWindow::OnGtkEvent(widget, event);
			break;
		}
		case GDK_CLIENT_EVENT:
		{
			GMessage m(event);
			OnEvent(&m);
			break;
		}
		default:
		{
			printf("%s:%i - Unknown event %i\n", _FL, event->type);
			break;
		}
	}
	
	return true;
}

static
gboolean
GtkDialogDestroy(GtkWidget *widget, GDialog *This)
{
	This->_View = NULL;
	return 0;
}

static
void
GtkDialogRealize(GtkWidget *widget, GDialog *This)
{
	This->OnGtkRealize();
}
               
bool GDialog::SetupDialog(bool Modal)
{
	if (GBase::Name())
		gtk_window_set_title(GTK_WINDOW(Wnd), GBase::Name());

	gtk_dialog_set_has_separator(GTK_DIALOG(Wnd), false);
	if (IsResizeable())
	{
	    gtk_window_set_default_size(Wnd, Pos.X(), Pos.Y());
	}
	else
	{
	    gtk_widget_set_size_request(GTK_WIDGET(Wnd), Pos.X(), Pos.Y());
	    gtk_window_set_resizable(Wnd, FALSE);
	}
	
	GtkWidget *content_area = 	
	#if GtkVer(2, 14)
		gtk_dialog_get_content_area(GTK_DIALOG(Wnd));
	#else
		GTK_DIALOG(Wnd)->vbox;
	#endif
	if (content_area)
	{
		GtkContainer *container = GTK_CONTAINER(content_area);
		GtkHButtonBox *btns = NULL;
		
		Gtk::GList *list = gtk_container_get_children(container);
		for (Gtk::GList *i = list; i != NULL; i = i->next)
		{
			const gchar *type = G_OBJECT_TYPE_NAME(i->data);
			GtkWidget *w = GTK_WIDGET(i->data);
			if (!btns && GTK_IS_HBUTTON_BOX(i->data))
			{
				btns = GTK_HBUTTON_BOX(i->data);
			}
		}		
		g_list_free(list);
    
		// Add our own root control to contain LGI widgets
		if (_Root = lgi_widget_new(this, Pos.X(), Pos.Y(), true))
		{
			gtk_container_add(container, _Root);
			gtk_widget_show(_Root);
			if (btns)
			{
				// Hide the btns container, as Lgi won't use it.
				gtk_widget_hide(GTK_WIDGET(btns));
			}
		}
	}
	
	GView *gv = this;
	g_signal_connect(	G_OBJECT(Wnd),
						"delete_event",
						G_CALLBACK(GtkViewCallback),
						gv);
	g_signal_connect(	G_OBJECT(Wnd),
						"configure-event",
						G_CALLBACK(GtkViewCallback),
						gv);
	g_signal_connect(	G_OBJECT(Wnd),
						"destroy",
						G_CALLBACK(GtkDialogDestroy),
						this);
	g_signal_connect(	G_OBJECT(Wnd),
						"client-event",
						G_CALLBACK(GtkViewCallback),
						gv);
	g_signal_connect(	G_OBJECT(Wnd),
						"focus-in-event",
						G_CALLBACK(GtkViewCallback),
						gv);
	g_signal_connect(	G_OBJECT(Wnd),
						"focus-out-event",
						G_CALLBACK(GtkViewCallback),
						gv);
	g_signal_connect(	G_OBJECT(Wnd),
						"realize",
						G_CALLBACK(GtkDialogRealize),
						gv);

	gtk_widget_realize(GTK_WIDGET(Wnd));
	OnCreate();
	AttachChildren();

	if (!_Default)
	{
		_Default = FindControl(IDOK);
	}

	gtk_widget_show(GTK_WIDGET(Wnd));
	GView::Visible(true);

	return true;
}

int GDialog::DoModal(OsView OverrideParent)
{
	d->ModalStatus = -1;

	if (GetParent())
		gtk_window_set_transient_for(GTK_WINDOW(Wnd), GetParent()->WindowHandle());

	d->IsModal = true;
	SetupDialog(true);
	
	#if 0
	gtk_main();
	#else
	LgiApp->Run();
	#endif
	
	return d->ModalStatus;
}

void _Dump(GViewI *v, int Depth = 0)
{
	for (int i=0; i<Depth*4; i++)
		printf(" ");

	GViewIterator *it = v->IterateViews();
	if (it)
	{
		for (GViewI *c=it->First(); c; c=it->Next())
			_Dump(c, Depth+1);
		DeleteObj(it);
	}					
}

void GDialog::EndModal(int Code)
{
	if (d->IsModal)
	{
		d->IsModal = false;
		d->ModalStatus = Code;

		#if 0
		gtk_main_quit();
		#else
		LgiApp->Exit();
		#endif
	}
	else
	{
		// LgiAssert(0);
	}
}

int GDialog::DoModeless()
{
	d->IsModal = false;
	SetupDialog(false);
	return 0;
}

void GDialog::EndModeless(int Code)
{
	Quit(Code);
}

extern GButton *FindDefault(GView *w);

GMessage::Param GDialog::OnEvent(GMessage *Msg)
{
	switch (MsgCode(Msg))
	{
	}

	return GView::OnEvent(Msg);
}


///////////////////////////////////////////////////////////////////////////////////////////
GControl::GControl(OsView view) : GView(view)
{
	Pos.ZOff(10, 10);
}

GControl::~GControl()
{
}

GMessage::Param GControl::OnEvent(GMessage *Msg)
{
	switch (MsgCode(Msg))
	{
	}
	return 0;
}

GdcPt2 GControl::SizeOfStr(const char *Str)
{
	int y = SysFont->GetHeight();
	GdcPt2 Pt(0, 0);

	if (Str)
	{
		const char *e = 0;
		for (const char *s = Str; s && *s; s = e?e+1:0)
		{
			e = strchr(s, '\n');
			int Len = e ? (int)e-(int)s : strlen(s);

			GDisplayString ds(SysFont, s, Len);
			Pt.x = MAX(Pt.x, ds.X());
			Pt.y += y;
		}
	}

	return Pt;
}

//////////////////////////////////////////////////////////////////////////////////
// Slider control
GSlider::GSlider(int id, int x, int y, int cx, int cy, const char *name, bool vert) :
	ResObject(Res_Slider)
{
	SetId(id);
	GRect r(x, y, x+cx, y+cy);
	SetPos(r);
	Name(name);
	Vertical = vert;
	Min = Max = 0;
	Val = 0;
	SetTabStop(true);
}

GSlider::~GSlider()
{
}

void GSlider::Value(int64 i)
{
	if (i > Max) i = Max;
	if (i < Min) i = Min;
	
	if (i != Val)
	{
		Val = i;

		GViewI *n = GetNotify() ? GetNotify() : GetParent();
		if (n)
		{
			n->OnNotify(this, Val);
		}
		
		Invalidate();
	}
}

int64 GSlider::Value()
{
	return Val;
}

void GSlider::GetLimits(int64 &min, int64 &max)
{
	min = Min;
	max = Max;
}

void GSlider::SetLimits(int64 min, int64 max)
{
	Min = min;
	Max = max;
}

GMessage::Param GSlider::OnEvent(GMessage *Msg)
{
	return 0;
}

void GSlider::OnPaint(GSurface *pDC)
{
	pDC->Colour(LC_MED, 24);
	pDC->Rectangle();
	
	GRect r = GetClient();
	int y = r.Y() >> 1;
	r.y1 = y - 2;
	r.y2 = r.y1 + 3;
	r.x1 += 3;
	r.x2 -= 3;
	LgiWideBorder(pDC, r, DefaultSunkenEdge);
	
	if (Min <= Max)
	{
		int x = Val * r.X() / (Max-Min);
		Thumb.ZOff(5, 9);
		Thumb.Offset(r.x1 + x - 3, y - 5);
		GRect b = Thumb;
		LgiWideBorder(pDC, b, DefaultRaisedEdge);
		pDC->Rectangle(&b);		
	}
}

void GSlider::OnMouseClick(GMouse &m)
{
	Capture(m.Down());
	if (Thumb.Overlap(m.x, m.y))
	{
		Tx = m.x - Thumb.x1;
		Ty = m.y - Thumb.y1;
	}
}

void GSlider::OnMouseMove(GMouse &m)
{
	if (IsCapturing())
	{
		int Rx = X() - 6;
		if (Rx > 0 && Max >= Min)
		{
			int x = m.x - Tx;
			int v = x * (Max-Min) / Rx;
			Value(v);
		}
	}
}

class GSlider_Factory : public GViewFactory
{
	GView *NewView(const char *Class, GRect *Pos, const char *Text)
	{
		if (stricmp(Class, "GSlider") == 0)
		{
			return new GSlider(-1, 0, 0, 100, 20, 0, 0);
		}

		return 0;
	}

} GSliderFactory;

